# GridBrawl Hardware

This directory contains the FPGA-side display and audio building blocks for the
DE1-SoC implementation.

## Current Modules

- `rtl/vga_timing.sv`: 640x480@60 Hz VGA timing generator. The top level must
  provide a 25.175 MHz pixel clock.
- `rtl/tile_map_ram.sv`: 300-byte row-major tile map RAM. Each byte maps to one
  32x32 tile; bits `[2:0]` hold the tile id from `game/game.h`.
- `rtl/tile_map_renderer.sv`: converts visible VGA coordinates into a tile-map
  address and emits background RGB values for the returned tile id.
- `rtl/sprite_registers.sv`: 30 memory-mapped 32-bit sprite registers for
  dynamic objects such as players, bombs, explosions, and UI glyphs.
- `rtl/sprite_image_rom.sv`: procedural placeholder sprite ROM for the 30
  sprite IDs defined in `game/game.h`.
- `rtl/sprite_renderer.sv`: one-sprite compositor that overlays an opaque
  32x32 sprite pixel on top of a background RGB pixel.
- `rtl/sprite_scan_renderer.sv`: full 30-sprite compositor. Higher sprite
  register indices have higher priority when opaque sprite pixels overlap.
- `rtl/video_pipeline.sv`: integrated VGA path combining timing, tile RAM,
  tile rendering, sprite registers, and 30-sprite compositing.

## Tile Map Layout

The display is a 20x15 grid over a 640x480 screen, so each tile is 32x32
pixels. Tile addresses are row-major:

```text
address = row * 20 + col
row     = pixel_y / 32
col     = pixel_x / 32
```

Tile ids match the software enum:

```text
0 empty
1 blue
2 pink
3 yellow
4 hard wall
5 soft wall
```

## Sprite Register Layout

Sprite registers are addressed as 32-bit words on byte offsets `0x000..0x077`:

```text
offset = sprite_index * 4
```

Each register matches the design document:

```text
bits [9:0]   x position
bits [18:10] y position
bits [23:19] sprite id
bit  [24]    enable
bits [31:25] reserved, read as 0
```

## Sprite Rendering

`sprite_renderer.sv` consumes one decoded sprite register at a time:

```text
pixel_x, pixel_y + background RGB
sprite_x, sprite_y, sprite_id, enable
    -> composited RGB
```

The current `sprite_image_rom.sv` uses simple procedural placeholder art. It is
meant to keep the hardware pipeline testable before final bitmap or MIF assets
are available.

`sprite_scan_renderer.sv` consumes the parallel decoded outputs from
`sprite_registers.sv` and checks all 30 sprite slots for the current pixel. If
multiple enabled sprites overlap, the highest register index wins.

## Integrated Video Pipeline

`video_pipeline.sv` is the first complete VGA rendering block. It exposes two
simple memory-mapped write/read regions:

```text
tile_*   -> Tile Map RAM offsets 0x000..0x12B
sprite_* -> Sprite Register offsets 0x000..0x077
```

The top level must provide a 25.175 MHz `pix_clk`. The pipeline outputs
`vga_hsync`, `vga_vsync`, `vga_visible`, and 8-bit RGB channels.

## Simulation

From `hardware/sim`:

```sh
make
make tile_map
make sprite_registers
make sprite_renderer
make sprite_scan_renderer
make video_pipeline
make clean
```

The simulation Makefile uses Icarus Verilog by default and can be extended with
additional module tests as the sprite registers, sprite renderer, and top-level
video pipeline are added.
