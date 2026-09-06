# PSP SDK Usage

This covers PSPSDK/PSP-homebrew-specific knowledge: the module lifecycle every PRX needs, what each
small support module wraps and why, and the PRX export/import mechanism. For the toolchain-compat
*bugs already hit* (with root causes), see [GOTCHAS.md](GOTCHAS.md).

## Module lifecycle: `module_start` / `module_stop`

Every PSP kernel module (PRX) needs two exported entry points, declared via
`PSP_MODULE_INFO`/`PSP_MAIN_THREAD_ATTR` macros and listed in `src/exports.exp`
(see [Exports and imports](#exportsexp-and-importss) below). TempAR's are in
[`src/objects/main.c`](../src/objects/main.c):

- **`module_start(argc, argv)`** — called once when the plugin loader loads the PRX. TempAR does
  the *minimum* here and defers everything else to a worker thread:
  1. Parses `argv[0]` (its own load path) to extract the plugin's directory (`plug_path`) and drive
     (`plug_drive`, e.g. `ms0:`) by hand-scanning for `/` characters — there's no path-splitting
     helper in the kernel libc.
  2. Records the boot game's path via `sceKernelInitFileName()`.
  3. Snapshots the current thread list (`sceKernelGetThreadmanIdList`) into `thread_buf_start` —
     this baseline is what `gamePause()`/`gameResume()` later diff against to know which threads
     belong to the *game* (suspend those) vs. threads that existed before TempAR loaded (leave
     those alone).
  4. Creates and starts `"TempAR_thread"` running `main_thread()`, then returns.

  **Why defer to a thread instead of doing init in `module_start` directly?** `module_start` runs
  very early in the boot sequence, before other system modules the plugin depends on
  (`sceKernelLibrary` in particular) have necessarily finished loading. `main_thread()`'s first act
  is a polling wait: `while(!sceKernelFindModuleByName("sceKernelLibrary")) sceKernelDelayThread(...)`.
  Returning from `module_start` quickly and doing real init later, on a separate thread, is the
  standard PSP plugin pattern for this reason — don't move initialization back into `module_start`
  without understanding why it was moved out.

- **`module_stop(argc, argv)`** — called when the plugin is unloaded (game exit / PSP shutdown).
  Sets `running = 0` and `menu.visible = 0`, waits up to 100ms for the worker thread to notice and
  exit cleanly (`sceKernelWaitThreadEnd`), and force-terminates it if it doesn't
  (`sceKernelTerminateDeleteThread`) — a bounded wait with a forced fallback, not a `join()` with no
  escape hatch. Then frees cheat memory, language memory, the text-viewer buffer, and disconnects
  USB if it was active. **If you add a new subsystem with its own allocated memory or open
  handle/connection, add its teardown here** — there's no automatic resource tracking.

## `button_callback` and the main loop

`button_callback(curr_but, last_but, arg)` is registered via `sceCtrlRegisterButtonCallback` and is
the global hotkey dispatcher — see [MENU_SYSTEM.md](MENU_SYSTEM.md#input-handling-ctrlc) for the
full detail. The steady-state loop in `main_thread()` is intentionally simple:

```c
while(running) {
    if(menu.visible)            menu_show();       // blocking, returns when user closes menu
    else if(cfg.cheat_hz != 0)  cheat_apply(0);     // apply all enabled cheats once
    // screenshot handling...
    sceKernelDelayThread(cfg.cheat_hz ? cfg.cheat_hz : 15000);
}
```

`cfg.cheat_hz` is a user-configurable delay (in microseconds) between cheat-apply passes — lower
means cheats reapply more often (useful against game code that keeps overwriting the same address)
at the cost of more CPU time stolen from the game.

## Support modules: what each one wraps

| Module | Wraps / solves |
|---|---|
| **`kmalloc.c`** | `sceKernelAllocPartitionMemory` wrapped with alignment support and automatic partition selection. `choose_alloc()` checks partitions `1` and `6` in order via `sceKernelQueryMemoryPartitionInfo`/`sceKernelPartitionMaxFreeMemSize` and picks the first with enough free space, so allocation doesn't hardcode a single kernel memory partition (which may already be full depending on what else is resident). The allocation stores its own `SceUID` just before the aligned pointer it returns, so `kfree()` can look it up (`*(u32*)(mem_addr - 4)`) without the caller tracking it separately. |
| **`syslibc.c`** | Fills gaps in the **kernel-mode** libc (`USE_KERNEL_LIBC=1` in the makefiles pulls in a smaller libc than user-mode PRXs get). Implements `vsnprintf`/`snprintf` on top of PSPSDK's internal `prnt()` formatter, and `strcasecmp` — neither is guaranteed present in the kernel libc. |
| **`log.c`** | A minimal file-based debug logger (`_log()` → `ms0:/tempar_log.txt`), entirely compiled out unless `_DEBUG_` is defined (it is **not** in any current makefile — logging is dead weight in shipped builds, opt-in only for local debugging). There's also a dead/commented `_log_psplink()` path for logging over a PSPLink cable instead of a file. |
| **`usb.c`** | Toggles USB mass-storage mode (`sceUsbActivate`/`Start`/`Stop`/`Deactivate` + `sceUsbstorBootSetCapacity`) so a PC can read the memory stick without the user rebooting into recovery mode. Gated by `_USB_` (full build only — not compatible with POPS per `src/notes.txt`). |
| **`screenshot.c`** | Captures the current framebuffer to a file on the screenshot hotkey. Gated by `_SCREENSHOT_`. |
| **`float.c`** | Float/fixed-point conversion helpers (`f_cvt` and friends) used by a handful of PSPAR-extended code types that operate on floating-point game values. This is the file behind the `u32` vs. `unsigned int` type-mismatch fix in git history — see [GOTCHAS.md](GOTCHAS.md). |
| **`sdk.c`** | The public API surface exposed to *other* PRX plugins via `exports.exp`'s `cwcheat` export group — lets a third-party plugin register cheats, read/save config, or trigger a cheat-apply pass programmatically without linking against TempAR's internals. See below. |
| **`psid.c`** | Deliberately corrupts the console's OpenPSID (via `sceOpenPSIDGetOpenPSID` + a write-back) — a 2010-era technique for evading PSN/network-level anti-cheat/ban systems tied to the console's unique ID. Only runs for non-POPS titles (`sceKernelInitKeyConfig() == PSP_INIT_KEYCONFIG_GAME`), gated by `_PSID_`. |

## `exports.exp` and `imports.S`

**`exports.exp`** declares what this PRX exposes to the outside world — the kernel module loader
and, potentially, other plugins. It has two export groups:

- **`syslib`** (mandatory for every PRX): `module_start`, `module_stop`, `module_info`. Every kernel
  module must export exactly these under this name for the loader to recognize it.
- **`cwcheat`**: TempAR's own small public API (`add_cheat_pspcheat_prx`, `add_codeline_pspcheat_prx`,
  `read_config`, `config_saver`, `setcodes`, `readdb` — all implemented in `sdk.c`). This exists so
  a separate PRX (a game-specific trainer plugin, for example) can add cheats or trigger a
  cheat-apply pass against TempAR's already-loaded, already-running instance, instead of
  reimplementing cheat storage/execution itself. If you rename or change the signature of any
  function in this group, you break binary compatibility for any third-party plugin built against
  it — treat this list as a public API, not an internal one.

**`imports.S`** is hand-written MIPS assembly declaring *stub imports* — functions TempAR calls
that live in system modules (`SysMemForKernel`, `sceImpose_driver`, `sceUsb`, `sceUsbstorBoot`,
`sceRtc`, `sceUtility`, `sceOpenPSID_driver`) but aren't exposed through PSPSDK's normal
`libpsp*`/`.h`+import-library pairing — usually because they're kernel-only, undocumented, or from
a PSPSDK version too old/new to have a matching stub already generated. Each `STUB_START`/`STUB_FUNC`
pair binds a function name to a library NID (name) and a function NID (hash) so the linker can
resolve the call without PSPSDK needing to know about it at the C level. If a future PSPSDK version
starts providing a proper header+stub for one of these (as eventually happened with
`sceKernelGetGameInfo` — see `a06066e` in [GOTCHAS.md](GOTCHAS.md)), the hand-written stub in
`imports.S` becomes a duplicate symbol and must be removed, not just left alongside the SDK-provided
one.

## Docker as the toolchain

TempAR doesn't assume a locally-installed PSPSDK. Every build (local via the documented Docker
command, and CI) runs inside a container image (`pspdev/pspsdk` or `pspdev/pspdev`) that bundles
the full MIPS cross-compiler + PSPSDK headers/libs pre-built. This means **the toolchain version is
whatever the Docker image tag currently resolves to** — there's no version pin in this repo. When
the image updates upstream (newer GCC, newer PSPSDK headers), the build can break without any
change to this repository's own code, purely from a `docker pull` picking up a new image. Every
compat-fix commit in [GOTCHAS.md](GOTCHAS.md) was triggered exactly this way. If you're debugging a
build failure that "used to work," check whether the Docker image changed before assuming the
source changed.
