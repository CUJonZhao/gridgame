// -----------------------------------------------------------------------------
// vga_timing_tb.sv -- exercises vga_timing for ~3 frames and asserts:
//   * Each line is exactly 800 pix_clk cycles long
//   * Each frame is exactly 525 lines long
//   * The visible window contains exactly 640x480 cycles per frame
//   * hsync is low only during columns 656..751
//   * vsync is low only during lines   490..491
//
//  The pix_clk period below (39.682 ns) is a closer approximation of the
//  25.175 MHz VGA pixel clock than the integer-ns alternatives, but timing
//  exactness doesn't matter for this functional testbench -- only relative
//  cycle counts do.
// -----------------------------------------------------------------------------

`timescale 1ns / 1ps
`default_nettype none

module vga_timing_tb;

    logic        pix_clk = 1'b0;
    logic        rst     = 1'b1;

    logic [9:0]  hcount;
    logic [9:0]  vcount;
    logic        hsync;
    logic        vsync;
    logic        visible;
    logic [9:0]  pixel_x;
    logic [9:0]  pixel_y;
    logic        new_line;
    logic        new_frame;

    vga_timing dut (
        .pix_clk   (pix_clk),
        .rst       (rst),
        .hcount    (hcount),
        .vcount    (vcount),
        .hsync     (hsync),
        .vsync     (vsync),
        .visible   (visible),
        .pixel_x   (pixel_x),
        .pixel_y   (pixel_y),
        .new_line  (new_line),
        .new_frame (new_frame)
    );

    // 25.175 MHz -> ~39.7 ns period, but we use 40 ns for clean simulation.
    initial forever #20 pix_clk = ~pix_clk;

    int pixels_per_line;
    int lines_per_frame;
    int visible_pixels_in_frame;
    int frames_observed;

    int hsync_low_start;
    int hsync_low_end;
    int vsync_low_start;
    int vsync_low_end;

    int errors;

    initial begin
        $dumpfile("vga_timing_tb.vcd");
        $dumpvars(0, vga_timing_tb);

        errors                  = 0;
        pixels_per_line         = 0;
        lines_per_frame         = 0;
        visible_pixels_in_frame = 0;
        frames_observed         = 0;
        hsync_low_start         = -1;
        hsync_low_end           = -1;
        vsync_low_start         = -1;
        vsync_low_end           = -1;

        // Hold reset for 4 cycles, then release.
        repeat (4) @(posedge pix_clk);
        rst <= 1'b0;
        @(posedge pix_clk);

        // Wait for the first new_frame to align our counts to a frame boundary.
        do @(posedge pix_clk); while (!new_frame);

        // Now collect statistics over exactly 2 full frames.
        while (frames_observed < 2) begin
            @(posedge pix_clk);

            pixels_per_line = pixels_per_line + 1;

            if (visible) begin
                visible_pixels_in_frame = visible_pixels_in_frame + 1;
            end

            // First low edge of hsync within this frame.
            if (hsync == 1'b0 && hsync_low_start == -1) begin
                hsync_low_start = hcount;
            end
            if (hsync == 1'b0) begin
                hsync_low_end = hcount;
            end
            // First low edge of vsync within this frame.
            if (vsync == 1'b0 && vsync_low_start == -1) begin
                vsync_low_start = vcount;
            end
            if (vsync == 1'b0) begin
                vsync_low_end = vcount;
            end

            if (new_line) begin
                if (pixels_per_line != 800) begin
                    $display("FAIL: line %0d had %0d pixels (expected 800)",
                             vcount, pixels_per_line);
                    errors = errors + 1;
                end
                pixels_per_line = 0;
                lines_per_frame = lines_per_frame + 1;
            end

            if (new_frame) begin
                if (lines_per_frame != 525) begin
                    $display("FAIL: frame had %0d lines (expected 525)",
                             lines_per_frame);
                    errors = errors + 1;
                end
                if (visible_pixels_in_frame != 640 * 480) begin
                    $display("FAIL: frame had %0d visible pixels (expected %0d)",
                             visible_pixels_in_frame, 640 * 480);
                    errors = errors + 1;
                end
                if (hsync_low_start != 656 || hsync_low_end != 751) begin
                    $display("FAIL: hsync window = [%0d..%0d], expected [656..751]",
                             hsync_low_start, hsync_low_end);
                    errors = errors + 1;
                end
                if (vsync_low_start != 490 || vsync_low_end != 491) begin
                    $display("FAIL: vsync window = [%0d..%0d], expected [490..491]",
                             vsync_low_start, vsync_low_end);
                    errors = errors + 1;
                end

                lines_per_frame         = 0;
                visible_pixels_in_frame = 0;
                hsync_low_start         = -1;
                hsync_low_end           = -1;
                vsync_low_start         = -1;
                vsync_low_end           = -1;
                frames_observed         = frames_observed + 1;
            end
        end

        if (errors == 0) begin
            $display("PASS: %0d frames verified", frames_observed);
        end else begin
            $display("FAIL: %0d total errors", errors);
        end

        $finish;
    end

endmodule

`default_nettype wire
