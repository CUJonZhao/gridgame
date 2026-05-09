// -----------------------------------------------------------------------------
// sprite_scan_renderer.sv -- composites all sprite registers over one pixel
//
//  The module checks all sprite slots for the current VGA pixel. If multiple
//  opaque sprite pixels overlap, the highest sprite index wins. This gives the
//  software a simple priority rule: write background/lower-priority objects into
//  lower registers and foreground/higher-priority objects into higher registers.
// -----------------------------------------------------------------------------

`default_nettype none

module sprite_scan_renderer #(
    parameter int SPRITE_COUNT = 30,
    parameter int SPRITE_SIZE  = 32,
    parameter int INDEX_WIDTH  = $clog2(SPRITE_COUNT)
) (
    input  logic                               visible,
    input  logic [9:0]                         pixel_x,
    input  logic [9:0]                         pixel_y,

    input  logic [7:0]                         bg_r,
    input  logic [7:0]                         bg_g,
    input  logic [7:0]                         bg_b,

    input  logic [SPRITE_COUNT-1:0][9:0]       sprite_x,
    input  logic [SPRITE_COUNT-1:0][8:0]       sprite_y,
    input  logic [SPRITE_COUNT-1:0][4:0]       sprite_id,
    input  logic [SPRITE_COUNT-1:0]            sprite_enable,

    output logic                               sprite_hit,
    output logic [INDEX_WIDTH-1:0]             hit_sprite_index,
    output logic [7:0]                         rgb_r,
    output logic [7:0]                         rgb_g,
    output logic [7:0]                         rgb_b
);

    logic [SPRITE_COUNT-1:0][9:0] dx;
    logic [SPRITE_COUNT-1:0][9:0] dy;
    logic [SPRITE_COUNT-1:0][4:0] local_x;
    logic [SPRITE_COUNT-1:0][4:0] local_y;
    logic [SPRITE_COUNT-1:0]      inside_box;
    logic [SPRITE_COUNT-1:0]      opaque;
    logic [SPRITE_COUNT-1:0][7:0] sprite_r;
    logic [SPRITE_COUNT-1:0][7:0] sprite_g;
    logic [SPRITE_COUNT-1:0][7:0] sprite_b;

    genvar g;
    integer i;

    generate
        for (g = 0; g < SPRITE_COUNT; g = g + 1) begin : sprite_pixels
            assign dx[g] = pixel_x - sprite_x[g];
            assign dy[g] = pixel_y - {1'b0, sprite_y[g]};
            assign inside_box[g] = visible &&
                                   sprite_enable[g] &&
                                   (pixel_x >= sprite_x[g]) &&
                                   (pixel_y >= {1'b0, sprite_y[g]}) &&
                                   (dx[g] < SPRITE_SIZE) &&
                                   (dy[g] < SPRITE_SIZE);
            assign local_x[g] = inside_box[g] ? dx[g][4:0] : 5'd0;
            assign local_y[g] = inside_box[g] ? dy[g][4:0] : 5'd0;

            sprite_image_rom image_rom (
                .sprite_id (sprite_id[g]),
                .local_x   (local_x[g]),
                .local_y   (local_y[g]),
                .opaque    (opaque[g]),
                .rgb_r     (sprite_r[g]),
                .rgb_g     (sprite_g[g]),
                .rgb_b     (sprite_b[g])
            );
        end
    endgenerate

    always_comb begin
        sprite_hit       = 1'b0;
        hit_sprite_index = '0;
        rgb_r            = bg_r;
        rgb_g            = bg_g;
        rgb_b            = bg_b;

        for (i = 0; i < SPRITE_COUNT; i = i + 1) begin
            if (inside_box[i] && opaque[i]) begin
                sprite_hit       = 1'b1;
                hit_sprite_index = i;
                rgb_r            = sprite_r[i];
                rgb_g            = sprite_g[i];
                rgb_b            = sprite_b[i];
            end
        end
    end

endmodule

`default_nettype wire
