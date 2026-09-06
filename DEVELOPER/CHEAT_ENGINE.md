# Cheat Engine

Core files: [`src/include/cheat.h`](../src/include/cheat.h),
[`src/objects/cheat.c`](../src/objects/cheat.c) (2,406 lines — the
largest logic file after `menu.c`).

## Data model

Two flat, pre-allocated arrays hold everything (allocated once in
`cheat_init`, sized by `cfg.max_cheats`/`cfg.max_blocks` from
`config.bin`):

- **`Cheat`** (`cheat.h:87`) — one entry per cheat *or* per folder/comment
  (folders and comments are just cheats with `length == 0` and specific
  flag bits set). Fields: `name[32]`, `parent` (pointer to parent folder
  Cheat, for nesting), `block` (start index into the block array),
  `length` (number of code lines), `flags` (bitmask, see below).
- **`Block`** (`cheat.h:103`) — one entry per code *line*. Just
  `address` and `value` (both `u32`). A cheat's code lines are
  `blocks[cheat->block .. cheat->block + cheat->length)`.

`Cheat.flags` (`enum CheatFlags`, `cheat.h:15`) packs several orthogonal
concerns into one byte:

| Flag | Meaning |
|---|---|
| `CHEAT_HIDDEN` | Containing folder is collapsed (display state) |
| `CHEAT_FRESH` | Just toggled on/off, not yet (re-)applied — used by the `_AUTOOFF_` backup/restore logic |
| `CHEAT_SELECTED` | User has this cheat checked on |
| `CHEAT_CONSTANT` | Cheat is always-on regardless of the global enable toggle |
| `CHEAT_CWCHEAT` / `CHEAT_PSPAR` / `CHEAT_PSPAR_EXT` | Which engine parses/executes this cheat's code lines (see below) |
| `CHEAT_FAVORITE` | Displayed at top of the list, `*` indicator (added in v1.65, see below) |

Folders reuse the same struct with `FolderFlags` (`cheat.h:37`):
`FOLDER_EXPANDED`/`FOLDER_COLLAPSED` (display state) and
`FOLDER_SINGLE_SELECT`/`FOLDER_MULTI_SELECT`/`FOLDER_COMMENT` (behavior —
single-select folders act like a radio group, multi-select like
independent checkboxes, comment folders are just visual grouping/labels
with no toggle behavior).

## Two engines, one file format

TempAR supports two independently-implemented code interpreters:

- **CWCheat** — `cheat_apply_cwcheat()` (`cheat.c:452`). Standard CWCheat
  code types (constant write, pointer chains including multi-level/
  multi-write, conditionals, etc). On POPS (PS1-on-PSP), CWCheat-flagged
  cheats are instead run through `cheat_apply_psx_gs()` (PSX GameShark
  format) since real CWCheat semantics don't apply to PS1 memory.
- **PSPAR / PSPAR Extended** — `cheat_apply_pspar()` (`cheat.c:207`).
  Datel's PSP Action Replay code types, plus TempAR's own `_N`-prefixed
  extended type set (register operations, safe data store, counters,
  call-function-with-args, etc — see `docs/README.md`'s changelog for
  the `0xC1`–`0xC5` extended type additions).

Which engine parses a cheat is decided **at load time** based on the code
name prefix on the *last* line of the code block in the source file
(`_C0`/`_C1`/`_C2`/`_N`/etc — see `cheat_load_db`), and is stored
permanently as one of the `CHEAT_CWCHEAT`/`CHEAT_PSPAR`/`CHEAT_PSPAR_EXT`
flag bits on the `Cheat`. A single code cannot mix both engines' line
types — see the FAQ in `docs/README.md`. `cheat_get_engine()` /
`cheat_set_engine()` / `cheat_get_engine_string()` are the accessors for
this flag group.

## Load → memory → apply pipeline

1. **`cheat_init(num_cheats, num_blocks, auto_off)`** (called once from
   `main_thread`) allocates the `Cheat`/`Block` arrays via `kmalloc` and
   optionally an `original_values` backup buffer if `_AUTOOFF_` is
   enabled.
2. **`cheat_load(game_id, dbnum, index)`** is the entry point that tries,
   in order: the game's saved `.db` file
   (`ms0:/seplugins/TempAR/cheats/{game-id}.db`), then falls back through
   `cheat_load_db` (CWCheat/PSPAR text format), `cheat_load_bin` (PSPAR
   `.bin` database), and `cheat_load_npr` (NitePR `.txt`, converted to
   PSPAR format on the fly) depending on what's found on disk — this is
   the mechanism behind the "can I use my old NitePR/PSPAR codes" FAQ
   entries in `docs/README.md`.
3. **`cheat_load_db`** streams the source file through the
   [`filebuffer.c`](#) line/word reader (`fileIoGet`, `fileIoSkipLine`,
   etc — see below), building up `Cheat`/`Block` entries with
   `cheat_add`/`block_insert` as it goes. Favorite markers (`_F <index>`
   lines) are collected into a deferred list and applied after all
   cheats are loaded, since a cheat's final index isn't known until the
   whole file is parsed (see `strtoul` gotcha in GOTCHAS.md — this
   parsing is exactly where that fix landed).
4. **`cheat_apply(action)`** (`cheat.c:154`) runs every plugin loop tick
   (interval `cfg.cheat_hz`, or every menu-closed iteration — see
   `main.c`). For each cheat in the flat array: if `_AUTOOFF_` is
   compiled in, it first runs the backup/restore state machine based on
   `CHEAT_FRESH`/`CHEAT_SELECTED`/`CHEAT_CONSTANT` transitions (this is
   what lets a plain constant-write code be safely turned off without a
   matching "off" code — the *original* memory value is restored). Then,
   if the cheat is active (`CHEAT_SELECTED` and the global engine is on,
   or `CHEAT_CONSTANT` regardless), it dispatches to
   `cheat_apply_cwcheat`/`cheat_apply_psx_gs`/`cheat_apply_pspar` based
   on the cheat's engine flag. This reapply-every-tick model (rather
   than apply-once) is what makes memory-patching codes "sticky" against
   game code that keeps overwriting the same addresses.
5. **`cheat_save(game_id)`** writes the current in-memory cheat list back
   out to the per-game `.db` file, including `_F` lines for any
   favorited cheats.

## Address model

`address_load`/`address_set` (`cheat.h:126`) are the single choke point
for all cheat memory reads/writes, and take a `type` byte encoding
8/16/32-bit width, OR'd with `CHEAT_REAL_ADDRESS` to permit addresses
outside normal user memory (kernel/hardware register ranges — used by
the "fake addresses" pad-state read feature mentioned in
`docs/README.md`, and by the browser/decoder when in real-address mode).
If you're adding a new code type that touches memory, go through these
functions rather than dereferencing pointers directly — see the security
hardening notes in GOTCHAS.md (`memory_copy` bounds-checking, patch
destination validation) for why raw dereferencing has bitten this
codebase before.

## Favorites (added v1.65, hardened v1.70)

Added in commit `be643f5`:

- New `CHEAT_FAVORITE` flag (`0x80`).
- Favorited cheats are shown first in the list, then non-favorites in
  original order — implemented via `cheat_get_by_display_index()` and
  `cheat_visible_count()`, which compute a *display-order* index
  separate from the underlying storage index, rather than physically
  reordering the `Cheat` array.
- Toggled from the TRIANGLE cheat menu ("Add/Remove Favorite").
- Persisted in `.db` files as `_F <index>` lines — a new, unrecognized-
  by-old-parsers line type, chosen deliberately so older TempAR builds
  (or other tools reading the same `.db` format) simply ignore the line
  rather than failing to parse the file.

**Follow-up fix (`8080309`)**: the initial implementation toggled the
in-memory flag but never called `cheat_save()`, so favorite status was
lost on next boot/reload. One-line fix: call `cheat_save(gameid_get(0))`
right after `cheat_toggle_favorite()` in the menu handler. If you add
another piece of per-cheat mutable state, remember it needs the same
explicit save call — there's no automatic dirty-tracking/autosave in
this codebase.

**Rendering regression fix (`21dac59`)**: introducing display-order
indexing changed the cheat list's on-screen scroll-window calculation,
which shrank the visible window to ~13 items near the top of the list
instead of the intended 25. Fixed by reworking the window math in
`layout_cheats()` (`menu.c`) to always maintain a full 25-item window
(12 items before/after the cursor when not near an edge, clamped and
re-extended forward when near the top) — see
[MENU_SYSTEM.md](MENU_SYSTEM.md) for the general pattern this function
follows.

## Security hardening (v1.70, commit `21dac59`)

A dedicated pass fixed ten numbered issues, mostly unchecked-length /
unchecked-address bugs typical of code from an era before this class of
bug was taken as seriously. Worth knowing before touching the same code
paths again:

1. `boot_path` (`main.c`) wasn't guaranteed null-terminated after
   `strncpy` from `sceKernelInitFileName()` — fixed by explicit
   null-termination after the copy.
2. `memory_copy` (`cheat.c`) had no bounds check against the game's
   valid memory range — now clamped.
3. The PSPAR `0x0E` "patch" code type didn't validate its destination
   address/length before writing — now validated.
4. All `sprintf` call sites replaced with `snprintf` (buffer-overflow
   hardening) across the codebase.
5. `game_id` wasn't guaranteed null-terminated after UMD/POPS reads —
   fixed.
6. `kmalloc.c`'s partition-ID loop had an out-of-bounds iteration bug —
   fixed by switching to a `sizeof`-based bound.
7. The file-buffer read buffer (`filebuffer.c`) wasn't guaranteed
   null-terminated — now allocated `FILE_BUFFER_SIZE + 1` and always
   terminated.
8. `patch_apply` (memory patch file loader) didn't validate address +
   length before writing — now validated.
9. The POPS game-ID address wasn't validated before being dereferenced —
   now validated.
10. `config.c` gained a checksum (`Config.checksum`, `CONFIG_VER` bumped
    to `0x08`) plus field clamping/validation on load, so a corrupted or
    hand-edited `config.bin` can't push out-of-range values (e.g.
    `max_cheats`/`max_blocks`) into `cheat_init`'s allocation sizing.

If you're adding a new file parser, memory-writing code path, or
fixed-size buffer fill, treat these ten as the checklist: null-terminate
after every fixed-size string copy, validate lengths/addresses before
any write derived from file/user input, and prefer `snprintf` over
`sprintf` by default.
