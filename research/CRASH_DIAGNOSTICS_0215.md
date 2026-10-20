# Crash and freeze diagnostics - round r26 (0.2.15)

2026-10-14. The user asked why the game crashed (game trace `Trace078.txt`) and then asked for better crash logging in the mod. This note records the crash analysis and the diagnostics added in r26. Everything below was checked offline from files (game traces, Windows Error Reporting dumps and reports, event logs, the pinned executable). No game was started, attached to or inspected.

## The three crashes of 2026-10-14

All three sessions ran the ordinary user map `Maps\User\3_1_0_1.map` (8 players, Conflict Mode, new game) with Settlers United, the HD patch, ExtraZoom, S4WarriorsLib and the community patch loaded. **The Dark Tribe plugin was not loaded in any of them.** Its DLL is absent from both dumps' module lists, and `evidence/v02-runtime.log` has no entries for these sessions.

| Time | Trace | Game folder | Evidence | Result |
|---|---|---|---|---|
| 00:40 | Trace074 | `C:\Projects\thesettlers4` | `%LOCALAPPDATA%\CrashDumps\S4_Main.exe.8404.dmp`, WER `AppCrash_S4_Main.exe_...cd09a8e3...`, event 1000 | `c0000374` heap corruption. The trace has 1,112 `Error in Map-Script! indexed expression not a table at line 1` lines from tick 5626 to the end. |
| 01:26 | Trace077 | Ubisoft install | `S4_Main.exe.30500.dmp`, WER `...d98cbbfa...`, event 1000 | `c0000374` heap corruption, with no script errors in the trace. |
| 01:55 | Trace078 | Ubisoft install | none | The trace stops in an autosave after the `FOG` section, before `SaveFileManager::saveLuaAsString`. |

### 00:40 and 01:26: double free in `lua_close` when leaving the map

Both dumps contain the same failure. Exception `c0000374` is raised at `ntdll+0xFD17F`. Its first parameter points to ntdll's heap failure record: version 2, size 0x3D0, **type 8, "block not busy", i.e. a double free**. The heaps are 0x00A60000 and 0x02620000; the blocks are 0x7AE7EB88 and 0x7C18EB88. ntdll recorded the same stack at detection in both dumps. RVAs are in the pinned `S4_Main.exe`:

```
ntdll RtlFreeHeap+0xCA
S4_Main.exe+0x9BB06A   CRT free (calls HeapFree through the IAT entry at 0xF150F0)
S4_Main.exe+0x27FDE8   function 0x27FDC0: frees the Lua string table (luaS_freeall)
S4_Main.exe+0x27984F   function 0x2797D0: lua_close (seen by stack scan; omitted by ntdll's frame walk)
S4_Main.exe+0x1BD4D4   function 0x1BD480: script cleanup, closes the Lua state at [this+0xC]
S4_Main.exe+0x1BD257   function 0x1BD220: script object destructor (vtable 0x106111C, delete 0x63C bytes)
S4_Main.exe+0xFE401    GameRun code that logs "==> CGameRun::Exit()...", virtual call [eax+8]
S4_Main.exe+0x11397B / 0x1034DB / 0x5CC05 / 0x952009   game loop, WinMain, CRT start (main thread)
```

How the functions were identified (disassembly of the pinned executable, capstone):
- **0x1BD220** sets the vtable, calls 0x1BD480, then deletes `this` (0x63C bytes) when bit 0 of its argument is set. This is a scalar deleting destructor.
- **0x1BD480** deletes the object at +0x10 through its virtual destructor. It then sets the global current Lua state `[0x146B078]` from +0xC, calls 0x2797D0, frees the 4-byte state holder and clears both fields.
- **0x2797D0** runs Lua 3.x's `lua_close` sequence: collect userdata, set GC threshold `[L+0x74]=0x7FFFFFFD`, run the tag methods, free tables, protos and closures, free userdata strings, then call 0x27FDC0 (free all strings), then free the stack at `[L+4]`.

So the game frees a block twice while **destroying the map's Lua script state on map exit** (quit to menu, restart or load). The dump shows where the second free was detected. It does not show which code freed the block first or corrupted the string table earlier. Candidates (hypotheses, not confirmed):
- a bug in the game's Lua runtime or map-script interface;
- a plugin that drives Lua: Settlers United's Lua patches (`PatchUseSpecificLuaScriptFromLuaCode`, `PatchLuaScriptSupportLuaEnding` were active) or S4ModApi Lua calls;
- the map script itself, through an engine bug.

### 01:55: no crash record

Searches between 01:50 and 01:56:37 found nothing:
- Windows Application and System event logs (no events at all from 01:53 to 01:58);
- Reliability Monitor;
- WER `ReportArchive`/`ReportQueue` and `%LOCALAPPDATA%\CrashDumps`;
- `%LOCALAPPDATA%\CrashRpt` (the HD patch's crash reporter) and both `.sentry-native` folders (Settlers United);
- the game folder, `Documents\TheSettlers4` and AppData.

The game's last trace line was written at 01:55:07. `Video.cfg` was written at 01:56:35 by the next launch. A crash that Windows sees leaves a WER record, so the most likely explanation is a hang inside the Lua save step, after which the game was closed. A Task Manager kill leaves no record. This is unconfirmed. It is consistent with the same damaged Lua state showing up as a hang.

## Why the plugin's log could not have helped

- The vectored handler only logged access violations, divide-by-zero and illegal instructions, so `c0000374` was ignored.
- It logged no stack, only the faulting address. For heap failures that address is always inside ntdll.
- Nothing detected hangs.
- It used `fopen` per line, which allocates and cannot work with a corrupted or locked heap.

## Diagnostics added (round r26)

Code: `native/diagnostics.hpp` (Windows part), `native/diagnostics-model.hpp` (pure logic, tested by `native/diagnostics-tests.cpp`). Connections in `dark-tribe.cpp`:
- `dtdiag::start` in the DLL start thread;
- `exceptionTrace` calls `dtdiag::exception`;
- `dtdiag::beat` in the tick, frame and map-init callbacks;
- `dtdiag::processExit` on DLL process detach.

All lines go to `evidence/v02-runtime.log` with the usual `<ms> pid= tick=` prefix.

| Line | Meaning |
|---|---|
| `Crash and freeze diagnostics ready: ...` | Startup. Says whether freeze dumps are available (PssCaptureSnapshot and dbghelp). |
| `FIRST_CHANCE code= address= gameRva= operation= (kind) at=module+0x.. thread= [main] [access= target=]` | First-chance fault, up to 32 lines per process. The prefix is unchanged. Covers access violation, divide-by-zero, illegal or privileged instruction, in-page error, array bounds, integer overflow and misalignment. The game, the HD patch's .NET runtime and others can handle these themselves. |
| `FATAL_EXCEPTION code= (kind) address= at= thread= [main] operation=` | Heap corruption, stack overflow, stack buffer overrun, fail-fast, assertion failure or out-of-memory. Up to 8 per process. |
| `HEAP_FAILURE type= (name) heap= block=` | For heap corruption: ntdll's failure record (type 8 = double free). |
| `STACK source=scan\|heap-failure\|main-thread frames=...` | Frames as `module+0xoffset`; S4_Main.exe offsets are RVAs. `heap-failure` is ntdll's stack recorded at detection. `scan` is a scan of the faulting thread's stack. A fault address already logged gets no second stack (up to 16 addresses). |
| `FREEZE_WATCH ready main thread <id> window found\|not found` | The watchdog knows the game's main thread (from the first tick) and its window. |
| `FREEZE main thread silent for N ms gameTick= window=not-responding\|responding\|unknown foreground\|background operation= eip=` | No tick or frame on the main thread for 10 s while the last one was in a map, and Windows reports the window as not responding. If no window was found, the game being in the foreground counts instead. Followed by `STACK source=main-thread`. |
| `FREEZE_SAMPLE ...` | Second sample of the same freeze after 30 s. Compare the two stacks: the same place means stuck, different places mean looping. |
| `FREEZE_DUMP written <path> bytes=` / `... unavailable\|failed error=` | `evidence/freeze-<pid>-<n>.dmp`, at most 2 per process. It is written at once if the window is not responding, otherwise at the second sample. |
| `FREEZE_END main thread resumed after N ms` | The stall ended, for example a long load. A real hang has no END line. |
| `PROCESS_EXIT process ending lastBeatMsAgo= inMap\|notInMap` | Normal exit (ExitProcess). Crashes, fail-fast and Task Manager kills leave no such line. |

Rules that keep it safe while the game is crashing or hung:
- **No heap, no loader lock** in the exception path and the watchdog. Lines are built with `dtdiag::Text` (no CRT formatting) and written with `WriteFile` to a handle opened at startup with `FILE_APPEND_DATA` and full sharing; `log()`'s per-line `fopen` still works beside it.
- **Memory reads:** `ReadProcessMemory` on the own process, after `VirtualQuery` refuses guard and no-access pages, in 1 KB blocks that never cross a page.
- **Module names** come from `PEB->Ldr->InLoadOrderModuleList`, read without the loader lock (bounded walk).
- **Static buffers** in the handler: a stack overflow leaves little stack. One thread logs at a time; a concurrent exception is skipped.
- **The watchdog suspends the main thread only** to read its registers and stack (syscalls and plain reads), then resumes it before formatting. The window lookup (`EnumThreadWindows` allocates) runs only while the game is responsive. During a freeze it only calls `IsWindow`, `IsHungAppWindow`, `GetForegroundWindow` and `GetWindowThreadProcessId`.
- **The dump** is a process snapshot: `PssCaptureSnapshot` with VA clone, threads, thread contexts and `PSS_CREATE_USE_VM_ALLOCATIONS`. dbghelp then writes it from the clone (`IsProcessSnapshotCallback` -> `S_FALSE`), so no game thread is suspended while dbghelp works. Type: data segments, unloaded modules, indirectly referenced memory, thread info. This is best effort: dbghelp uses the process heap, so if the hung thread holds the heap lock the dump may not finish. The FREEZE lines are written first.

Limits:
- **Not caught:** `__fastfail` (`c0000409` raised through `int 0x29`) goes straight to the kernel, and `TerminateProcess` runs no handlers. Only WER sees those; its dumps land in `%LOCALAPPDATA%\CrashDumps`.
- **Stack scans** can include stale return addresses from earlier calls.
- **Freeze heartbeats** are ticks and frames on the main thread. If S4ModApi does not deliver frame callbacks under the HD renderer, a paused game produces no beats. The window-responsiveness rule still prevents a report, because a paused game keeps processing messages.
- **False reports:** a long load or game exit that stops processing messages for more than 10 s while the last beat was in a map is reported, then ended by `FREEZE_END`.
- **Coverage:** only games started with `Launch Dark Tribe.cmd` get the DLL. The launcher helper injects it into every game it starts, whatever the map. Games started from Ubisoft Connect or the executable are not covered.

## Offline verification

- `diagnostics-tests` (built and run by `build-native.py`) covers:
  - text formatting and truncation;
  - every FF /2 call length and E8 rel32;
  - the stack scan (module bounds, frame and byte limits, unreadable memory, duplicate removal);
  - heap failure names and exception classes;
  - the watchdog state machine (menus, pauses, responsive windows, unknown windows, the second sample, recovery) and the dump policy.
- `tools/test-diagnostics.py` (workflow step after `build-native.py`) builds `native/diagnostics-fixture.cpp`, a disposable non-game x86 process, and checks four modes:
  - **fault:** two raised access violations log two `FIRST_CHANCE` lines with `access=read target=00001234` but only one stack, the C++ exception is ignored, and `PROCESS_EXIT` is logged.
  - **heap:** a real double free on a private heap logs `FATAL_EXCEPTION c0000374`, `HEAP_FAILURE type=8` and the ntdll-recorded stack into the fixture. This is the same record as the game's crashes.
  - **overflow:** unbounded recursion logs `FATAL_EXCEPTION c00000fd` with a stack in the recursive function.
  - **freeze:** a window, then a 13.5 s busy loop without message processing. The watchdog reports at 10.6 s with `window=not-responding`, the main-thread stack and `eip` inside the hung function, a valid 0.46 MB dump whose main-thread context is in that function, and `FREEZE_END` at 13.7 s.

  Results go to `research/diagnostics-checks.json`.
- `tools/read-crash-dump.py` reads WER and freeze dumps offline: the exception, the heap failure record and a stack scan with export names. It reproduces the chain above from `S4_Main.exe.30500.dmp`.

None of this is verified inside the game. TESTING.md r26 lists the user's checks.

## Round r51: the Ubisoft services crash about 35 s after the game starts (2026-10-21)

The user reported a crash on r50 (pid 27908, Iron Kingdom, tick 4496). Checked offline from the runtime log, the game traces and the pinned executable; no game was started or attached.

### Evidence

- `evidence/v02-runtime.log`: `FIRST_CHANCE code=c0000005 ... gameRva=00341676 ... thread=3140 access=write target=deadbaad`. Thread 3140 is not the main game thread (12512). The main thread kept ticking for about one second (tick 4510), then the process ended. No WER dump was written.
- `Trace113.txt` (the game's own trace, same clock): `Exception 0xc0000005 at 0x00e11676 ... Write to location 0xdeadbaad`, with the stack `sub_341660+22 <- sub_453C60+1021 <- sub_45FBA0+45 <- sub_45F100+27 <- sub_3B0A90+64 <- sub_3AFF90+956 <- sub_50FA80+18 <- sub_434160+185 <- sub_9ACD44+87 <- BaseThreadInitThunk`. The plugin's stack scan adds `0x4685d2` (inside `sub_468520`), which the trace's frame walk skips.
- The same crash happened twice before, with the same stack and the same `deadbaad` write: pid 28292 (r23 build, First Dawn, `Trace071.txt`) and pid 9960 (r44 build, Iron Kingdom, `Trace105.txt`). So it is not caused by r50 or by any one round.
- All three came 34.8 to 35.4 s after the process started (trace opened at 24747921, 49156968 and 116692250 ms; crashes at 24783359, 49191890 and 116727093). An autosave had finished 1.6 to 4.9 s before each crash, but about 1,800 autosaves in the user's traces were followed by no crash. The fixed delay after start points to a timer the services SDK starts at launch (most likely a request timeout racing its response); that part is a hypothesis.

### What fails (file-only disassembly, `work/native-analysis/ai/scripts/q.py`)

- `sub_434160` is the thread entry of the Ubisoft services SDK linked into S4_Main.exe (it asserts `param != nullptr` with the source path `.../ubiservices/client-sdk/private/ubiservices`). `sub_45F100`/`sub_45FBA0` run one job per update: a Ready (2) job is set to Running (3) through the job's state setter `sub_4CC550` and stepped by `sub_453C60`, which logs "Processing Job:" and repeats the step while the job stays Running (`0x45426D`). After it, the runner deletes a Complete (4) job and re-queues a Waiting (1) or Ready (2) one; anything else is "Job is in invalid state".
- `sub_468520` runs one step of the job's step sequence (at job + 0x20). When both `m_pStepMethod` (+0) and `m_pStepMethodEx` (+8) are null it asserts `m_pStepMethod != nullptr || m_pStepMethodEx != nullptr` ("Missing requirement"). The SDK has no log manager (dword `0x122A6A4` is 0) and no assert callback (dword `0x122A6CC`), so the assert handler `sub_341660` takes its fallback, `mov dword ptr [0xDEADBAAD], 0`: a deliberate crash. Without that assert the function would call the null step at `0x468669`.
- The job runner holds a reference to the job while it steps it, so the job object is alive; only its step sequence is empty. A job that is Ready while its sequence has no step points to a race inside the SDK (a job woken a second time after its steps ended). The job's class could not be read: no dump exists.
- The plugin has no part in this path. Its only code on other threads is the save import hooks (`CreateFileA/W`, `WriteFile`, `CloseHandle` of the EXE's import table, which the statically linked runtime and the SDK also use); they pass every non-`.sav` call through and keep no state for it.

### The guard (round r51)

`installUbiStepGuard` (dark-tribe.cpp, at startup after the carrier reserve hook) replaces the 33 bytes of the broken branch at `0x46854C`, which is reached only when both null checks fail, with:

```
mov ecx, [ebp+8]            ; the job (the runner's own argument)
mov eax, [ecx]
mov [ubiParkedVtable], eax  ; plugin global: the job's vtable
lock inc [ubiParked]        ; plugin global: how many jobs were parked
push 1                      ; Job::Waiting
call 0x4CC550               ; the job's own state setter (thiscall, ret 4); Running -> Waiting is legal (0x4F7F20)
jmp 0x46866B                ; the function's own epilogue (cookie check, pop esi, ret 4)
nop x4
```

The step loop then ends (the job is no longer Running) and the runner re-queues the job; if something wakes it again, it is parked again. The normal step path is unchanged. A parked job never finishes, so whatever online feature started it (single-player does not depend on one) gets no answer instead of the game dying. The tick logs `UBISERVICES_JOB_PARKED count= vtable= rva= class=` (the first time, then at most once a minute) with the job's RTTI class, so the next occurrence names the job.

Pins (fail closed, `Pinned Ubisoft services step guard signature mismatch; plugin disabled`, code 18): the function's prologue with the relocated security cookie, both null checks, the replaced branch with its relocated message, the epilogue, and the setter's prologue and state store; a page this module already patched passes. `tools/inspect-construction.py` checks these byte arrays against the file and against the image rebased to 0x400000, 0xAD0000 (the field crash's load address) and 0x60000000, disassembles the replacement (call target 0x4CC550, jump target 0x46866B), and asserts the state machine facts above (the assert string and its deliberate crash, no other branch into the replaced bytes, the setter's ABI and store, the legal Running -> Waiting transition, the step loop and the runner's re-queue).

Not verified in game. If the SDK later relies on the parked job finishing (for example a wait at game exit), the symptom would change from this crash to something else; `UBISERVICES_JOB_PARKED` in the log marks every case.
