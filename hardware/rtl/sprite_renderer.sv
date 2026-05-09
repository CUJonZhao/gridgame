// -----------------------------------------------------------------------------
// sprite_renderer.sv -- overlays one 32x32 sprite over an RGB background pixel
//
//  This compositor is deliberately single-sprite. A top-level video pipeline can
//  chain it for fixed priority, or a later scanner can iterate over the 30
//  sprite registers and feed the winning sprite into this module.
// -----------------------------------------------------------------------------

`default_nettype none

module sprite_renderer #(
    parameter int SPRITE_SIZE = 32
) (
    input  logic       visible,
    input  logic [9:0] pixel_x,
    input  logic [9:0] pixel_y,

    input  logic [7:0] bg_r,
    input  logic [7:0] bg_g,
    input  logic [7:0] bg_b,

    input  logic       sprite_enable,
    input  logic [9:0] sprite_x,
    input  logic [8:0] sprite_y,
    input  logic [4:0] sprite_id,

    output logic       sprite_hit,
    output logic [4:0] sprite_local_x,
    output logic [4:0] sprite_local_y,
    output logic [7:0] rgb_r,
    output logic [7:0] rgb_g,
    output logic [7:0] rgb_b
);

    logic [9:0] sprite_y_ext;
    logic [9:0] dx;
    logic [9:0] dy;
    logic       inside_sprite_box;
    logic       sprite_opaque;
    logic [7:0] sprite_r;
    logic [7:0] sprite_g;
    logic [7:0] sprite_b;

    assign sprite_y_ext       = {1'b0, sprite_y};
    assign dx                 = pixel_x - sprite_x;
    assign dy                 = pixel_y - sprite_y_ext;
    assign inside_sprite_box  = visible &&
                                sprite_enable &&
                                (pixel_x >= sprite_x) &&
                                (pixel_y >= sprite_y_ext) &&
                                (dx < SPRITE_SIZE) &&
                                (dy < SPRITE_SIZE);
    assign sprite_local_x     = inside_sprite_box ? dx[4:0] : 5'd0;
    assign sprite_local_y     = inside_sprite_box ? dy[4:0] : 5'd0;
    assign sprite_hit         = inside_sprite_box && sprite_opaque;

    sprite_image_rom image_rom (
        .sprite_id (sprite_id),
        .local_x   (sprite_local_x),
        .local_y   (sprite_local_y),
        .opaque    (sprite_opaque),
        .rgb_r     (sprite_r),
        .rgb_g     (sprite_g),
        .rgb_b     (sprite_b)
    );

    always_comb begin
        if (sprite_hit) begin
            rgb_r = sprite_r;
            rgb_g = sprite_g;
            rgb_b = sprite_b;
        end else begin
            rgb_r = bg_r;
            rgb_g = bg_g;
            rgb_b = bg_b;
        end
    end

endmodule

`default_nettype wire
