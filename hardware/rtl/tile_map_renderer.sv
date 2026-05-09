// -----------------------------------------------------------------------------
// tile_map_renderer.sv -- direct background renderer for GridBrawl tiles
//
//  Converts VGA pixel coordinates into a row-major Tile Map RAM address and
//  colors the pixel according to the tile id returned from the RAM. This is the
//  first video-layer building block; sprite compositing can be placed after this
//  module and override rgb_* where sprite pixels are opaque.
// -----------------------------------------------------------------------------

`default_nettype none

module tile_map_renderer #(
    parameter int SCREEN_WIDTH  = 640,
    parameter int SCREEN_HEIGHT = 480,
    parameter int MAP_COLS      = 20,
    parameter int MAP_ROWS      = 15,
    parameter int TILE_SIZE     = 32,
    parameter int ADDR_WIDTH    = $clog2(MAP_COLS * MAP_ROWS)
) (
    input  logic                  visible,
    input  logic [9:0]            pixel_x,
    input  logic [9:0]            pixel_y,
    input  logic [2:0]            tile_id,

    output logic [ADDR_WIDTH-1:0] tile_address,
    output logic [4:0]            tile_local_x,
    output logic [4:0]            tile_local_y,
    output logic [7:0]            rgb_r,
    output logic [7:0]            rgb_g,
    output logic [7:0]            rgb_b
);

    localparam logic [2:0] TILE_EMPTY     = 3'd0;
    localparam logic [2:0] TILE_BLUE      = 3'd1;
    localparam logic [2:0] TILE_PINK      = 3'd2;
    localparam logic [2:0] TILE_YELLOW    = 3'd3;
    localparam logic [2:0] TILE_HARD_WALL = 3'd4;
    localparam logic [2:0] TILE_SOFT_WALL = 3'd5;

    logic [4:0] tile_col;
    logic [3:0] tile_row;
    logic       in_bounds;
    logic       grid_line;
    logic       brick_line;

    always_comb begin
        tile_col     = pixel_x[9:5];
        tile_row     = pixel_y[8:5];
        tile_local_x = pixel_x[4:0];
        tile_local_y = pixel_y[4:0];
        in_bounds    = visible &&
                       (pixel_x < SCREEN_WIDTH) &&
                       (pixel_y < SCREEN_HEIGHT) &&
                       (tile_col < MAP_COLS) &&
                       (tile_row < MAP_ROWS);

        if (in_bounds) begin
            tile_address = (tile_row * MAP_COLS) + tile_col;
        end else begin
            tile_address = '0;
        end
    end

    always_comb begin
        grid_line  = (tile_local_x == 5'd0) || (tile_local_y == 5'd0);
        brick_line = (tile_local_y[3:0] == 4'd0) ||
                     ((tile_local_x[4:1] == 4'd0) && tile_local_y[4]);

        if (!in_bounds) begin
            rgb_r = 8'h00;
            rgb_g = 8'h00;
            rgb_b = 8'h00;
        end else begin
            unique case (tile_id)
                TILE_EMPTY: begin
                    rgb_r = grid_line ? 8'h18 : 8'h10;
                    rgb_g = grid_line ? 8'h1C : 8'h14;
                    rgb_b = grid_line ? 8'h22 : 8'h18;
                end
                TILE_BLUE: begin
                    rgb_r = grid_line ? 8'h24 : 8'h30;
                    rgb_g = grid_line ? 8'h6A : 8'h8C;
                    rgb_b = grid_line ? 8'hB8 : 8'hE8;
                end
                TILE_PINK: begin
                    rgb_r = grid_line ? 8'hA8 : 8'hEA;
                    rgb_g = grid_line ? 8'h3A : 8'h62;
                    rgb_b = grid_line ? 8'h72 : 8'hA0;
                end
                TILE_YELLOW: begin
                    rgb_r = grid_line ? 8'hB8 : 8'hF4;
                    rgb_g = grid_line ? 8'h8D : 8'hC7;
                    rgb_b = grid_line ? 8'h24 : 8'h46;
                end
                TILE_HARD_WALL: begin
                    rgb_r = brick_line ? 8'h42 : 8'h72;
                    rgb_g = brick_line ? 8'h46 : 8'h78;
                    rgb_b = brick_line ? 8'h4D : 8'h82;
                end
                TILE_SOFT_WALL: begin
                    rgb_r = brick_line ? 8'h72 : 8'hB8;
                    rgb_g = brick_line ? 8'h42 : 8'h76;
                    rgb_b = brick_line ? 8'h20 : 8'h35;
                end
                default: begin
                    rgb_r = 8'hFF;
                    rgb_g = 8'h00;
                    rgb_b = 8'hFF;
                end
            endcase
        end
    end

endmodule

`default_nettype wire
