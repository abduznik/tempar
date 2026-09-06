# Build System

## TL;DR

```powershell
docker run -it -v "${PWD}:/src" -w "/src" pspdev/pspsdk make release
```

That's it for a full local build — Docker supplies the entire PSP MIPS toolchain and PSPSDK so you
don't install anything on the host. Output lands in `build/tempar-<RELVER>.zip`.

CI (`.github/workflows/build.yml`) does the same thing but against the `pspdev/pspdev` image (a
superset/newer image than `pspdev/pspsdk` — see [GOTCHAS.md](GOTCHAS.md) for why the image
name matters and has broken before).

## The three makefiles

There's a top-level `makefile` that orchestrates everything, plus per-binary makefiles in `src/`
that PSPSDK's own build system (`build.mak`) consumes.

### `makefile` (repo root)

```makefile
export RELVER := 1.70

release:    prep psp lite pack clean
release_pr: prep psppr litepr pack clean
```

Targets:

| Target | What it does |
|---|---|
| `prep` | Compiles `src/languages/english.bin` into a C header (`english.h`) via `bin2c`, so language strings can be `#include`d as data rather than loaded from a separate file at first boot. |
| `psp` | `make -C src -f makefile_psp` — builds the **full** `tempar.prx`. |
| `lite` | `make -C src -f makefile_lite` — builds the **lite** `tempar_lite.prx`. |
| `psppr` / `litepr` | Same, but using `makefile_psppr`/`makefile_litepr` — variants used by the `release_pr` target (PR/preview builds; check those makefiles for what differs, typically a different `-D` flag set or version suffix). |
| `pack` | Creates `build/temp`, copies `src/resources/*` (the `seplugins` folder structure users install) and `docs/` (end-user docs) into it, moves the built `.prx` files into `build/temp/seplugins/TempAR`, then zips the whole thing to `build/tempar-$(RELVER).zip`. |
| `clean` | Removes build artifacts: `.elf`/`.prx` outputs, `objects/*.o`, generated language headers, `build/temp`. |

`release` runs `prep`, builds both `psp` and `lite`, packs, then cleans. **`RELVER` must be bumped
by hand** in this file for every release — it's also duplicated in
`src/include/version.h` (`VER_MAIN`/`VER_SUB`) and must be kept in sync manually (there is no
single source of truth for the version number — see [GOTCHAS.md](GOTCHAS.md)).

### `src/makefile_psp` and `src/makefile_lite`

These are real PSPSDK makefiles (they `include $(PSPSDK)/lib/build.mak` at the bottom, which is
where all the actual compile/link rules live — these files only set variables).

Key variables and what changing them means:

- **`TARGET`** — output PRX name (`tempar` / `tempar_lite`).
- **`OBJS`** — the full, explicit list of `.o` files to link. **Adding a new `.c` file to the
  project means adding its `.o` here in both makefiles** (and in `makefile_psppr`/`makefile_litepr`
  if you want it in PR builds too) — there's no wildcard/glob, it's a static list.
- **`CFLAGS`** — compiler flags, most importantly the `-D_FEATURE_` defines that gate which
  optional subsystems compile into this binary (see the table in [ARCHITECTURE.md](ARCHITECTURE.md)
  and `src/notes.txt` for the full flag reference). Also carries several `-Wno-*` flags added to
  suppress warnings-as-errors-adjacent issues from building 2010-era C against a modern GCC — see
  [GOTCHAS.md](GOTCHAS.md), this is not optional cleanliness, some of these are load-bearing
  for the build to succeed at all with the current Docker image's toolchain.
- **`BUILD_PRX = 1`**, **`PRX_EXPORTS = exports.exp`** — tells PSPSDK's build system to produce a
  loadable PRX and to use `exports.exp` as the export table (see
  [SDK_USAGE.md](SDK_USAGE.md#exportsexp-and-importss) for what that file does).
- **`USE_KERNEL_LIBC = 1`**, **`USE_KERNEL_LIBS = 1`** — link against the kernel-mode libc/libs
  instead of the normal user-mode PSP libc. This is *why* `kmalloc.c`/`syslibc.c` exist — the
  kernel libc is missing pieces the user-mode one has.
- **`LIBS`** — PSP system libraries to link. Notably differs between full and lite builds
  (lite adds `-lpsputility`, full adds `-lpsppower_driver -lpspreg_driver -lpspge_driver` where lite
  uses the non-`_driver` variants `-lpspreg -lpspge` — kernel driver libs vs. normal libs, tied to
  which features are compiled in).

### Full vs. Lite: the actual `-D` diff

Full (`makefile_psp`):
```
_FONT_acorn _CWCHEAT_ _PSPAR_ _USB_ _PSID_ _SCREENSHOT_ _GUIDE_ _UMDDUMP_ _MODLIST_ _THLIST_
_MIPS_ _AUTOOFF_ _MULTILANGUAGE_
```

Lite (`makefile_lite`):
```
_FONT_acorn _CWCHEAT_ _PSPAR_ _GUIDE_ _MIPS_ _AUTOOFF_ _LITE_ _MULTILANGUAGE_
```

Lite drops `_USB_`, `_PSID_`, `_SCREENSHOT_`, `_UMDDUMP_`, `_MODLIST_`, `_THLIST_`, and adds a
`_LITE_` flag of its own (used in a couple of places to alter behavior, not just to strip code —
check `grep -rn _LITE_ src/` if you're touching lite-specific logic). Per the top-level README FAQ,
lite is what you use for POPS (PS1-on-PSP) titles.

## What `bin2c` is and why `prep` exists

`bin2c` (provided by the PSPSDK/toolchain image) turns an arbitrary binary file into a C source
array. The `prep` step turns the compiled language binary (`english.bin`) into `english.h` so it
can be compiled directly into the PRX rather than shipped/loaded as a loose file. If you're editing
language strings, you're editing whatever produces `english.bin` upstream (not committed source in
this repo as far as the tree shows — `english.bin` itself is checked in under
`src/languages/`), then re-running `prep` before `psp`/`lite`.

## CI (`.github/workflows/build.yml`)

Triggers: push to `main`/`master`/`feat/*`, PRs into `main`/`master`, tag pushes (`v*`), and manual
`workflow_dispatch` (which also lets you type a release tag).

Steps:
1. `docker run ... pspdev/pspdev make prep psp lite` — builds both PRXs (does **not** run `pack`
   here, that's separate).
2. `make pack` on the runner directly (installs `zip` via apt first if missing) — this only works
   because the previous step already produced the `.prx` files in `src/`.
3. Collects every `*.prx` and `*.zip` under `build/` into `artifacts/`.
4. Uploads `artifacts/` as a workflow artifact (`tempar-build`).
5. If the trigger was a tag push or a manual dispatch, creates a GitHub Release with those
   artifacts attached and auto-generated release notes.

Because `permissions: contents: write` is required for the release-creation step (`softprops/action-gh-release`) — this was a real fix commit (`ef2ec3d`, "add contents:write permission for
release creation"); if release creation starts failing with a 403, check this block first.

## Manual (non-Docker) builds

Not documented/tested in this repo — the README explicitly says "Build process tested on Windows
10" using the Docker flow above. If you want a native PSPSDK install instead of Docker, you're on
your own for toolchain setup; the makefiles themselves are ordinary PSPSDK makefiles and don't
assume Docker specifically, only `PSPSDK = $(shell psp-config --pspsdk-path)` needs to resolve
correctly on your `PATH`.
