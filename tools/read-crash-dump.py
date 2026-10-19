"""Read a 32-bit S4_Main.exe crash or freeze dump offline (WinDbg not needed).

  python tools/read-crash-dump.py <file.dmp> [--thread <id>] [--frames N]
  python tools/read-crash-dump.py --latest

--latest takes the newest S4_Main.exe dump from %LOCALAPPDATA%\\CrashDumps (Windows Error
Reporting) or evidence/freeze-*.dmp (the plugin's freeze watchdog, round r26).
Prints the exception, ntdll's heap failure record for heap corruption, and a stack for the
exception thread (freeze dumps: the given thread, else the first). Frames are module+offset
(S4_Main.exe offsets are RVAs of the pinned build), with the nearest export for system
DLLs read from the files on disk. The stack is a scan for return addresses after call
instructions, so stale entries can appear. Reads files only; no process is touched.
"""
import argparse
import bisect
import os
import struct
import sys
from pathlib import Path

from project_paths import ROOT, PYTHON_LIBS

sys.path.insert(0, str(PYTHON_LIBS))
import pefile  # noqa: E402

HEAP_FAILURES = ['internal', 'unknown', 'generic', 'entry corruption', 'multiple entries corruption',
                 'virtual block corruption', 'buffer overrun', 'buffer underrun', 'double free (block not busy)',
                 'invalid argument', 'use after free', 'cross-heap operation', 'free lists corruption',
                 'list entry corruption', 'LFH bitmap mismatch', 'segment LFH bitmap corruption',
                 'segment LFH double free', 'VS subsegment corruption', 'null heap', 'allocation limit',
                 'commit limit', 'invalid allocation type']
CODES = {0xC0000374: 'heap corruption', 0xC00000FD: 'stack overflow', 0xC0000409: 'stack buffer overrun',
         0xC0000005: 'access violation', 0xC0000602: 'fail fast', 0xC0000420: 'assertion failure',
         0xC0000017: 'out of memory', 0x80000003: 'breakpoint'}


class Dump:
    def __init__(self, path):
        self.b = Path(path).read_bytes()
        signature, _, count, directory = struct.unpack_from('<4sIII', self.b, 0)
        if signature != b'MDMP':
            raise SystemExit('not a minidump: %s' % path)
        self.streams = {}
        for i in range(count):
            kind, size, rva = struct.unpack_from('<III', self.b, directory + 12 * i)
            self.streams.setdefault(kind, (size, rva))
        self.arch = struct.unpack_from('<H', self.b, self.streams[7][1])[0] if 7 in self.streams else None
        self.modules, self.ranges, self.threads, self.exception = [], [], [], None
        if 4 in self.streams:
            rva = self.streams[4][1]
            for i in range(self.u32(rva)):
                offset = rva + 4 + 108 * i
                base, size, _, _, name = struct.unpack_from('<QIIII', self.b, offset)
                length = self.u32(name)
                self.modules.append((base, size, self.b[name + 4:name + 4 + length].decode('utf-16le', 'replace')))
            self.modules.sort()
        if 9 in self.streams:
            rva = self.streams[9][1]
            count, data = struct.unpack_from('<QQ', self.b, rva)
            for i in range(count):
                start, size = struct.unpack_from('<QQ', self.b, rva + 16 + 16 * i)
                self.ranges.append((start, size, data))
                data += size
        if 5 in self.streams:
            rva = self.streams[5][1]
            for i in range(self.u32(rva)):
                start, size, data = struct.unpack_from('<QII', self.b, rva + 4 + 16 * i)
                self.ranges.append((start, size, data))
        self.ranges.sort()
        self.starts = [r[0] for r in self.ranges]
        if 3 in self.streams:
            rva = self.streams[3][1]
            for i in range(self.u32(rva)):
                tid, _, _, _, teb, stack, stack_size, _, context_size, context = struct.unpack_from('<IIIIQQIIII', self.b, rva + 4 + 48 * i)
                self.threads.append(dict(tid=tid, teb=teb, stack=stack, stack_size=stack_size, context=(context_size, context)))
        if 6 in self.streams:
            rva = self.streams[6][1]
            tid = self.u32(rva)
            code, flags, _, address, parameters = struct.unpack_from('<IIQQI', self.b, rva + 8)
            values = struct.unpack_from('<15Q', self.b, rva + 40)[:parameters]
            context = struct.unpack_from('<II', self.b, rva + 160)
            self.exception = dict(tid=tid, code=code, flags=flags, address=address, parameters=values, context=context)

    def u32(self, offset):
        return struct.unpack_from('<I', self.b, offset)[0]

    def read(self, address, size):
        i = bisect.bisect_right(self.starts, address) - 1
        if i < 0:
            return None
        start, length, data = self.ranges[i]
        if address + size > start + length:
            return None
        return self.b[data + address - start:data + address - start + size]

    def module(self, address):
        for base, size, name in self.modules:
            if base <= address < base + size:
                return base, name
        return None


class Symbols:
    """Module+offset names; system DLL code bytes and exports come from the files on disk."""

    def __init__(self, dump):
        self.dump, self.files = dump, {}

    def pe(self, base, name):
        if base not in self.files:
            path = name
            if path.lower().startswith('c:\\windows\\system32\\'):
                path = 'C:\\Windows\\SysWOW64\\' + path[20:]  # 32-bit process view
            pe = None
            if os.path.exists(path):
                try:
                    pe = pefile.PE(path, fast_load=True)
                    pe.parse_data_directories(directories=[0])
                    table = getattr(pe, 'DIRECTORY_ENTRY_EXPORT', None)
                    pe.exports = sorted((s.address, s.name.decode()) for s in table.symbols if s.name) if table else []
                    pe.addresses = [e[0] for e in pe.exports]
                except pefile.PEFormatError:
                    pe = None
            self.files[base] = pe
        return self.files[base]

    def code(self, address, size):
        found = self.dump.module(address)
        if not found:
            return None
        pe = self.pe(*found)
        if pe is None:
            return self.dump.read(address, size)
        try:
            return pe.get_data(address - found[0], size)
        except pefile.PEFormatError:
            return None

    @staticmethod
    def indirect_call_length(modrm, sib):
        """Length of an FF /2 call (same rules as dtdiag::indirectCallLength)."""
        mod, rm = modrm >> 6, modrm & 7
        if mod == 3:
            return 2
        if mod == 0:
            if rm == 5:
                return 6
            return (7 if sib & 7 == 5 else 3) if rm == 4 else 2
        if mod == 1:
            return 4 if rm == 4 else 3
        return 7 if rm == 4 else 6

    def after_call(self, address):
        b = self.code(address - 7, 7)
        if not b or len(b) < 7:
            return False
        if b[2] == 0xE8:
            return True
        for k in range(2, 8):
            at = 7 - k
            if b[at] == 0xFF and (b[at + 1] >> 3) & 7 == 2 and self.indirect_call_length(b[at + 1], b[at + 2] if at + 2 < 7 else 0) == k:
                return True
        return False

    def name(self, address):
        found = self.dump.module(address)
        if not found:
            return '%08x' % address
        base, path = found
        module, offset = path.split('\\')[-1], address - base
        text = '%s+0x%x' % (module, offset)
        pe = self.pe(base, path)
        if pe is not None and pe.exports and not module.lower().startswith('s4_main'):
            i = bisect.bisect_right(pe.addresses, offset) - 1
            if i >= 0 and offset - pe.exports[i][0] < 0x4000:
                text += ' (%s+0x%x)' % (pe.exports[i][1], offset - pe.exports[i][0])
        return text


def stack(dump, symbols, thread, limit):
    size, rva = thread['context']
    context = dump.b[rva:rva + size]
    eip, esp = struct.unpack_from('<I', context, 0xB8)[0], struct.unpack_from('<I', context, 0xC4)[0]
    frames = [eip]
    top = thread['stack'] + thread['stack_size']
    address = max(esp, thread['stack'])
    while address + 4 <= top and len(frames) < limit:
        raw = dump.read(address, 4)
        value = struct.unpack('<I', raw)[0] if raw else 0
        if value and dump.module(value) and symbols.after_call(value) and frames[-1] != value:
            frames.append(value)
        address += 4
    return frames


def latest():
    candidates = list((ROOT / 'evidence').glob('freeze-*.dmp'))
    crash_dumps = Path(os.environ.get('LOCALAPPDATA', '')) / 'CrashDumps'
    if crash_dumps.is_dir():
        candidates += list(crash_dumps.glob('S4_Main.exe.*.dmp'))
    if not candidates:
        raise SystemExit('no S4_Main.exe or freeze dumps found')
    return max(candidates, key=lambda p: p.stat().st_mtime)


def main():
    parser = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    parser.add_argument('dump', nargs='?')
    parser.add_argument('--latest', action='store_true')
    parser.add_argument('--thread', type=int)
    parser.add_argument('--frames', type=int, default=40)
    args = parser.parse_args()
    path = latest() if args.latest or not args.dump else Path(args.dump)
    dump = Dump(path)
    symbols = Symbols(dump)
    print('dump:', path)
    if dump.arch != 0:
        print('note: not an x86 dump (architecture %s); stacks below assume x86' % dump.arch)
    main_module = next((name for _, _, name in dump.modules if name.lower().endswith('s4_main.exe')), None)
    print('game:', main_module or 'S4_Main.exe not in module list', '| threads:', len(dump.threads), '| modules:', len(dump.modules))
    print('non-system modules:')
    for base, size, name in dump.modules:
        if '\\windows\\' not in name.lower():
            print('  %08x %s' % (base, name))
    e = dump.exception
    if e:
        print('exception: code=%08x (%s) flags=%x at %s thread=%d' % (e['code'], CODES.get(e['code'], 'exception'), e['flags'], symbols.name(e['address']), e['tid']))
        if e['code'] == 0xC0000374 and e['parameters']:
            raw = dump.read(e['parameters'][0], 0x38 + 32 * 4)
            if raw:
                version, size, kind, heap, block = struct.unpack_from('<5I', raw)
                print('heap failure: type=%d (%s) heap=%08x block=%08x (record version %d, size 0x%x)' % (kind, HEAP_FAILURES[kind] if kind < len(HEAP_FAILURES) else 'unrecognised', heap, block, version, size))
                print('stack recorded by ntdll at detection:')
                for value in struct.unpack_from('<32I', raw, 0x38):
                    if not dump.module(value):
                        break
                    print('   ', symbols.name(value))
    else:
        print('exception: none (snapshot dump, e.g. a freeze)')
    wanted = args.thread or (e['tid'] if e else None)
    thread = next((t for t in dump.threads if t['tid'] == wanted), dump.threads[0] if dump.threads else None)
    if thread is None:
        raise SystemExit('no threads in the dump')
    if e and thread['tid'] == e['tid'] and e['context'][0]:
        thread = dict(thread, context=e['context'])  # registers at the exception
    print('stack scan of thread %d:' % thread['tid'])
    for value in stack(dump, symbols, thread, args.frames):
        print('   ', symbols.name(value))


if __name__ == '__main__':
    main()
