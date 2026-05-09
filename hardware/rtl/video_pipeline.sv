// -----------------------------------------------------------------------------
// video_pipeline.sv -- first complete GridBrawl VGA video pipeline
//
//  This module ties together:
//    * VGA timing
//    * HPS-writable tile map RAM
//    * background tile renderer
//    * HPS-writable sprite registers
//    * 30-sprite scan compositor
//
//  The HPS/Avalon-facing ports here are intentionally simple single-clock
//  register-style interfaces. A DE1-SoC top level can wrap these ports with
//  Platform Designer/Avalon glue and feed pix_clk from the VGA PLL.
// -----------------------------------------------------------------------------

`default_nettype none

module video_pipeline #(
    parameter int SCREEN_WIDTH  = 640,
    parameter int SCREEN_HEIGHT = 480,
    parameter int MAP_COLS      = 20,
    parameter int MAP_ROWS      = 15,
    parameter int SPRITE_COUNT  = 30,
    parameter int TILE_ADDR_WIDTH   = $clog2(MAP_COLS * MAP_ROWS),
    parameter int SPRITE_ADDR_WIDTH = $clog2(SPRITE_COUNT * 4),
    parameter int SPRITE_INDEX_WIDTH = $clog2(SPRITE_COUNT)
) (
    input  logic                         pix_clk,
    input  logic                         rst,

    // Tile Map RAM register interface, byte offsets 0x000..0x12B.
    input  logic                         tile_write,
    input  logic                         tile_read,
    input  logic [TILE_ADDR_WIDTH-1:0]   tile_address,
    input  logic [7:0]                   tile_writedata,
    output logic [7:0]                   tile_readdata,

    // Sprite register interface, byte offsets 0x000..0x077.
    input  logic                         sprite_write,
    input  logic                         sprite_read,
    input  logic [SPRITE_ADDR_WIDTH-1:0] sprite_address,
    input  logic [31:0]                  sprite_writedata,
    output logic [31:0]                  sprite_readdata,

    // VGA outputs.
    output logic                         vga_hsync,
    output logic                         vga_vsync,
    output logic                         vga_visible,
    output logic [7:0]                   vga_r,
    output logic [7:0]                   vga_g,
    output logic [7:0]                   vga_b,

    // Useful debug/status taps.
    output logic [9:0]                   pixel_x,
    output logic [9:0]                   pixel_y,
    output logic                         new_frame,
    output logic [TILE_ADDR_WIDTH-1:0]   debug_tile_address,
    output logic [2:0]                   debug_tile_id,
    output logic                         debug_sprite_hit,
    output logic [SPRITE_INDEX_WIDTH-1:0] debug_hit_sprite_index
);

    logic [9:0] hcount;
    logic [9:0] vcount;
    logic       new_line;

    logic [TILE_ADDR_WIDTH-1:0] render_tile_address;
    logic [2:0]                 render_tile_id;
    logic [4:0]                 tile_local_x;
    logic [4:0]                 tile_local_y;
    logic [7:0]                 tile_r;
    logic [7:0]                 tile_g;
    logic [7:0]                 tile_b;

    logic [SPRITE_COUNT-1:0][9:0] sprite_x;
    logic [SPRITE_COUNT-1:0][8:0] sprite_y;
    logic [SPRITE_COUNT-1:0][4:0] sprite_id;
    logic [SPRITE_COUNT-1:0]      sprite_enable;

    logic [9:0] unused_render_x;
    logic [8:0] unused_render_y;
    logic [4:0] unused_render_sprite_id;
    logic       unused_render_enable;

    vga_timing timing (
        .pix_clk   (pix_clk),
        .rst       (rst),
        .hcount    (hcount),
        .vcount    (vcount),
        .hsync     (vga_hsync),
        .vsync     (vga_vsync),
        .visible   (vga_visible),
        .pixel_x   (pixel_x),
        .pixel_y   (pixel_y),
        .new_line  (new_line),
        .new_frame (new_frame)
    );

    tile_map_ram #(
        .MAP_COLS   (MAP_COLS),
        .MAP_ROWS   (MAP_ROWS),
        .ADDR_WIDTH (TILE_ADDR_WIDTH)
    ) tile_ram (
        .clk            (pix_clk),
        .rst            (rst),
        .avs_write      (tile_write),
        .avs_read       (tile_read),
        .avs_address    (tile_address),
        .avs_writedata  (tile_writedata),
        .avs_readdata   (tile_readdata),
        .render_address (render_tile_address),
        .render_tile_id (render_tile_id)
    );

    tile_map_renderer #(
        .SCREEN_WIDTH  (SCREEN_WIDTH),
        .SCREEN_HEIGHT (SCREEN_HEIGHT),
        .MAP_COLS      (MAP_COLS),
        .MAP_ROWS      (MAP_ROWS),
        .ADDR_WIDTH    (TILE_ADDR_WIDTH)
    ) tile_renderer (
        .visible       (vga_visible),
        .pixel_x       (pixel_x),
        .pixel_y       (pixel_y),
        .tile_id       (render_tile_id),
        .tile_address  (render_tile_address),
        .tile_local_x  (tile_local_x),
        .tile_local_y  (tile_local_y),
        .rgb_r         (tile_r),
        .rgb_g         (tile_g),
        .rgb_b         (tile_b)
    );

    sprite_registers #(
        .SPRITE_COUNT (SPRITE_COUNT),
        .ADDR_WIDTH   (SPRITE_ADDR_WIDTH),
        .INDEX_WIDTH  (SPRITE_INDEX_WIDTH)
    ) sprite_regs (
        .clk              (pix_clk),
        .rst              (rst),
        .avs_write        (sprite_write),
        .avs_read         (sprite_read),
        .avs_address      (sprite_address),
        .avs_writedata    (sprite_writedata),
        .avs_readdata     (sprite_readdata),
        .render_index     ('0),
        .render_x         (unused_render_x),
        .render_y         (unused_render_y),
        .render_sprite_id (unused_render_sprite_id),
        .render_enable    (unused_render_enable),
        .sprite_x         (sprite_x),
        .sprite_y         (sprite_y),
        .sprite_id        (sprite_id),
        .sprite_enable    (sprite_enable)
    );

    sprite_scan_renderer #(
        .SPRITE_COUNT (SPRITE_COUNT),
        .INDEX_WIDTH  (SPRITE_INDEX_WIDTH)
    ) sprites (
        .visible          (vga_visible),
        .pixel_x          (pixel_x),
        .pixel_y          (pixel_y),
        .bg_r             (tile_r),
        .bg_g             (tile_g),
        .bg_b             (tile_b),
        .sprite_x         (sprite_x),
        .sprite_y         (sprite_y),
        .sprite_id        (sprite_id),
        .sprite_enable    (sprite_enable),
        .sprite_hit       (debug_sprite_hit),
        .hit_sprite_index (debug_hit_sprite_index),
        .rgb_r            (vga_r),
        .rgb_g            (vga_g),
        .rgb_b            (vga_b)
    );

    assign debug_tile_address = render_tile_address;
    assign debug_tile_id      = render_tile_id;

endmodule

`default_nettype wire
