# embroidery-studio

Lightweight, native C23 + SDL2 cross-stitch embroidery editor designed for **Janome Memory Craft 550E** workflows.

Runs natively on Wayland and X11 with a fast software framebuffer, built-in 2x bitmap UI font, POSIX file browser, drag-and-drop support, and zero heavy GUI toolkit dependencies.

## Features

- **Multi-format import:** `.jef`, `.jef+`, `.pes`, `.pec`, `.dst`
- **Valid `.jef` export:** Janome 79-color palette, automatic jump routing, and tie-off lock stitches on cut chain boundaries
- **Automatic cross-stitch grid detection:** detects stitch pitch (`1.0–5.0 mm`), grid origin phase, and pass thickness (`1x–6x`)
- **Three visual stitch modes:**
  - `Thread` (default): zoom-scaled rounded thread capsules with 1px shading (`0.35`, `0.45`, `0.60 mm`)
  - `Blocks`: deduplicated cross-stitch chart squares with 1px cell gaps and contour overlay
  - `Line`: fast 1px stitch-path preview
- **Cross-stitch editing tools:**
  - `Sel Elem`: connected-component selection (flood fill), recolor, arrow-key grid nudge, delete
  - `Sel X`: single-cross click or rectangle selection and locked deletion
  - `Draw X`: click-and-drag cross painting snapped to grid (`1x–6x` passes)
  - `Strip`: built-in repeating border/strip pattern designer (`Insert` onto canvas or export directly to `.jef`)
  - `Crop` & `Cut`: exact midpoint + atomic cross-cluster cropping and cutout with live dimmed preview
  - `Split V` / `Split H`: split large designs vertically or horizontally for multi-hoop stitching
  - `Clone` & `Merge`: clone regions or drop/merge external `.jef`/`.pes`/`.pec`/`.dst` fragments snapped to grid
- **Janome MC550E hoop overlays:** `SQ14b` (`140×140`), `RE20b` (`140×200`), `SQ20b` (`200×200`), `RE28b` (`200×280`), `RE36b` (`200×360`) with rotation and out-of-hoop warning
- **Stitch simulator:** interactive `0–100%` stitch playback slider with live needle indicator

## Requirements

- C23 compiler (`gcc >= 14` or `clang >= 19`)
- `sdl2` development package (`pkg-config --cflags --libs sdl2`)
- Standard C/POSIX library and `libm`

## Build, Test, and Install

```bash
# Build optimized release binary
make

# Run standalone C23 core & regression test suite under ASan + UBSan
make test

# Optional: also verify real embroidery files from a local directory
EMB_TEST_DIR=/path/to/designs make test

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
| `View:Thread` | Thick rounded thread capsules scaled by zoom (`Thk:0.35 / 0.45 / 0.60 mm`) |
| `View:Blocks` | Crisp embroidery chart squares (deduplicated passes + backstitch contours) |
| `View:Line` | Fast 1-pixel stitch line view |
| `Sel Elem` | Select connected element, recolor, move on grid, or delete |
| `Sel X` | Select individual crosses (click or drag box) |
| `Draw X` | Paint new crosses snapped to pitch and pass count (`1x..6x`) |
| `Strip` | Open repeating ornamental strip builder (`Insert`, `Save JEF`, `Save As`) |
| `Crop` / `Cut` | Keep only inside box (`Crop`) or cut out inside box (`Cut`) with live preview |
| `Split V` / `Split H` | Split design into two valid `.jef` files along vertical or horizontal line |
| `Clone` | Copy boxed region into a movable grid-snapped fragment |
| `Pick` | Eyedropper to select active color block from canvas |

## Keyboard Shortcuts

| Key | Action |
| --- | --- |
| `T` | Cycle view mode (`Thread` → `Blocks` → `Line`) |
| `W` | Cycle thread thickness (`0.35` → `0.45` → `0.60 mm`) |
| `1` / `E` | Select Element (`Sel Elem`) |
| `2` / `X` | Select Cross (`Sel X`) |
| `3` / `D` | Draw Cross (`Draw X`) |
| `M` | Open `Strip` pattern builder |
| `4` / `5` | `Crop` / `Cut` |
| `6` / `7` | `Split V` / `Split H` |
| `8` | `Clone` |
| `9` / `I` | Color Picker (`Pick`) |
| `Enter` | Apply active `Crop` / `Cut` / `Split` or place floating fragment |
| `Delete` / `Backspace` | Delete selected crosses / element |
| `Arrow keys` (`Shift` for 5×) | Nudge selected element or floating fragment along grid |
| `R` / `H` / `V` / `C` | Rotate 90° / Flip X / Flip Y / Center design |
| `G` / `J` / `L` | Toggle Grid / Jumps / Locks |
| `O` | Cycle Janome MC550E hoop preset |
| `F` / `0` | Fit design to viewport |
| `Space + Drag` / `Middle Drag` | Pan canvas (`Mouse Wheel` to zoom) |
| `Ctrl+O` | Open file dialog |
| `Ctrl+S` / `Ctrl+Shift+S` | Save / Save As `.jef` |
| `Ctrl+Z` / `Ctrl+Shift+Z` (`Ctrl+Y`) | Undo / Redo |
| `Esc` | Cancel placement, close modal, or clear selection |
| `Ctrl+Q` | Quit |

## License

MIT — see [LICENSE](LICENSE).
