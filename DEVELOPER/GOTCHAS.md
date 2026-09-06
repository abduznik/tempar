# Gotchas & Pitfalls

A single consolidated reference of everything that has already bitten this codebase, or is
structurally likely to. Organized by category. Cross-referenced from the other docs — this is the
"read before you touch X" checklist.

## Build / toolchain compatibility (sourced from git history)

TempAR is 2010-era C, built with whatever the `pspdev/pspdev` / `pspdev/pspsdk` Docker image
currently resolves to — there's no pinned toolchain version (see
[SDK_USAGE.md](SDK_USAGE.md#docker-as-the-toolchain)). Every entry below was a real break caused by
the Docker image picking up a newer PSPSDK and/or GCC 14, not a code regression in this repo.

### The `SceLoadCoreExecFileInfo` saga

This one is worth knowing in detail because it took **six commits** to land properly and is a good
example of the kind of PSPSDK-version whack-a-mole this project deals with:

- **Root cause:** `pspmodulemgr_kernel.h` (pulled in transitively by `pspkernel.h`, which
  `menu.c`/`screenshot.c`/`utils.c` all need) references a type `SceLoadCoreExecFileInfo` that, in
  newer PSPSDK, is only defined in `psploadcore.h` — a header not everyone includes first, and one
  that itself has inclusion-order requirements.
- **Attempts before the fix stuck (`cbdb547` → `6a08954` → `6a284d7` → `375dd4b` → `ee19b37` →
  `fa55883`):** the first attempts scattered a local stub typedef (`typedef struct { int dummy; }
  SceLoadCoreExecFileInfo;` or `typedef struct {} SceLoadCoreExecFileInfo;`) directly at the top of
  individual `.c` files (`menu.c`, `screenshot.c`, `utils.c`) — copy-pasted per file, worded
  slightly differently each time, sometimes as a *forward declaration* and sometimes as a *stub
  struct*, and at one point flip-flopping on whether to `#include <psploadcore.h>` directly instead.
- **Final, correct fix (`fa55883`):** a single shared header,
  [`src/include/psp_compat.h`](../src/include/psp_compat.h), providing one canonical
  `typedef struct { int _dummy; } SceLoadCoreExecFileInfo;`, documented with a comment explaining
  exactly why and instructing "include this header BEFORE any PSPSDK kernel headers", included once
  from each `.c` file that needs it (`kmalloc.c` includes it too, per the file itself).

**Lesson:** if you hit a "PSPSDK header references an undefined type" error again, **don't
re-invent a local stub in the `.c` file with the error** — check `psp_compat.h` first, and if the
fix belongs there, add it there once rather than scattering copies. This is exactly the mistake the
project made (and fixed) before landing on the shared-header approach.

### Other compat-fix commits

| Commit | What broke | Fix |
|---|---|---|
| `93093a7` | `float.h` used a `u32` type without pulling in the header that defines it (worked previously because something else transitively included it first — a newer/different PSPSDK header graph stopped doing so). | Added `#include <pspkerneltypes.h>` directly to `float.h` rather than relying on transitive inclusion. |
| `9162de8` | CI referenced the wrong Docker image name / had a stale `RELVER`. | Corrected the image name and `RELVER` in `.github/workflows/build.yml` and the root `makefile`. |
| `a06066e` | Newer pspdev image's SDK started providing its *own* declaration of `sceKernelGetGameInfo` (previously TempAR declared it itself in `utils.h` since older SDKs didn't), causing a duplicate/conflicting declaration; separately, `button_callback`'s signature didn't exactly match the SDK's callback typedef on the newer SDK. | Removed TempAR's own `sceKernelGetGameInfo` declaration (let the SDK provide it) and changed `button_callback`'s parameters to `u32` to match the SDK typedef exactly. |
| `4594821` | `cheat_insert` had an incompatible-pointer-type assignment the older GCC accepted silently but a newer GCC/PSPSDK type-checks more strictly. | Replaced an address-of assignment with an explicit `memcpy`. |
| `b3e25d1` | `f_cvt` (float conversion, `float.c`/`float.h`) had `u32` vs. `unsigned int` mismatches across `float.h`, `float.c`, and a call site in `menu.c` — these are the same bit width but distinct types to a stricter compiler. | Aligned the types across all three call sites. |
| `eaf4897` | `sceKernelGetThreadmanIdList` call in `menu.c` passed an array in a way that triggered a pointer-type warning/error, plus an unused-variable warning. | Fixed the array-pointer usage and removed the unused variable. |
| `15f2570` | A `f_cvt` call site in `menu.c` passed `search.results` (a `u32[]`) somewhere expecting a differently-typed pointer. | Explicit cast to `u32*`. |
| `14fcb6f` | `sceImposeHomeButton`/`scePowerGetResumeCount` were removed from the *headers* of the latest PSPSDK (but are still present/linkable at runtime on real hardware/CFW) — calling them without a declaration is an error under newer GCC's default strictness. | Added `-Wno-implicit-function-declaration` to both makefiles' `CFLAGS`, preserving the runtime call while suppressing the missing-declaration compile error. **This is a real, permanent, load-bearing flag** — not just warning cleanup — because the alternative would be hand-writing stub declarations (or `imports.S` stubs, see [SDK_USAGE.md](SDK_USAGE.md#exportsexp-and-importss)) for functions the SDK insists don't exist but the OS still exports. |
| `5376dc4` | GCC 14 tightened default behavior around implicit int-to-pointer conversions, pointer-type mismatches, and int conversions — all over legacy code that has always relied on C's historically loose defaults in this area. | Added `-Wno-incompatible-pointer-types -Wno-int-conversion -Wno-int-to-pointer-cast` to both makefiles. Like the flag above, this isn't cosmetic — without it the current toolchain fails to compile large parts of this codebase as originally written. |
| `b7d5215` | `atoi` isn't linked into the PSP kernel-mode libc build (`USE_KERNEL_LIBC=1`) even though it compiles without complaint — resulting in a link-time failure, not a compile-time one, which is easy to misdiagnose as an unrelated issue. | Replaced with `strtoul`, which *is* available. |

**Working theory for future compat breaks:** if a Docker image update breaks the build, look for (1)
a PSPSDK header now requiring a type/include it didn't before (fix: extend `psp_compat.h`, don't
scatter local stubs), (2) a function removed from headers but still runtime-linkable
(fix: `-Wno-implicit-function-declaration`, already set, or an `imports.S` stub if linking also
fails), or (3) GCC being stricter about a type mismatch that's always technically been there
(fix: narrow, targeted `-Wno-*`, or actually fix the type — the project has done both depending on
how invasive the real fix would be).

## Security hardening (v1.70, commit `21dac59`)

A dedicated pass fixed ten unchecked-length / unchecked-address issues typical of code from before
this bug class was taken seriously in hobbyist C. Full detail in
[CHEAT_ENGINE.md](CHEAT_ENGINE.md#security-hardening-v170-commit-21dac59); the pattern to
internalize for **any new code**:

- Null-terminate explicitly after every fixed-size buffer copy (`strncpy` does not guarantee this).
- Validate address + length against `cfg.address_start`/`cfg.address_end` before any memory write
  derived from file or user input (`memory_copy`, patch files, PSPAR `0x0E` code type all had this
  bug).
- Use `snprintf`, never `sprintf`.
- If you add a `Config` field or otherwise trust a value loaded from a file, add it to
  `config_validate()`'s clamping rather than assuming the file is well-formed — `config.bin` and
  friends are not cryptographically signed and can be hand-edited or corrupted.

## Global state and lifecycle traps

- **No RAII / manual cleanup only.** `menu_show()`'s input-masking/framebuffer-swap/game-pause
  setup must be torn down in the exact reverse order on every exit path. A new early-return added
  deep inside a `layout_*` call tree that skips the teardown tail leaves the game's input
  permanently masked. See [MENU_SYSTEM.md](MENU_SYSTEM.md#architecture-a-blocking-state-machine-over-a-text-console).
- **No config migration.** Changing `Config`'s field set without bumping `CONFIG_VER` causes old
  config files to be *misread* (garbage values through `config_validate()`'s clamps) rather than
  cleanly rejected. Changing it *with* a bump just resets users to defaults — there's no field-level
  migration, by design, at this project's scale. See
  [MENU_SYSTEM.md](MENU_SYSTEM.md#config-persistence-configc).
- **No dirty-tracking/autosave.** Any new piece of persistent per-cheat state (like `CHEAT_FAVORITE`)
  needs an explicit `cheat_save()` call at the point it's mutated, or the change is silently lost on
  next load. This was an actual shipped bug, fixed in `8080309` — see
  [CHEAT_ENGINE.md](CHEAT_ENGINE.md#favorites-added-v165-hardened-v170).
- **`ctrl_read()` force-sets CIRCLE when the menu is closed.** Any code reading raw controller state
  outside a menu-visible context will always see CIRCLE held. See
  [MENU_SYSTEM.md](MENU_SYSTEM.md#input-handling-ctrlc).
- **Static OBJS lists.** Adding a new `.c` file requires manually adding its `.o` to *every*
  relevant makefile (`makefile_psp`, `makefile_lite`, and the PR variants) — there's no glob. See
  [BUILD.md](BUILD.md#src-makefile_psp-and-src-makefile_lite).
- **`printf`/`puts` are macros**, not the libc functions, in any file that includes `menu.h` or
  `disasm.h` (`#define printf(...) pspDebugScreenKprintf(__VA_ARGS__)`) — they draw to the
  debug-screen framebuffer at the current cursor position, not to any log/stdout. Cursor position
  management (`pspDebugScreenSetXY`) is load-bearing for correct output ordering.
- **`MENU_KEY` is a hidden bitmask.** `get_menu_ctrl()`'s inline `#define MENU_KEY ...` in `menu.c`
  silently excludes any button not already listed there. A new hotkey added elsewhere in the UI
  won't be seen by the top-level tab debounce loop unless this macro is also updated.
- **Double hotkey registration.** `sceCtrlRegisterButtonCallback` is called at boot and re-called in
  three places in the options screen when keys are rebound. A fourth rebinding UI needs a fourth
  re-registration, or it'll dispatch on stale bindings.

## Vendored code

Two files are vendored from other open-source PSP homebrew projects and carry their own license
headers/conventions — treat changes to these more conservatively than the rest of the codebase:

- `disasm.c`/`disasm.h` — from PSPLINK (2006, BSD), MIPS disassembler. See
  [DISASM_SEARCH_BROWSER.md](DISASM_SEARCH_BROWSER.md#mips-disassembler-disasmc).
- `pspdebugkb.c`/`pspdebugkb.h` — PSPSDK's own on-screen-keyboard sample utility. See
  [MENU_SYSTEM.md](MENU_SYSTEM.md#on-screen-keyboard-pspdebugkbc).

## Versioning is manual and duplicated

The release version number exists in at least two places that must be kept in sync by hand: `RELVER`
in the root `makefile`, and `VER_MAIN`/`VER_SUB` (and the display string `VER_STR`) in
[`src/include/version.h`](../src/include/version.h). There is no single source of truth and no
build-time check that they match — a release with mismatched values will build fine and just show
the wrong version string somewhere.
