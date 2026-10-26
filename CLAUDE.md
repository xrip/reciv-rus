# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

reciv runs Sid Meier's Civilization (DOS 474.05) natively on Windows and Linux with SDL3. The goal is the original game, **bugs included**, in a window. Do not "fix" original game behaviour unless asked.

- `src/game` is the game logic from [civd](https://github.com/sjmelia/civd), a reverse-engineered reconstruction of the MSC 5.1 source. `src/game/CIVD_COMMIT` records the upstream commit. Every reciv change in these files is marked with a `reciv:` comment.
- `src/platform` replaces DOS, the BIOS, the resident assembly (`RESIDENT.ASM` and its `.INC` files) and the drivers (`MGRAPHIC.EXE`, `MISC.EXE`, the AdLib driver) with C over SDL3.
- `src/crt` is the Microsoft C 5.1 run-time, with MSC-visible behaviour (for example `rand`) reproduced exactly.
- The original game data is not in the repo. Running needs a real Civilization folder.

## Build and test

Linux (GCC 14+ is required for `_BitInt`; CMake, Ninja, SDL3):

    cmake -S . -B build -G Ninja
    ninja -C build
    ctest --test-dir build --output-on-failure
    ctest --test-dir build -R keyboard --output-on-failure   # one test
    ./build/reciv -d /path/to/CIVILIZATION -s /path/to/saves [--skip-quiz]

Windows, MSVC (from PowerShell; the script downloads and checks SDL3):

    .\tools\build-msvc.ps1                       # VS 2026; add -VisualStudio 2022 -BuildDir build-msvc-2022 for VS 2022
    ctest --test-dir build-msvc -C Release --output-on-failure

- MSVC is opt-in: plain CMake needs `-DRECIV_MSVC=ON`. Only GCC 14+ or MSVC is accepted.
- `tools/build-windows.sh` cross-builds a Windows package with MinGW-w64 GCC 14+.
- `-DRECIV_ASAN=ON` adds AddressSanitizer (Linux GCC only).
- `RECIV_DEBUG=1` (glibc only): SIGUSR1 prints a backtrace, useful for a hung game.
- Test names: `graphics-layers`, `dos-files`, `city-disaster`, `keyboard`, `battle-strength`, `sound-timer`, `asound-sequence`, `asound-voice`. The four tests `city-disaster`, `keyboard`, `battle-strength`, `sound-timer` link the real game code and replace some functions with GNU `ld --wrap` (`__wrap_*` in the test). They are not built under MSVC.
- There is no lint or format tool. CI (`.github/workflows`) builds and tests on Linux (GCC 14) and Windows (MSVC). Pushing a `v*` tag publishes the MSVC zip as a GitHub release.

## Architecture: two kinds of C

The main idea of the build (`CMakeLists.txt`) is that there are two kinds of code with different type sizes.

**Period code** is the `period` object library: every `src/game/*.C` (globbed, so new files are picked up), plus `src/platform/resident_names.c`, `stubs.c` and `layout_check.c`.
- `src/period/prelude.h` is force-included. It does `#define int short` and makes `long` 32 bits (`_BitInt(32)` on GCC), and it removes `far`/`near`/`huge`. `main` becomes `game_main`.
- It sees only `src/period/include` (the MSC 5.1 headers), never host headers (`-nostdinc`). `src/period/crtnames.h` maps each C library name (`strcpy`, `rand`, `int86`, ...) to `crt_*` in `src/crt/crt.c`. If the game needs a new library function, add the `#define` there and the function in `crt.c`.
- Flags: `-fwrapv` (MSC arithmetic wraps), `-fsigned-char`, `-fpermissive`, warnings off. MSVC builds it with `/Od`, so wrapping is not optimized away.
- `layout_check.c` uses `_Static_assert` to keep record sizes (`struct unit` 12, `struct city` 28, ...) as MSC made them. Saved games copy these records byte by byte.

**Host code** is everything else in `src/platform` and `src/crt`: ordinary modern C with warnings on. Values cross the edge as `int16_t` (game `int`) and `int32_t` (game `long`). Host declarations of game functions and variables must use these types, not `int`/`long`.

Prelude macros for places where the original code depended on undefined behaviour or register values:
- `RECIV_UB(value, site)`: an explicit value for a legacy UB site.
- `RECIV_SI(value)`: sets `reciv_si`, the SI register a callee reads as a missing argument.
- `RECIV_STACK(value, site)`: a value shared across some legacy calls.

## Architecture: the emulated DOS machine (`src/platform`)

- **Memory** (`mem.c`, `reciv.h`): `reciv_mem` is the 1 MB real-mode address space. `REAL(seg, off)` gives a pointer into it. VGA is at `A000:0000`. DOS blocks, pages, sprites and loaded driver files live there at real segment numbers, so the game's segment arithmetic works. Game variables are ordinary C variables. `FP_SEG` of a native pointer gives a fake segment from `C000h` up (`reciv_seg_wrap`/`reciv_seg_ptr`).
- **Timer** (`timer.c`): there are no interrupts. `reciv_poll` runs the 70 Hz ticks that are due by the real clock (palette fades, colour cycles, shake, sound), handles window events and shows the screen. **Every routine that waits for time or input must call `reciv_poll`/`reciv_wait`/`reciv_spin`**, or the game hangs.
- **Drivers**: the game calls driver entry points by slot number. `resident.c` turns on the built-in C routine for a slot when the game loads the matching driver. `reciv_require_slot` stops the program if a slot is used before its driver is loaded. `mgraphic.c` is `MGRAPHIC.ASM` ported routine by routine. It still reads the palette and fonts from the loaded driver file in `reciv_mem`.
- **Sound**: `sound.c` sends AdLib register writes to Nuked-OPL3 (`third_party/nuked-opl3`) and then to an SDL audio stream. `asound-*.c` are the AdLib driver's (`ASOUND.ASM`) sequencer, voice and command code, kept separate so they can be tested alone.
- **Video** (`video.c`, `filters.c`): shows `A000` through the DAC in an SDL window at 4:3. Filters (`hq4x` from `third_party/hqx`, `sharp`, `nearest`, `bilinear`) change only what is shown. They are host code in a separate library, kept away from period settings.
- **Files** (`files.c`): DOS names are matched without case. Reads look in the save directory, then the game directory. Writes go to the save directory.
- **Keyboard** (`keyboard.c`): keys are taken by physical position (US layout) and turned into BIOS scan<<8|ascii codes.
- `stubs.c` holds platform routines not written yet. Each one stops with its name, so running the game shows what is needed next.
- `resident_names.c` defines variables that `RESIDENT.ASM`/`F0DATA.ASM` gave only by DGROUP address. Its header says it is generated by `tools/gen-resident-names.py`, but that script is not in this repo.

## Conventions

- `src/game` files have upper-case names and **CRLF line endings in the repo** (`.gitattributes`). Keep CRLF when you edit them. Other source files are LF.
- Mark each change to `src/game` with a `/* reciv: ... */` comment that gives the reason, often the original instruction sequence (for example `SHL/NOT AX, then CWD`).
- Comments often name the original source: assembly labels, `.INC` files, DGROUP offsets, driver slots. Keep these references when porting more code.
- `RESIDENT.H` holds declarations whose owning module is not recovered yet. When that module is recovered, move the declaration to its header.
