# Architecture

## The big picture

TempAR is a single PSP kernel-mode PRX module. There is no multi-process architecture, no IPC, no
separate "backend" — it's one `.prx` file loaded into the address space of whatever game/homebrew
is running, via the PSP's plugin system (`seplugins`). Everything described below happens inside
that one module.

```
module_start()                       [src/objects/main.c]
   │  records the plugin's own boot path, snapshots the current thread list
   └─ spawns "TempAR_thread" → main_thread()
                                         │
                                         │  one-time init (in order):
                                         │   config_load()      – settings from config.bin
                                         │   gameid_get()       – identify the running game
                                         │   cheat_init()       – allocate cheat/block pools
                                         │   cheat_load()       – load cheats for this game id
                                         │   language_init/load – UI strings
                                         │   psid_init()        – (optional) PSID corruption
                                         │   screenshot_init()  – (optional)
                                         │   search_init()      – resume any in-progress search
                                         │   menu_init()        – zero UI state
                                         │   sceCtrlRegisterButtonCallback(button_callback)
                                         │
                                         └─ while(running):
                                               if menu.visible      → menu_show()      (blocking UI)
                                               else if cheat_hz set → cheat_apply(0)    (apply cheats)
                                               screenshot handling
                                               sceKernelDelayThread(cheat_hz or 15ms)
```

`module_stop()` tears this down: flips `running = 0`, waits (with a timeout + forced terminate) for
the thread to exit, then frees cheat/language/text-viewer memory and disconnects USB.

This is the entire lifecycle. There's no event loop framework, no scheduler beyond the PSP's own
kernel threads, and no persistent background service beyond this one thread.

## Module map

| Module | Files | Responsibility |
|---|---|---|
| Entry point | `main.c` / `main.h` | `module_start`/`module_stop`, the main thread loop, global hotkey dispatch (`button_callback`), game-thread pause/resume |
| Cheat engine | `cheat.c` / `cheat.h` | Cheat/Block data model, loading cheats from `.db`/`.bin`/`.txt`/NitePR files, saving, and *executing* CWCheat/PSPAR code types against memory every loop tick |
| Menu / UI | `menu.c` / `menu.h` | All on-screen menus: cheat list, searcher, options, browser/decoder, credits. Owns the tab state machine |
| Input | `ctrl.c` / `ctrl.h` | Controller polling, CROSS/CIRCLE swap, analog→dpad synthesis, key-repeat, blocking wait helpers |
| Config | `config.c` / `config.h` | Loads/saves `config.bin` (raw packed struct + checksum) and `colors/colorN.txt` skin files |
| Search | `search.c` / `search.h` | Memory search ("cheat finder") — exact/unknown/range search across 8/16/32-bit values, backed by files on the memory stick so searches persist across sessions |
| Disassembler | `disasm.c` / `disasm.h` / `disasm_prxtool.h` | MIPS instruction decoder, vendored from PSPLINK — feeds the in-menu "Decoder" screen |
| File browser | `filebrowser.c` / `filebrowser.h` | Lists/navigates memory stick directories for loading text/patch/cheat files |
| File buffering | `filebuffer.c` / `filebuffer.h` | Buffered file I/O helpers (`fileIoOpen/Read/Write/...`) used everywhere instead of raw `sceIo*` |
| On-screen keyboard | `pspdebugkb.c` / `pspdebugkb.h` | Vendored PSPSDK utility for text entry (renaming cheats, search text) without the system OSK |
| Text/guide viewer | `text.c` / `text.h` | Pages through an arbitrary text file for the in-menu game guide viewer |
| Language | `language.c` / `language.h` | Loads a binary string table so the UI can be shown in multiple languages |
| PSID | `psid.c` / `psid.h` | Deliberately corrupts the PSP's PSID for network anti-cheat evasion (see [GOTCHAS.md](GOTCHAS.md)) |
| Screenshot | `screenshot.c` / `screenshot.h` | Captures the framebuffer to a file on a hotkey |
| USB | `usb.c` / `usb.h` | Toggles USB mass-storage mode so a PC can read the memory stick without rebooting |
| kmalloc | `kmalloc.c` / `kmalloc.h` | Kernel partition-memory allocator wrapper (`sceKernelAllocPartitionMemory` + alignment) |
| syslibc | `syslibc.c` / `syslibc.h` | Minimal libc-shaped shims for functions the kernel-mode libc doesn't provide |
| float | `float.c` / `float.h` | Float/fixed-point helpers used by a couple of PSPAR-extended code types |
| utils | `utils.c` / `utils.h` | Grab-bag: game ID resolution, misc string/memory helpers |
| log | `log.c` / `log.h` | Debug logging to a file (`log()` macro/function), compiled out in release unless enabled |
| SDK shim | `sdk.c` / `sdk.h` | Small exported API surface for third-party PRX plugins to add cheats/read config programmatically (see exports) |

`common.h` is the "include everything" header — nearly every `.c` file includes only `common.h`
plus whatever PSPSDK headers it needs directly.

## Two binaries, one source tree

The build produces two PRX files from the *same* source files, differentiated entirely by
preprocessor defines set in the makefile (see [BUILD.md](BUILD.md)):

- **`tempar.prx`** — full build: CWCheat + PSPAR + USB + PSID + screenshots + guide viewer + UMD
  dump + module list + thread list + disassembler + auto-off + multi-language.
- **`tempar_lite.prx`** — drops USB, PSID, UMD dump, module list, thread list (smaller memory
  footprint); used for POPS (PS1-on-PSP) and other memory-constrained game modes.

Because both binaries share every `.c` file, most feature code is wrapped in `#ifdef _FEATURE_`
guards rather than living in separate files — grep for the flag names in `src/notes.txt` when you
need to find where a feature is gated.

## Data flow: how a cheat gets from a text file to game memory

1. **Load.** `cheat_load()` (`cheat.c`) is called once at boot with the resolved game ID. It tries,
   in order: the plugin's own `cheat.db` (indexed multi-game database), then per-game `.txt`
   CWCheat/PSPAR files, then a PSPAR `.bin` file, then a NitePR `.txt` file (converted to PSPAR
   format on load). Parsed cheats become `Cheat` structs (name + flags + pointer range into a
   shared `Block` array) — see [CHEAT_ENGINE.md](CHEAT_ENGINE.md) for the exact struct layout.
2. **Toggle.** The user opens the menu (`menu_show()`), navigates the Cheater tab
   (`layout_cheats()`), and presses CROSS/SQUARE to flip `CHEAT_SELECTED`/`CHEAT_CONSTANT` bits on
   a `Cheat`'s flags. This does not touch memory yet — it only changes state.
3. **Apply.** Every iteration of the main loop (when the menu isn't open and `cheat_hz` isn't 0),
   `cheat_apply(0)` walks the whole cheat list and, for every cheat that's currently enabled,
   dispatches to `cheat_apply_cwcheat()`, `cheat_apply_pspar()`, or `cheat_apply_psx_gs()`
   depending on the cheat's engine flag and whether the loaded title is a POPS (PS1) title. These
   functions interpret the code-type byte of each `Block` and read/write game memory directly via
   `address_load()`/`address_set()`.
4. **Persist.** `cheat_save()` writes the current in-memory cheat list back out to
   `cheats/{game-id}.db` so new cheats, edits, and favorite/order changes survive a reboot.

## Why so much global state

This is homebrew for a single-core, no-MMU-protection, cooperative-ish embedded target with 32-64MB
of RAM. There is exactly one "session" (one game running, one plugin instance), so there was never
a reason to avoid module-level globals (`cheats`, `blocks`, `cfg`, `menu`, `search`, `game_id`,
etc.) — they *are* the application state, shared via `extern` across files. If you're coming from
application/server development, expect this pattern everywhere rather than passed-around context
objects. See [GOTCHAS.md](GOTCHAS.md) for the specific ways this bites contributors.
