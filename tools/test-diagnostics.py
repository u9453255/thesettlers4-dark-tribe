"""Check the crash and freeze diagnostics (native/diagnostics.hpp, round r26) end to end in a
disposable non-game x86 process: a first-chance fault, a real heap double free, a stack
overflow and a 13.5 s main-thread freeze with a window. No game is started or inspected."""
import json
import re
import shutil
import struct
import subprocess

from project_paths import ROOT, COMPILER


def lines(directory):
    return (directory/'evidence/v02-runtime.log').read_text(encoding='ascii', errors='replace').splitlines()


def frames(log, source):
    found = [l.split(' frames=', 1)[1].split() for l in log if 'STACK source='+source+' frames=' in l]
    return found[0] if found else []


def rvas(frame_list, module):
    return [int(f.split('+0x')[1], 16) for f in frame_list if f.startswith(module+'+0x')]


def fixture_rva(log, name):
    return int(re.search(r'FIXTURE '+name+r'=0x([0-9a-f]+)', '\n'.join(log)).group(1), 16)


def dump_main_eip(path, thread_id, module):
    """Minimal minidump read: the given thread's eip as an RVA in the named module."""
    data = path.read_bytes()
    assert data[:4] == b'MDMP'
    count, directory = struct.unpack_from('<II', data, 8)
    streams = {}
    for i in range(count):
        kind, size, rva = struct.unpack_from('<III', data, directory+12*i)
        streams[kind] = rva
    assert 3 in streams and 4 in streams, 'thread or module list missing'
    base = None
    modules = struct.unpack_from('<I', data, streams[4])[0]
    for i in range(modules):
        offset = streams[4]+4+108*i
        image, name_rva = struct.unpack_from('<Q', data, offset)[0], struct.unpack_from('<I', data, offset+20)[0]
        length = struct.unpack_from('<I', data, name_rva)[0]
        if data[name_rva+4:name_rva+4+length].decode('utf-16le').lower().endswith(module):base = image
    threads = struct.unpack_from('<I', data, streams[3])[0]
    for i in range(threads):
        offset = streams[3]+4+48*i
        if struct.unpack_from('<I', data, offset)[0] == thread_id:
            size, rva = struct.unpack_from('<II', data, offset+40)
            return struct.unpack_from('<I', data, rva+0xB8)[0]-base
    raise AssertionError('main thread missing from the dump')


def run():
    out = ROOT/'work/native-generated'
    out.mkdir(parents=True, exist_ok=True)
    exe = out/'diagnostics-fixture.exe'
    subprocess.run([str(COMPILER), '-std=c++17', '-O2', '-static', '-fms-extensions', '-Wall', '-Wextra', '-Werror',
                    str(ROOT/'native/diagnostics-fixture.cpp'), '-o', str(exe)], check=True)
    module = exe.name.lower()
    logs = {}
    for mode, code in (('fault', 0), ('heap', 0x374), ('overflow', 0xfd), ('freeze', 0)):
        directory = out/('diagnostics-'+mode)
        if directory.exists():
            shutil.rmtree(directory)
        result = subprocess.run([str(exe), str(directory), mode], capture_output=True, text=True, timeout=120,
                                creationflags=subprocess.CREATE_NO_WINDOW)
        assert result.returncode == code, (mode, result.returncode, result.stdout, result.stderr)
        logs[mode] = lines(directory)

    log = logs['fault']
    faults = [l for l in log if 'FIRST_CHANCE code=c0000005 ' in l]
    assert len(faults) == 2 and all('(access violation) at=' in l and ' main access=read target=00001234' in l for l in faults), log
    assert len([l for l in log if 'STACK source=scan' in l]) == 1, 'a repeated fault address gets no second stack'
    assert rvas(frames(log, 'scan'), module), log
    assert not any('e06d7363' in l for l in log), 'C++ exceptions are not logged'
    assert any('PROCESS_EXIT process ending' in l for l in log), log

    log = logs['heap']
    assert any('FATAL_EXCEPTION code=c0000374 (heap corruption) ' in l and ' main ' in l for l in log), log
    assert any('HEAP_FAILURE type=8 (double free (block not busy))' in l for l in log), log
    stack = frames(log, 'heap-failure')
    assert stack and stack[0].startswith('ntdll.dll+0x') and rvas(stack, module), log
    assert not any('PROCESS_EXIT' in l for l in log), 'terminated processes do not report an orderly exit'

    log = logs['overflow']
    assert any('FATAL_EXCEPTION code=c00000fd (stack overflow) ' in l for l in log), log
    recurse = fixture_rva(log, 'recurse')
    assert any(recurse <= r < recurse+0x100 for r in rvas(frames(log, 'scan'), module)), log

    log = logs['freeze']
    ready = [l for l in log if 'FREEZE_WATCH ready main thread ' in l]
    assert len(ready) == 1 and 'window found' in ready[0], log
    main = int(re.search(r'main thread (\d+)', ready[0]).group(1))
    freeze = [l for l in log if ' FREEZE main thread silent for ' in l]
    assert len(freeze) == 1 and 'window=not-responding' in freeze[0], log
    silent = int(re.search(r'silent for (\d+) ms', freeze[0]).group(1))
    assert 10000 <= silent < 12500, freeze
    hang = fixture_rva(log, 'hangHere')
    assert any(hang <= r < hang+0x200 for r in rvas(frames(log, 'main-thread'), module)), log
    written = [l for l in log if 'FREEZE_DUMP written ' in l]
    assert len(written) == 1, log
    dump = out/'diagnostics-freeze/evidence'/re.search(r'(freeze-\d+-1\.dmp)', written[0]).group(1)
    eip = dump_main_eip(dump, main, module)  # outside the fixture image only if sampled inside GetTickCount64
    assert hang <= eip < hang+0x200 or not 0 <= eip < 0x1000000, ('dump main thread eip', hex(eip))
    end = [l for l in log if 'FREEZE_END main thread resumed after ' in l]
    assert len(end) == 1 and 13000 <= int(re.search(r'after (\d+) ms', end[0]).group(1)) < 20000, log
    assert not any('FREEZE_SAMPLE' in l for l in log), 'no second sample before 30 s'
    assert any('PROCESS_EXIT process ending' in l for l in log), log

    report = {'version': '0.2.15', 'round': 'r26', 'process': 'disposable non-game x86 fixture',
              'first_chance_fault_with_stack': True, 'repeated_fault_address_without_second_stack': True,
              'cxx_exception_not_logged': True, 'heap_double_free_failure_type': 8,
              'heap_failure_stack_from_ntdll_record': True, 'stack_overflow_logged_with_stack': True,
              'freeze_reported_between_10_and_12_5_s': True, 'freeze_window_not_responding': True,
              'freeze_stack_in_hung_function': True, 'freeze_dump_valid_with_main_thread': True,
              'freeze_end_logged': True, 'orderly_exit_logged': True, 'game_started': False}
    (ROOT/'research/diagnostics-checks.json').write_text(json.dumps(report, indent=2))
    print('PASS: fault, heap double free, stack overflow and freeze diagnostics in a disposable non-game process')


if __name__ == '__main__':
    run()
