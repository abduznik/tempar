# Disassembler, Search & File Browser

These three modules back the "Browser/Decoder" tab and the "Searcher" tab described in the
top-level README's hotkey tables.

## MIPS disassembler (`disasm.c`)

[`src/objects/disasm.c`](../src/objects/disasm.c) (1,022 lines) and
[`src/include/disasm.h`](../src/include/disasm.h) are **vendored, not TempAR-original** — both
carry the PSPLINK project's 2006 BSD-licensed header (`James F <tyranid@gmail.com>`). This is the
same disassembler PSPLINK used for its debug/Impose menu. `disasm_prxtool.h` supplies the MIPS
opcode table format this decoder walks.

It decodes a raw 32-bit MIPS instruction word into a mnemonic + operand string using a
format-string-driven table: each opcode entry has a format string using codes like `%d`/`%s`/`%t`
(register fields), `%i`/`%I` (signed/unsigned 16-bit immediate), `%o`/`%O`/`%V` (offset variants),
`%j`/`%J` (jump targets), plus VFPU-specific codes (`%x?`/`%y?`/`%z?` etc. for the PSP's vector
floating-point unit instructions) — see the comment block at the top of `disasm.c` for the full
format-code legend before touching this file.

`mipsDecode(opcode, PC)` is the entry point used by the menu's Decoder screen; if `_MIPS_` isn't
defined at compile time, a stub version is compiled in instead that just prints `"???"` — the whole
disassembler is optional, gated purely by that one flag (set in both `makefile_psp` and
`makefile_lite` currently, so it's effectively always on, but the toggle exists).

Runtime options (`disasmSetHexInts`, `disasmSetMRegs`, `disasmSetSymAddr`, `disasmSetMacro`,
`disasmSetPrintReal`) control formatting — hex vs. decimal immediates, mnemonic vs. numeric register
names, PC-relative symbol resolution, etc. — surfaced in the menu as the decoder's SELECT-toggled
"value vs. opcode view".

**If you ever touch this file:** it's someone else's vendored code with its own conventions
(different from the rest of the codebase); prefer patching around it (in `menu.c`'s call sites)
over modifying the decode tables unless you're fixing an actual decode bug.

## Memory search / "cheat finder" (`search.c`)

[`src/objects/search.c`](../src/objects/search.c) (346 lines) implements the classic
Gameshark-style "narrow down an unknown address" search: repeated passes over a memory range,
each pass filtering the previous pass's candidate addresses by a new criterion, until few enough
addresses remain to inspect by hand.

### Data model

`Search` (`search.h`) holds: a fixed `results[SEARCH_ADDRESSES_MAX]` array (100 addresses — this is
just the *preview* held in memory, not the full result set, see below), a ring of up to 16
`SearchHistoryItem` entries (`{value, flags}` — one per search pass, most recent first), counts, and
the active `address_start`/`address_end` range (defaults from `cfg.address_start`/`address_end`,
independently overridable per-search).

### Why it's file-backed, not memory-backed

Every search pass is streamed to a file: `searches/search{N}.dat`. This isn't an implementation
detail you can ignore — it's the reason searches **survive across menu sessions and even reboots**
(mentioned nowhere in the end-user docs, but it's a real feature: `search_init()` at boot looks for
existing `search{N}.dat` files or a `search.ram` file and resumes from there rather than starting
over). Each `.dat` file's format is: one `SearchHistoryItem` header, followed by a stream of
`(address, value)` pairs (or just `address` for a "known value" search, since the value is already
implied by the search criterion — see the `(search_item.flags & 0xF) == 6` check throughout).

`search.ram` (search #0) is special: it's an *initial full memory dump* rather than a filtered
result set, used as the input to the very first search pass. Passes after that read the previous
`search{N}.dat` as input and write a new, smaller `search{N+1}.dat` as output.

### The performance rewrite

CHANGELOG.md documents a search rewrite that was 25×–253× faster than the original MKUltra-derived
implementation. Reading `search_start()` today, the technique is: **stream through candidate
addresses with buffered file I/O in one tight pass**, computing the check inline
(`*(u32*)address`/`*(u16*)address`/`*(u8*)address` directly — no function-call overhead per
address), rather than issuing a separate `sceKernel*` syscall per address/comparison as older
implementations reportedly did. The loop also periodically yields
(`sceKernelDelayThread(1500)` every 64KB of address space, `if(!(address & 0xFFFF))`) specifically
to avoid starving the game thread badly enough to crash it — the comment `// delay search so we
don't crash` is literal, not decorative. If you're optimizing this further, preserve that yield;
removing it to "go faster" risks reintroducing crashes on real hardware.

### Search modes

`search_mode` (passed to `search_start`) is a small integer selecting the comparison:

| Mode | Comparison | Typical use |
|---|---|---|
| 0 | `value == check` (exact, fixed value) | "find this exact number" |
| 1 | `value != check` | "find anything that changed away from X" |
| 2 / 3 | `value >= check` / `value <= check` | range narrowing |
| 4 / 5 | `previous + delta == check` / `previous - delta == check` | "increased/decreased by exactly N" |
| 6–9 | Same as 0–3 but compared against the **previous pass's stored value per-address** rather than a fixed constant | "unknown value" searches — find what changed, without knowing the target value up front |

### Adding results as cheats

`search_add_result(address, flags)` and `search_add_loaded_results(flags)` convert search hits
directly into `Cheat`/`Block` entries via `cheat_new()`, using `MEM_VALUE`/`MEM_VALUE_SHORT`/
`MEM_VALUE_INT` macros to read the current value at each hit address as the cheat's initial value.
This is the SQUARE-on-result shortcut mentioned in the CHANGELOG ("Can add search results in
CWCheat format by pressing SQUARE on the search result").

## File browser (`filebrowser.c`)

[`src/objects/filebrowser.c`](../src/objects/filebrowser.c) (154 lines) is a straightforward
memory-stick directory lister used anywhere the UI needs the user to pick a file (loading a text
guide, a memory patch file, an alternate cheat database). `FileBrowser` (`filebrowser.h`) caps at
`MAX_FILES` (500) entries of `MAX_NAME_LEN` (49) chars each — if a directory has more entries than
that, the excess simply won't be listed (no pagination/streaming — the whole directory listing is
expected to fit in one pass).

`filebrowser_cache(path, ext, ext_count)` populates the `FileBrowser` struct from
`sceIoDopen`/`sceIoDread`, filtering by an extension allow-list (`ext`, an array of `ext_count`
suffix strings) so, e.g., the guide-file picker only shows `.txt` files. `filebrowser_display()`
renders and drives the actual pick UI; `filebrowser_updir()` handles navigating to the parent
directory by trimming the last path segment.
