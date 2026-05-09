// -----------------------------------------------------------------------------
// tile_map_tb.sv -- checks Tile Map RAM and tile renderer address/color behavior
// -----------------------------------------------------------------------------

`timescale 1ns / 1ps
`default_nettype none

module tile_map_tb;

    localparam int MAP_COLS   = 20;
    localparam int MAP_ROWS   = 15;
    localparam int ADDR_WIDTH = $clog2(MAP_COLS * MAP_ROWS);

    logic                  clk = 1'b0;
    logic                  rst = 1'b1;
    logic                  avs_write;
    logic                  avs_read;
    logic [ADDR_WIDTH-1:0] avs_address;
    logic [7:0]            avs_writedata;
    logic [7:0]            avs_readdata;
    logic [ADDR_WIDTH-1:0] render_address;
    logic [2:0]            render_tile_id;

    logic                  visible;
    logic [9:0]            pixel_x;
    logic [9:0]            pixel_y;
    logic [ADDR_WIDTH-1:0] renderer_address;
    logic [4:0]            tile_local_x;
    logic [4:0]            tile_local_y;
    logic [7:0]            rgb_r;
    logic [7:0]            rgb_g;
    logic [7:0]            rgb_b;

    int errors;

    tile_map_ram #(
        .MAP_COLS   (MAP_COLS),
        .MAP_ROWS   (MAP_ROWS),
        .ADDR_WIDTH (ADDR_WIDTH)
    ) ram (
        .clk             (clk),
        .rst             (rst),
        .avs_write       (avs_write),
        .avs_read        (avs_read),
        .avs_address     (avs_address),
        .avs_writedata   (avs_writedata),
        .avs_readdata    (avs_readdata),
        .render_address  (render_address),
        .render_tile_id  (render_tile_id)
    );

    tile_map_renderer #(
        .MAP_COLS   (MAP_COLS),
        .MAP_ROWS   (MAP_ROWS),
        .ADDR_WIDTH (ADDR_WIDTH)
    ) renderer (
        .visible       (visible),
        .pixel_x       (pixel_x),
        .pixel_y       (pixel_y),
        .tile_id       (render_tile_id),
        .tile_address  (renderer_address),
        .tile_local_x  (tile_local_x),
        .tile_local_y  (tile_local_y),
        .rgb_r         (rgb_r),
        .rgb_g         (rgb_g),
        .rgb_b         (rgb_b)
    );

    initial forever #5 clk = ~clk;

    task automatic write_tile(input int addr, input logic [7:0] value);
        begin
            @(posedge clk);
            avs_write     <= 1'b1;
            avs_read      <= 1'b0;
            avs_address   <= addr[ADDR_WIDTH-1:0];
            avs_writedata <= value;
            @(posedge clk);
            avs_write     <= 1'b0;
        end
    endtask

    task automatic read_tile(input int addr, input logic [7:0] expected);
        begin
            @(posedge clk);
            avs_read    <= 1'b1;
            avs_write   <= 1'b0;
            avs_address <= addr[ADDR_WIDTH-1:0];
            @(posedge clk);
            avs_read    <= 1'b0;
            #1;
            if (avs_readdata !== expected) begin
                $display("FAIL: RAM read addr %0d = 0x%02h, expected 0x%02h",
                         addr, avs_readdata, expected);
                errors++;
            end
        end
    endtask

    task automatic check_pixel(
        input int x,
        input int y,
        input logic [2:0] expected_tile,
        input int expected_addr
    );
        begin
            visible = 1'b1;
            pixel_x = x[9:0];
            pixel_y = y[9:0];
            #1;
            render_address = renderer_address;
            #1;
            if (renderer_address !== expected_addr[ADDR_WIDTH-1:0]) begin
                $display("FAIL: pixel (%0d,%0d) address = %0d, expected %0d",
                         x, y, renderer_address, expected_addr);
                errors++;
            end
            if (render_tile_id !== expected_tile) begin
                $display("FAIL: pixel (%0d,%0d) tile = %0d, expected %0d",
                         x, y, render_tile_id, expected_tile);
                errors++;
            end
        end
    endtask

    initial begin
        $dumpfile("tile_map_tb.vcd");
        $dumpvars(0, tile_map_tb);

        errors        = 0;
        avs_write     = 1'b0;
        avs_read      = 1'b0;
        avs_address   = '0;
        avs_writedata = '0;
        render_address = '0;
        visible       = 1'b0;
        pixel_x       = '0;
        pixel_y       = '0;

        repeat (3) @(posedge clk);
        rst <= 1'b0;

        write_tile(0,   8'h01); // top-left blue
        write_tile(21,  8'h03); // row 1, col 1 yellow
        write_tile(299, 8'h05); // bottom-right soft wall

        read_tile(0, 8'h01);
        read_tile(21, 8'h03);
        read_tile(299, 8'h05);

        check_pixel(0,   0,   3'd1, 0);
        check_pixel(32,  32,  3'd3, 21);
        check_pixel(639, 479, 3'd5, 299);

        visible = 1'b0;
        pixel_x = 10'd100;
        pixel_y = 10'd100;
        #1;
        if ({rgb_r, rgb_g, rgb_b} !== 24'h000000) begin
            $display("FAIL: invisible pixel produced RGB %02h%02h%02h",
                     rgb_r, rgb_g, rgb_b);
            errors++;
        end

        if (errors == 0) begin
            $display("PASS: tile map RAM and renderer verified");
        end else begin
            $display("FAIL: %0d tile map errors", errors);
        end

        $finish;
    end

endmodule

`default_nettype wire
