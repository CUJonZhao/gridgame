// -----------------------------------------------------------------------------
// sprite_scan_renderer_tb.sv -- verifies 30-sprite scan/composite priority
// -----------------------------------------------------------------------------

`timescale 1ns / 1ps
`default_nettype none

module sprite_scan_renderer_tb;

    localparam int SPRITE_COUNT = 30;
    localparam int INDEX_WIDTH  = $clog2(SPRITE_COUNT);

    logic                               visible;
    logic [9:0]                         pixel_x;
    logic [9:0]                         pixel_y;
    logic [7:0]                         bg_r;
    logic [7:0]                         bg_g;
    logic [7:0]                         bg_b;
    logic [SPRITE_COUNT-1:0][9:0]       sprite_x;
    logic [SPRITE_COUNT-1:0][8:0]       sprite_y;
    logic [SPRITE_COUNT-1:0][4:0]       sprite_id;
    logic [SPRITE_COUNT-1:0]            sprite_enable;
    logic                               sprite_hit;
    logic [INDEX_WIDTH-1:0]             hit_sprite_index;
    logic [7:0]                         rgb_r;
    logic [7:0]                         rgb_g;
    logic [7:0]                         rgb_b;

    int errors;

    sprite_scan_renderer #(
        .SPRITE_COUNT (SPRITE_COUNT),
        .INDEX_WIDTH  (INDEX_WIDTH)
    ) dut (
        .visible          (visible),
        .pixel_x          (pixel_x),
        .pixel_y          (pixel_y),
        .bg_r             (bg_r),
        .bg_g             (bg_g),
        .bg_b             (bg_b),
        .sprite_x         (sprite_x),
        .sprite_y         (sprite_y),
        .sprite_id        (sprite_id),
        .sprite_enable    (sprite_enable),
        .sprite_hit       (sprite_hit),
        .hit_sprite_index (hit_sprite_index),
        .rgb_r            (rgb_r),
        .rgb_g            (rgb_g),
        .rgb_b            (rgb_b)
    );

    task automatic expect_result(
        input string label_text,
        input bit expected_hit,
        input int expected_index,
        input logic [23:0] expected_rgb
    );
        begin
            #1;
            if (sprite_hit !== expected_hit) begin
                $display("FAIL: %s hit = %0b, expected %0b",
                         label_text, sprite_hit, expected_hit);
                errors++;
            end
            if (hit_sprite_index !== expected_index) begin
                $display("FAIL: %s index = %0d, expected %0d",
                         label_text, hit_sprite_index, expected_index);
                errors++;
            end
            if ({rgb_r, rgb_g, rgb_b} !== expected_rgb) begin
                $display("FAIL: %s RGB = %06h, expected %06h",
                         label_text, {rgb_r, rgb_g, rgb_b}, expected_rgb);
                errors++;
            end
        end
    endtask

    initial begin
        $dumpfile("sprite_scan_renderer_tb.vcd");
        $dumpvars(0, sprite_scan_renderer_tb);

        errors        = 0;
        visible       = 1'b1;
        pixel_x       = 10'd102;
        pixel_y       = 10'd80;
        bg_r          = 8'h11;
        bg_g          = 8'h22;
        bg_b          = 8'h33;
        sprite_x      = '0;
        sprite_y      = '0;
        sprite_id     = '0;
        sprite_enable = '0;

        // One enabled sprite: player 1 outline at local (6,4).
        sprite_x[0]      = 10'd96;
        sprite_y[0]      = 9'd76;
        sprite_id[0]     = 5'd0;
        sprite_enable[0] = 1'b1;
        expect_result("single sprite", 1'b1, 0, 24'h18181c);

        // Higher index wins when both sprites cover the same pixel.
        sprite_x[5]      = 10'd96;
        sprite_y[5]      = 9'd76;
        sprite_id[5]     = 5'd4;
        sprite_enable[5] = 1'b1;
        pixel_x          = 10'd110;
        pixel_y          = 10'd84;
        expect_result("priority overlap", 1'b1, 5, 24'hea62a0);

        // Transparent art inside a sprite box should not block lower sprites.
        sprite_x[8]      = 10'd96;
        sprite_y[8]      = 9'd76;
        sprite_id[8]     = 5'd8;
        sprite_enable[8] = 1'b1;
        pixel_x          = 10'd96;
        pixel_y          = 10'd76;
        expect_result("transparent upper sprite", 1'b0, 0, 24'h112233);

        // Explosion core from a high index should override lower sprites.
        sprite_x[12]      = 10'd96;
        sprite_y[12]      = 9'd76;
        sprite_id[12]     = 5'd10;
        sprite_enable[12] = 1'b1;
        pixel_x           = 10'd112;
        pixel_y           = 10'd92;
        expect_result("explosion priority", 1'b1, 12, 24'hffe872);

        visible = 1'b0;
        expect_result("not visible", 1'b0, 0, 24'h112233);

        if (errors == 0) begin
            $display("PASS: sprite scan renderer verified");
        end else begin
            $display("FAIL: %0d sprite scan renderer errors", errors);
        end

        $finish;
    end

endmodule

`default_nettype wire
