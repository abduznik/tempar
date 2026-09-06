# TempAR

[![Build & Release PRX](https://github.com/abduznik/tempar/actions/workflows/build.yml/badge.svg)](https://github.com/abduznik/tempar/actions/workflows/build.yml)
[![Latest release](https://img.shields.io/github/v/release/abduznik/tempar)](https://github.com/abduznik/tempar/releases/latest)
[![Platform](https://img.shields.io/badge/platform-PSP-blue)](https://en.wikipedia.org/wiki/PlayStation_Portable)

**TempAR** is a **cheat engine plugin for the PlayStation Portable (PSP)**, supporting both
**CWCheat** and **PSP Action Replay (PSPAR)** code types in a single plugin. It runs on **custom
firmware (CFW)** as a kernel-mode `.prx` plugin loaded via `seplugins`, and includes an in-game
menu, a live **memory search / cheat finder**, a **MIPS disassembler**, and a **memory
browser/editor** — everything needed to both *use* and *create* PSP cheat codes on real hardware.

It is a heavily modified descendant of MKUltra, which is itself a modified descendant of NitePR —
carrying forward PSP cheat-device history back to the CWCheat/PSPAR era while adding features
neither predecessor had (favorites, extended PSPAR code types, PSX GameShark support for POPS
titles, multi-language UI, and more — see the [changelog](docs/CHANGELOG.md)).

## Features

- **Dual cheat engine**: CWCheat *and* PSP Action Replay (PSPAR + PSPAR Extended) code types, both
  loadable from the same cheat database.
- **PSX GameShark support** for PS1-on-PSP (POPS) titles.
- **In-game cheat menu**: toggle, edit, rename, copy, favorite, and reorder cheats without leaving
  your game.
- **Memory search ("cheat finder")**: 8/16/32-bit exact and unknown-value search, with results
  addable directly as new cheats.
- **MIPS disassembler & memory browser/decoder**: inspect and patch live game memory on-device.
- **NitePR / PSPAR.com `.bin` compatibility**: load existing NitePR `.txt` and PSPAR `.bin` cheat
  files without conversion.
- **Homebrew & PS1 (POPS) support**, with a lightweight `tempar_lite.prx` build for memory-
  constrained modes.
- **Skinnable UI** and **multi-language** support.

See the [full user guide](docs/README.md) for hotkeys, cheat file formats, and a detailed
comparison against PSPAR, CWCheat, and NitePR/MKUltra.

## Installation

Copy the `seplugins` folder from a [release build](https://github.com/abduznik/tempar/releases) to
your PSP's Memory Stick, then enable the plugin for your desired mode (`game.txt`/`pops.txt`) —
see the [plugin readme](docs/README.md#usage) for full installation steps and hotkey reference.

## Building from source

TempAR builds against the [PSPSDK](https://github.com/pspdev/pspsdk) MIPS toolchain via Docker —
no local PSP toolchain install required. From the repo root:

```powershell
docker run -it -v "${PWD}:/src" -w "/src" pspdev/pspsdk make release
```

This produces both `tempar.prx` (full) and `tempar_lite.prx` (lite) builds, packaged as a `.zip`
in a `build` folder. Build process tested on Windows 10.

For build internals, PSPSDK/toolchain compatibility notes, and codebase architecture, see the
[developer documentation](DEVELOPER/README.md).

## Documentation

| | |
|---|---|
| [User guide](docs/README.md) | Installation, hotkeys, cheat file formats, skinning, FAQ |
| [Changelog](docs/CHANGELOG.md) | Version history |
| [Developer docs](DEVELOPER/README.md) | Architecture, build system, cheat engine internals, PSP SDK usage, known gotchas |
| [License](docs/LICENSE) | License information |

## Related PSP cheat formats

TempAR interoperates with codes written for [CWCheat](https://github.com/raing3/psp-cheat-documentation/blob/master/cheat-devices/cwcheat.md),
[PSP Action Replay (PSPAR)](https://github.com/raing3/psp-cheat-documentation/blob/master/cheat-devices/pspar.md),
and NitePR — see the [code types reference](docs/README.md#code-types) for details.