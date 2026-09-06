# TempAR Developer Documentation

This is the developer-facing documentation for TempAR, a PSP cheat plugin (CWCheat + PSP Action
Replay code types). It's separate from `docs/`, which is the *end-user* documentation that gets
zipped into every release — nothing in this folder ships to users.

If you're new to the codebase, read these in order:

1. **[Architecture](ARCHITECTURE.md)** — module map, the plugin's execution model, how everything
   fits together.
2. **[Build System](BUILD.md)** — how to actually compile TempAR (Docker/PSPSDK), what the
   makefile targets do, full vs. lite builds.
3. **[Cheat Engine](CHEAT_ENGINE.md)** — the core: how cheats are stored, loaded, saved, and
   executed every frame; CWCheat/PSPAR code type reference; the favorites feature and the v1.70
   security hardening pass.
4. **[Menu System](MENU_SYSTEM.md)** — the UI layer: tabs, drawing pattern, controller input,
   config persistence, on-screen keyboard.
5. **[Disassembler, Search & Browser](DISASM_SEARCH_BROWSER.md)** — the MIPS disassembler, the
   memory search ("cheat finder"), and the file browser.
6. **[SDK Usage](SDK_USAGE.md)** — the PSP-specific and PSPSDK-specific knowledge you need that
   isn't obvious from the code: module lifecycle boilerplate, what each support module
   (kmalloc/syslibc/usb/screenshot/log/float/sdk) wraps, what `exports.exp`/`imports.S` are for.
7. **[Gotchas](GOTCHAS.md)** — every toolchain-compat landmine and subtle bug already hit, with
   root causes, sourced from git history. Read this before touching build flags, PSPSDK kernel
   headers, or anything that looks like a workaround.

## What TempAR is, in one paragraph

TempAR is a PSP kernel-mode plugin (`.prx`) loaded via the plugin loader (`seplugins`), heavily
descended from NitePR → MKUltra. It hooks into a running game/homebrew's memory space, presents a
debug-screen-style menu over the game's own framebuffer (triggered by a hotkey), and applies
"cheat codes" (CWCheat or PSP Action Replay format) to game memory once per loop iteration. It also
bundles a memory browser, a MIPS disassembler, and a memory search tool for finding new cheat
addresses — everything a cheat-device user or cheat-code author needs, in one plugin.

## Repo layout cheat-sheet

```
src/
  objects/        .c implementation files (one per module, compiled 1:1 to objects/*.o)
  include/        .h headers (one per module, plus common.h which pulls in ~everything)
  fonts/          precompiled bitmap font headers (only "acorn" is wired into the build)
  languages/      english.bin, compiled to english.h via bin2c at build time (see prep target)
  resources/      files that ship with the plugin (seplugins folder, PC tools)
  exports.exp     PRX export table (module_start/stop + the public cwcheat SDK API)
  imports.S       hand-written stub imports for SDK functions not in the normal import libs
  makefile_psp    build rules for the full build  (tempar.prx)
  makefile_lite   build rules for the lite build   (tempar_lite.prx)
  makefile_psppr / makefile_litepr  variants used by the "release_pr" makefile target
  notes.txt       the original author's list of build flags (_CWCHEAT_, _PSPAR_, etc.)
docs/             END-USER docs — README/CHANGELOG/LICENSE — packaged into every release zip
DEVELOPER/        this folder — developer docs, never packaged
makefile          top-level orchestration: docker wrapper around src/makefile_*
.github/workflows/build.yml   CI: builds both PRXs in the pspdev/pspdev Docker image, zips, releases
```

## Quick facts

- **Language:** C (MIPS target via PSPSDK's `psp-gcc`), plus hand-written MIPS assembly in
  `imports.S`.
- **Current version:** 1.70 (`src/include/version.h`; the release zip version, `RELVER` in the
  root `makefile`, is tracked separately — bump both, see [GOTCHAS.md](GOTCHAS.md)).
- **Two build variants:** `tempar.prx` (full) and `tempar_lite.prx` (fewer features, used for POPS
  compatibility / game-mode homebrew — see [BUILD.md](BUILD.md)).
- **No unit tests.** This is kernel-mode homebrew that pokes at another process's memory; testing
  is manual, on real hardware or PSP emulators, per feature.
- **Single external dependency:** PSPSDK itself, provided pre-built inside the `pspdev/pspdev`
  Docker image used for CI and local builds.
- **~12,000 lines of C**, dominated by `menu.c` (3,698 lines) and `cheat.c` (2,406 lines).
