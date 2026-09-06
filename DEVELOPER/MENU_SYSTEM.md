# Menu System

Core files: [`src/objects/menu.c`](../src/objects/menu.c) (3,698 lines — the largest file in the
project) / [`src/include/menu.h`](../src/include/menu.h), plus
[`src/objects/ctrl.c`](../src/objects/ctrl.c) (input), [`src/objects/config.c`](../src/objects/config.c)
(settings persistence), and [`src/objects/pspdebugkb.c`](../src/objects/pspdebugkb.c) (text entry).

## Architecture: a blocking state machine over a text console

There's no frame-timed render loop and no custom graphics engine. TempAR's UI draws directly onto
PSPSDK's built-in debug-screen text console (`pspDebugScreenKprintf`/`pspDebugScreenPuts`, aliased
via macros in `menu.h`: `#define printf(...) pspDebugScreenKprintf(__VA_ARGS__)` and
`#define puts pspDebugScreenPuts`). Every screen is a `while` loop that blocks on controller input,
redraws synchronously, and returns control up the call stack once the user backs out (CIRCLE) or
switches tabs (L/R trigger). There's no dirty-rectangle tracking — screens simply reprint.

**`menu_show()`** (`menu.c`) is the single entry point into the whole UI, called from
`button_callback()` in `main.c` once `menu.visible` is set. It performs PSP-specific setup/teardown
around the actual menu:

1. Hide the system home popup, delay ~150ms.
2. Mask controller input from reaching the game.
3. Snapshot `resume_count` (used to detect a suspend/resume cycle while the menu is open).
4. Resolve a VRAM pointer via `get_vram()` and re-init the debug screen against it (so the menu
   draws over the game's own framebuffer rather than a separate one).
5. Optionally pause the game's other threads (`gamePause()` in `main.c`).
6. Call `layout_tab()` — the actual UI — which blocks until the user closes the menu.
7. Reverse every step above, in reverse order.

**This setup/teardown is order-dependent and must be mirrored exactly.** There's no RAII/cleanup-
on-error here — it's manual. A `layout_*` function that adds a new early-return path deep in the
call tree without going through the normal return chain would leave the game's input permanently
masked, or the home-button config in the wrong state. Always `return`, never bypass the call stack.

`get_vram()` has a fallback path (hardcoded `0x44000000`) when the Impose plugin's framebuffer
can't be resolved via `find_uid_control_block_by_name` — this is the homebrew-without-imposeplugin
compatibility path, and it also force-sets `menu.options.force_pause = 1` as a side effect. That's
intentional, not a bug, but it's a non-obvious behavior change triggered by an unrelated-looking
function.

## Tab system

`layout_tab()` is the top-level UI loop: a `switch` on the global `char tab_selected` (0–4:
Cheater, Searcher, PRX/Options, Browser/Decoder, Credits/GameID), dispatching to one of five
`layout_*()` functions, each of which owns its own inner input loop and returns a `u32` control
mask when it exits. `layout_tab()` interprets L/R trigger on that returned mask to change
`tab_selected` and loops until CIRCLE is returned. Navigation is not event-driven — each screen
hands control back up explicitly when it's done.

Key screen functions:

| Function | Tab / role |
|---|---|
| `layout_heading()` | Draws the tab bar + cheat-engine on/off indicator (shared by every tab) |
| `layout_cheats()` | Cheater tab — cheat list browsing/toggling |
| `layout_cheatmenu(cheat)` | TRIANGLE popup submenu on a selected cheat (edit/rename/copy/delete/favorite) |
| `layout_cheatedit(cheat)` | Hex/code editor for a cheat's raw lines; invokes the on-screen keyboard for renaming |
| `layout_searcher()` | Searcher tab |
| `layout_options()` | PRX/Options tab — settings, hotkey rebinding, config save |
| `layout_browser()` | Memory Browser/Disassembler tab |
| `layout_copymenu(address, value, flags)` | Address/value copy-paste dialog shared by browser & disassembler |

Shared drawing helpers worth knowing:

- **`line_print(line)`** — draws a horizontal separator (row of box-drawing characters); appends at
  the cursor if `line < 0`.
- **`line_clear(line)`** — blanks a row (68 chars wide), or clears from the cursor to column 68 if
  `line < 0`.
- **`line_cursor(index, color)`** — draws a `^` marker under a column on the current line.
- **`get_print_start_end()`** — computes the visible scroll-window for any list UI (cheat list,
  disassembler, browser, mod/thread list) by centering the current selection and clamping to
  bounds. This function's math was the site of the display-order rendering regression fixed in
  `21dac59` — see [CHEAT_ENGINE.md](CHEAT_ENGINE.md#rendering-regression-fix-21dac59).
- **`percentage_to_color(percent)`** — interpolates red→green for progress/battery-style displays.
- **`show_error(error)`** — prints an error on the last line and blocks for 1 second.

## Input handling (`ctrl.c`)

`ctrl_read()` is the single point where the raw `SceCtrlData` is polled
(`sceCtrlPeekBufferPositive`) and turned into TempAR's augmented button mask:

- A HOME press, or a `resume_count` mismatch (the PSP woke from suspend while the menu was open),
  forces `menu.visible = 0` — this is how the menu auto-closes across suspend/resume.
- Analog stick tilt is synthesized into `PSP_CTRL_ANALOG_LEFT/RIGHT/UP/DOWN` bits (threshold
  `<50`/`>200` on the 0–255 axis) so list navigation code can treat the analog stick like a D-pad.
- When the menu isn't visible, `PSP_CTRL_CIRCLE` is force-set in the returned mask. **Any code that
  reads `ctrl_read()` outside a menu-visible context will always see CIRCLE held down** — know this
  before repurposing the function.
- `cfg.swap_xo` (CROSS/CIRCLE swap) is applied here, transparently, before any other code sees the
  mask — every other file that checks CROSS/CIRCLE never needs to know the setting exists.

Blocking wait helpers built on `ctrl_read()`: `ctrl_waitany()`, `ctrl_waitkey()`, `ctrl_waitmask()`,
and `ctrl_waitrelease()` (blocks until *all* buttons are released — used to stop one physical press
from being read by two different menu states in a row). All the wait helpers implement key-repeat
via `ctrl_delay()`: while held, `repeat_count` increments (capped at 12) to accelerate repeat speed,
using `CTRL_REPEAT_TIME`/`CTRL_REPEAT_INTERVAL` (145ms / 9ms) by default, or the `_QUICK` variants
(17ms / 1ms) for fast-scroll contexts like the browser/disassembler (SQUARE+direction, per the
top-level README's hotkey table).

`get_menu_ctrl(delay, allow_empty)` (in `menu.c`) is a separate, simpler debounced read used by the
top-level tab loop: it polls twice, `delay` µs apart, and only accepts input if both reads agree —
restricted to a `MENU_KEY` bitmask (an inline `#define` inside the function, easy to miss when
scanning the file). **If you add a new hotkey elsewhere in the UI, it won't be visible to this
debounce loop unless `MENU_KEY` is updated too.**

**Global hotkeys** are registered via `sceCtrlRegisterButtonCallback` in `main.c` at boot, and
re-registered three more times inside `menu.c`'s options screen whenever the user rebinds a key.
`button_callback()` (`main.c`) is the actual dispatcher: while the menu is closed, it checks
`curr_but` against `cfg.menu_key` (open menu), `cfg.screen_key` (screenshot), and `cfg.trigger_key`
(toggle all cheats via `cheat_apply(CHEAT_TOGGLE_ALL)`). If you add a fourth place where hotkeys can
be edited, remember to re-register the callback there too, or it keeps firing on stale bindings.

## Config persistence (`config.c`)

`Config` (`config.h`) is a single `__attribute__((packed))` struct — `ver` byte, settings fields,
trailing `checksum`. **The struct layout *is* the file format**: `config_load()` reads the version
byte, and if it matches `CONFIG_VER` (currently `0x08`), reads the rest of the struct as one raw
memory blob straight from the file (`(void*)&cfg + 1`, size `sizeof(Config) - 1`). Field order,
packing, and alignment must exactly match between whatever wrote the file and whatever reads it.

There is **no migration path**. A version mismatch — or a failed checksum, or an out-of-range field
caught by `config_validate()` (which clamps `max_cheats`/`max_blocks`/`max_text_rows` and
`address_start < address_end`) — causes a silent full reset to defaults, not a partial upgrade.
**Any time you add, remove, or reorder a `Config` field, bump `CONFIG_VER`** — otherwise old config
files will be misinterpreted rather than rejected.

`config_save()` computes the checksum (simple additive byte sum over all fields except itself) and
writes the whole struct as one blob. Colors are loaded separately and are human-editable:
`color_load()` parses `colors/colorN.txt` line-by-line for `0x`-prefixed hex values via `strtoul`
(this is the file format documented in the top-level README's Skinning section).

## On-screen keyboard (`pspdebugkb.c`)

A PSPSDK-provided utility (not TempAR-authored — carries the original 2006 PSPSDK copyright) for
editing a string in place with the D-pad instead of the system OSK dialog. Draws a fixed
13-column × 4-row character grid plus a 5-item command row (backspace/space/shift/etc) at a
hardcoded screen position. `pspDebugKbInit(str, len)` is the blocking entry point, called from
exactly two places in `menu.c`: renaming a cheat (`layout_cheatedit`) and entering search text
(`layout_searcher`'s text-search path).

## Text rendering & multi-language

`text.c`/`text.h` is *not* about fonts — it's the guide/text-file viewer (`_GUIDE_`), providing
`text_open`/`text_rows`/`text_read`/`text_close` to page through an arbitrary text file for the
in-menu game guide screen. Actual glyph rendering is delegated entirely to PSPSDK's debug-screen
font system; `src/fonts/*.h` are alternate bitmap font tables selectable via `_FONT_xxx` build
defines, but only `_FONT_acorn` is ever passed by the current makefiles — the others are dead
unless a makefile is edited to reference them.

Every user-facing string in `menu.c` goes through a global `extern Language lang;` struct
(`lang.tabs.cheater`, `lang.errors.error`, etc.) rather than string literals, gated by
`_MULTILANGUAGE_`.

## Global state (`MenuState`)

`menu` (`MenuState`, `menu.h`) is a single global struct holding: `visible` (menu open/closed),
`bd` (`BrowserDecoder` — cursor/view state for the browser and disassembler), `cheater`
(display/edit state for the cheat list), `options` (force-pause flag, dump index, cheat index),
`copier` (copy/paste buffer for addresses+values), and `guide_path`. Like the rest of the codebase,
there's no per-session context object — this global *is* the UI's session state, reset once by
`menu_init()` at boot. There is currently no reentrancy into `layout_tab()`, but a future refactor
that introduces any should treat this struct's mutation very carefully.
