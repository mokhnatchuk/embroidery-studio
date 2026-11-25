# embroidery-studio

Lightweight, native C23 + SDL3 cross-stitch embroidery editor for Linux.

Runs natively on Wayland and X11 with HiDPI scaling, a fast cached software framebuffer (dirty-rect incremental updates), crisp FreeType2 antialiased typography (Latin + Ukrainian Cyrillic with automatic built-in 8×8 fallback), native system file dialogs, drag-and-drop support, and zero heavy GUI toolkit dependencies.

## Features

- **Multi-format import:** `.jef`, `.jef+`, `.pes`, `.pec`, `.dst` (`.jef+` is selectable via `All files` in the system dialog, drag-and-drop, or CLI)
- **Valid `.jef` export:** standard 79-color JEF thread palette, automatic jump routing, tie-off lock stitches on cut chain boundaries, and atomic file replacement (`fsync` + `rename` in the destination directory)
- **Automatic cross-stitch grid detection:** detects stitch pitch (`1.0–5.0 mm`), grid origin phase, and pass thickness (`1x–6x`)
- **Three visual stitch modes:**
  - `Thread` (default): zoom-scaled rounded thread capsules with 1px shading (`0.35`, `0.45`, `0.60 mm`)
  - `Blocks`: deduplicated cross-stitch chart squares with 1px cell gaps and backstitch contour overlay
  - `Line`: fast 1px technical stitch-path preview
- **Compact split-layout interface:**
  - Single-row `38 px` top bar with file/edit/view controls and `*` unsaved-changes indicator
  - `76 px` left tool rail for editing tools, transforms, and selection actions
  - Collapsible `268 px` right panel (`Tab`) with grid/thread/hoop properties and scrollable color block list (**Right-Click** on a color row toggles **Solo** visibility)
  - Live drag dimensions badge (in mm and crosses), bottom stitch simulator (`0–100%`), and unsaved-changes guard on `Open` / `Quit`
- **Native system file dialogs:** `Open`, `Merge`, and `Save As` use SDL3 native file dialogs (XDG Desktop Portal on Linux), plus drag-and-drop and CLI loading
- **Cross-stitch editing tools:**
  - `Sel Elem`: connected-component selection, recolor, arrow-key grid nudge, delete
  - `Sel X`: single-cross click or rectangle selection and locked deletion
  - `Draw X`: click-and-drag cross painting snapped to grid (`1x–6x` passes)
  - `Strip`: built-in repeating border/strip pattern designer (`Insert` onto canvas or export directly to `.jef`)
  - `Crop` & `Cut`: exact midpoint + atomic cross-cluster cropping and cutout with live dimmed preview
  - `Split V` / `Split H`: split large designs vertically or horizontally for multi-hoop stitching
  - `Clone` & `Merge`: clone regions or drop/merge external `.jef`/`.pes`/`.pec`/`.dst` fragments snapped to grid
- **Standard hoop overlays:** `140x140 mm`, `140x200 mm`, `200x200 mm`, `200x280 mm`, `200x360 mm` with 90° rotation and out-of-hoop warning

## Requirements

- C23 compiler (`gcc >= 14` or `clang >= 19`)
- `sdl3` development package (`pkg-config --cflags --libs sdl3`)
- `freetype2` development package (`pkg-config --cflags --libs freetype2`)
- Standard C/POSIX library and `libm`
- Optional: set `EMBROIDERY_FONT=/path/to/font.ttf` to override the automatic system UI sans-serif font discovery (falls back automatically to the built-in 8×8 font if no system TTF is available)
- Optional development tools: `clang-tidy` (for `make lint`), `clang` with libFuzzer (for `make fuzz`)

## Build, Test, and Install

```bash
# Build optimized release binary
make

# Run C23 core, layout, dialog-filter, and regression tests under ASan + UBSan + LSan
make test

# Optional: also verify real embroidery files from a local directory
EMB_TEST_DIR=/path/to/designs make test

# Run clang-tidy static analysis across all C23 modules
make lint

# Run 200k+ stitch headless rendering and hover benchmark (with allocation tracking)
make bench

# Run libFuzzer harnesses on .jef, .pes, .pec, and .dst binary parsers
make fuzz

# Install binary to ~/.local/bin and .desktop entry to ~/.local/share/applications
make install
```

Custom installation prefix:

```bash
make install PREFIX=/usr/local
```

Launch from terminal or file manager:

```bash
embroidery-studio [design.jef]
```

## Tools & View Modes

| Mode / Tool | Description |
| --- | --- |
| `View:Thread` | Thick rounded thread capsules scaled by zoom (`0.35` / `0.45` / `0.60 mm`) |
| `View:Blocks` | Crisp embroidery chart squares (deduplicated passes + backstitch contours) |
| `View:Line` | Fast 1-pixel stitch line view |
| `Elem` (`Sel Elem`) | Select connected element, recolor, move on grid, or delete |
| `SelX` (`Sel X`) | Select individual crosses (click or drag box) |
| `Draw` (`Draw X`) | Paint new crosses snapped to pitch and pass count (`1x..6x`) |
| `Strip` | Open repeating ornamental strip builder (`Insert`, `SaveAs`, `SaveJEF`) |
| `Crop` / `Cut` | Keep only inside box (`Crop`) or cut out inside box (`Cut`) with live preview |
| `SplV` / `SplH` | Split design into two valid `.jef` files along vertical or horizontal line |
| `Clon` (`Clone`) | Copy boxed region into a movable grid-snapped fragment |
| `Pick` | Eyedropper to select active color block from canvas |

## Keyboard Shortcuts & Mouse Controls

| Input | Action |
| --- | --- |
| `Tab` | Collapse / expand right `Props + Colors` panel |
| `Right-Click` (color row) | Toggle **Solo** visibility for a color block (right-click again to restore all) |
| `T` | Cycle view mode (`Thread` → `Blocks` → `Line`) |
| `K` | Cycle thread thickness (`0.35` → `0.45` → `0.60 mm`) |
| `P` | Cycle cross pass count (`1x`..`6x`) |
| `[` / `]` | Decrease / increase grid pitch (`0.1 mm`) |
| `1` / `2` / `3` | `Sel Elem` / `Sel X` / `Draw X` |
| `4` / `5` | `Crop` / `Cut` |
| `6` / `7` | `Split V` / `Split H` |
| `8` / `9` | `Clone` / `Pick` |
| `Enter` | Apply active `Crop` / `Cut` / `Split` or place floating fragment (`Shift+Enter` to keep stamping) |
| `Delete` / `Backspace` | Delete selected crosses / element |
| `Arrow keys` | Nudge selected element or floating fragment by 1 grid pitch (`Shift+Arrows` shifts design center when nothing is selected) |
| `R` / `H` / `V` / `C` | Rotate 90° / Flip X / Flip Y / Center design |
| `G` / `J` / `L` | Toggle Grid / Jumps / Locks |
| `F` | Fit design to viewport |
| `Space + Drag` / `Middle Drag` | Pan canvas (`Mouse Wheel` to zoom, or scroll color list over right panel) |
| `Ctrl+O` / `Ctrl+M` | Open / Merge embroidery file (native dialog) |
| `Ctrl+S` / `Ctrl+Shift+S` | Save / Save As `.jef` |
| `Ctrl+Z` / `Ctrl+Shift+Z` (`Ctrl+Y`) | Undo / Redo |
| `Esc` | Cancel placement, close modal, or clear selection |
| `Ctrl+Q` | Quit (with unsaved-changes guard) |

## License

MIT — see [LICENSE](LICENSE).
