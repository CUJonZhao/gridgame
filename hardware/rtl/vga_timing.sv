// -----------------------------------------------------------------------------
// vga_timing.sv -- 640x480 @ 60 Hz VGA timing generator
//
//  Industry-standard VESA DMT timing:
//    Pixel clock        : 25.175 MHz  (provided by upper-level PLL)
//    Horizontal total   : 800 pixels  (visible 640 + FP 16 + Sync 96 + BP 48)
//    Vertical   total   : 525 lines   (visible 480 + FP 10 + Sync 2  + BP 33)
//    HSync / VSync      : active-low (the DE1-SoC ADV7123 expects active-low)
//
//  Output coordinates (pixel_x, pixel_y) are valid when `visible' is high.
//
//  This module is purely combinational on the count side -- it just runs two
//  modulo counters off the pixel clock and decodes timing windows. Reset is
//  synchronous active-high.
// -----------------------------------------------------------------------------

`default_nettype none

module vga_timing #(
    parameter int H_VISIBLE  = 640,
    parameter int H_FRONT    = 16,
    parameter int H_SYNC     = 96,
    parameter int H_BACK     = 48,
    parameter int V_VISIBLE  = 480,
    parameter int V_FRONT    = 10,
    parameter int V_SYNC     = 2,
    parameter int V_BACK     = 33
) (
    input  logic        pix_clk,    // 25.175 MHz pixel clock
    input  logic        rst,        // synchronous active-high reset

    // Raw counters (10 bits cover 800 / 525)
    output logic [9:0]  hcount,
    output logic [9:0]  vcount,

    // VGA sync / blanking
    output logic        hsync,      // active-low
    output logic        vsync,      // active-low
    output logic        visible,    // 1 inside the 640x480 visible window

    // Visible-region pixel coordinates (valid only when visible=1)
    output logic [9:0]  pixel_x,    // 0..639
    output logic [9:0]  pixel_y,    // 0..479

    // One-cycle pulses
    output logic        new_line,   // pulses high for the cycle hcount wraps
    output logic        new_frame   // pulses high for the cycle vcount wraps
);

    localparam int H_TOTAL = H_VISIBLE + H_FRONT + H_SYNC + H_BACK; // 800
    localparam int V_TOTAL = V_VISIBLE + V_FRONT + V_SYNC + V_BACK; // 525

    localparam int H_SYNC_START = H_VISIBLE + H_FRONT;             // 656
    localparam int H_SYNC_END   = H_SYNC_START + H_SYNC;           // 752
    localparam int V_SYNC_START = V_VISIBLE + V_FRONT;             // 490
    localparam int V_SYNC_END   = V_SYNC_START + V_SYNC;           // 492

    logic h_wrap;
    logic v_wrap;

    // Horizontal counter
    always_ff @(posedge pix_clk) begin
        if (rst) begin
            hcount <= '0;
        end else if (hcount == H_TOTAL - 1) begin
            hcount <= '0;
        end else begin
            hcount <= hcount + 10'd1;
        end
    end

    assign h_wrap = (hcount == H_TOTAL - 1);

    // Vertical counter advances once per horizontal wrap
    always_ff @(posedge pix_clk) begin
        if (rst) begin
            vcount <= '0;
        end else if (h_wrap) begin
            if (vcount == V_TOTAL - 1) begin
                vcount <= '0;
            end else begin
                vcount <= vcount + 10'd1;
            end
        end
    end

    assign v_wrap = h_wrap && (vcount == V_TOTAL - 1);

    // Sync and blanking decoders
    assign hsync   = ~((hcount >= H_SYNC_START) && (hcount < H_SYNC_END));
    assign vsync   = ~((vcount >= V_SYNC_START) && (vcount < V_SYNC_END));
    assign visible = (hcount < H_VISIBLE) && (vcount < V_VISIBLE);

    assign pixel_x = visible ? hcount : 10'd0;
    assign pixel_y = visible ? vcount : 10'd0;

    assign new_line  = h_wrap;
    assign new_frame = v_wrap;

endmodule

`default_nettype wire
