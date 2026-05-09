// -----------------------------------------------------------------------------
// sprite_renderer_tb.sv -- verifies one-sprite RGB compositing behavior
// -----------------------------------------------------------------------------

`timescale 1ns / 1ps
`default_nettype none

module sprite_renderer_tb;

    logic       visible;
    logic [9:0] pixel_x;
    logic [9:0] pixel_y;
    logic [7:0] bg_r;
    logic [7:0] bg_g;
    logic [7:0] bg_b;
    logic       sprite_enable;
    logic [9:0] sprite_x;
    logic [8:0] sprite_y;
    logic [4:0] sprite_id;
    logic       sprite_hit;
    logic [4:0] sprite_local_x;
    logic [4:0] sprite_local_y;
    logic [7:0] rgb_r;
    logic [7:0] rgb_g;
    logic [7:0] rgb_b;

    int errors;

    sprite_renderer dut (
        .visible        (visible),
        .pixel_x        (pixel_x),
        .pixel_y        (pixel_y),
        .bg_r           (bg_r),
        .bg_g           (bg_g),
        .bg_b           (bg_b),
        .sprite_enable  (sprite_enable),
        .sprite_x       (sprite_x),
        .sprite_y       (sprite_y),
        .sprite_id      (sprite_id),
        .sprite_hit     (sprite_hit),
        .sprite_local_x (sprite_local_x),
        .sprite_local_y (sprite_local_y),
        .rgb_r          (rgb_r),
        .rgb_g          (rgb_g),
        .rgb_b          (rgb_b)
    );

    task automatic expect_pixel(
        input string label_text,
        input bit expected_hit,
        input logic [4:0] expected_local_x,
        input logic [4:0] expected_local_y,
        input logic [23:0] expected_rgb
    );
        begin
            #1;
            if (sprite_hit !== expected_hit) begin
                $display("FAIL: %s hit = %0b, expected %0b",
                         label_text, sprite_hit, expected_hit);
                errors++;
            end
            if (sprite_local_x !== expected_local_x) begin
                $display("FAIL: %s local_x = %0d, expected %0d",
                         label_text, sprite_local_x, expected_local_x);
                errors++;
            end
            if (sprite_local_y !== expected_local_y) begin
                $display("FAIL: %s local_y = %0d, expected %0d",
                         label_text, sprite_local_y, expected_local_y);
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
        $dumpfile("sprite_renderer_tb.vcd");
        $dumpvars(0, sprite_renderer_tb);

        errors        = 0;
        visible       = 1'b1;
        pixel_x       = 10'd102;
        pixel_y       = 10'd80;
        bg_r          = 8'h11;
        bg_g          = 8'h22;
        bg_b          = 8'h33;
        sprite_enable = 1'b1;
        sprite_x      = 10'd96;
        sprite_y      = 9'd76;
        sprite_id     = 5'd0;

        // Local (6,4) is on player outline, so it should overlay background.
        expect_pixel("player outline", 1'b1, 5'd6, 5'd4, 24'h18181c);

        // Local (0,0) is inside the sprite bounding box but transparent art.
        pixel_x = 10'd96;
        pixel_y = 10'd76;
        expect_pixel("transparent corner", 1'b0, 5'd0, 5'd0, 24'h112233);

        // Disabled sprite must pass the background through.
        sprite_enable = 1'b0;
        pixel_x = 10'd104;
        pixel_y = 10'd84;
        expect_pixel("disabled sprite", 1'b0, 5'd0, 5'd0, 24'h112233);

        // Explosion core should be opaque and bright.
        sprite_enable = 1'b1;
        sprite_id = 5'd10;
        pixel_x = 10'd112;
        pixel_y = 10'd92;
        expect_pixel("explosion core", 1'b1, 5'd16, 5'd16, 24'hffe872);

        // Outside the 32x32 box should also pass through.
        pixel_x = 10'd128;
        pixel_y = 10'd92;
        expect_pixel("outside sprite", 1'b0, 5'd0, 5'd0, 24'h112233);

        if (errors == 0) begin
            $display("PASS: sprite renderer verified");
        end else begin
            $display("FAIL: %0d sprite renderer errors", errors);
        end

        $finish;
    end

endmodule

`default_nettype wire
