# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Tractyl ManuForm is a parametric, split ergonomic keyboard with integrated trackball. The single source file (`src/dactyl_keyboard/dactyl.clj`) generates OpenSCAD models programmatically using the `scad-clj` library. Generated `.scad` files are then rendered to `.stl`/`.dxf` for 3D printing.

## Toolchain

- **Clojure + Leiningen**: Model generation
- **OpenSCAD**: Rendering SCAD → STL/DXF
- **nutsnbolts**: Git submodule (`nutsnbolts/`) providing bolt/fastener SCAD models — initialize with `git submodule update --init`

## Commands

Generate SCAD files (outputs to `things/`):
```sh
lein generate
# or equivalently:
lein run src/dactyl_keyboard/dactyl.clj
```

Render a single SCAD file to STL:
```sh
openscad -o things/right.stl things/right.scad
```

Build all layout variants (4x5, 4x6, 5x6, 6x6) and export STL/DXF in parallel:
```sh
./create-models.sh
```

Fix trailing commas in generated SCAD (run if OpenSCAD complains about syntax):
```sh
./scripts/fix-scad-trailing-comma.sh
```

## Architecture

All keyboard geometry is defined in one file: `src/dactyl_keyboard/dactyl.clj` (~1757 lines).

**Key parameters at the top of the file** (lines ~15–100):
- `nrows`, `ncols` — key matrix dimensions
- `trackball-enabled` — include trackball mount
- `printed-hotswap?` — 3D-printed vs. commercial hotswap sockets
- `α`, `β` — column/row curvature angles
- `tenting-angle`, `centercol`, `centerrow` — tilt/height geometry
- `keyboard-z-offset`, `extra-width`, `extra-height`, `wall-thickness` — overall sizing

**Output files written at the bottom of the file** (lines ~1720+):
- `things/right.scad` / `things/left.scad` — main keyboard halves (left is right mirrored)
- `things/right-plate.scad` / `things/left-plate.scad` — bottom plates
- `things/palm-rest.scad`, `things/left-palm-rest.scad`
- `things/tent-*.scad` — tenting foot/stand/nut
- `things/hotswap-*.scad` — hotswap socket models
- `things/right-test.scad` — composite test model with caps, hand overlay, etc.

**Layout variants** are managed via patch files (`4x6.patch`, `5x6.patch`, `6x6.patch`) applied to `dactyl.clj` before generation, then reverted with `git checkout`. The `create-models.sh` script automates this.

**nutsnbolts submodule** is included via `(include "../nutsnbolts/cyl_head_bolt.scad")` in the SCAD output — required for bolt models in tent and right/left assemblies.

## Customization Workflow

To customize for a different hand size or layout:
1. Edit parameters at the top of `src/dactyl_keyboard/dactyl.clj`
2. Run `lein generate` to regenerate SCAD files
3. Open `things/right-test.scad` in OpenSCAD for a quick visual preview (includes caps and hand overlay)
4. Render final STL with `openscad -o things/right.stl things/right.scad`
