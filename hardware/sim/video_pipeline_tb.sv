// -----------------------------------------------------------------------------
// video_pipeline_tb.sv -- smoke-test complete VGA video pipeline integration
// -----------------------------------------------------------------------------

`timescale 1ns / 1ps
`default_nettype none

module video_pipeline_tb;

    localparam int TILE_ADDR_WIDTH   = $clog2(20 * 15);
    localparam int SPRITE_ADDR_WIDTH = $clog2(30 * 4);
    localparam int SPRITE_INDEX_WIDTH = $clog2(30);

    logic                         pix_clk = 1'b0;
    logic                         rst = 1'b1;
    logic                         tile_write;
    logic                         tile_read;
    logic [TILE_ADDR_WIDTH-1:0]   tile_address;
    logic [7:0]                   tile_writedata;
    logic [7:0]                   tile_readdata;
    logic                         sprite_write;
    logic                         sprite_read;
    logic [SPRITE_ADDR_WIDTH-1:0] sprite_address;
    logic [31:0]                  sprite_writedata;
    logic [31:0]                  sprite_readdata;
    logic                         vga_hsync;
    logic                         vga_vsync;
    logic                         vga_visible;
    logic [7:0]                   vga_r;
    logic [7:0]                   vga_g;
    logic [7:0]                   vga_b;
    logic [9:0]                   pixel_x;
    logic [9:0]                   pixel_y;
    logic                         new_frame;
    logic [TILE_ADDR_WIDTH-1:0]   debug_tile_address;
    logic [2:0]                   debug_tile_id;
    logic                         debug_sprite_hit;
    logic [SPRITE_INDEX_WIDTH-1:0] debug_hit_sprite_index;

    int errors;

    video_pipeline dut (
        .pix_clk                (pix_clk),
        .rst                    (rst),
        .tile_write             (tile_write),
        .tile_read              (tile_read),
        .tile_address           (tile_address),
        .tile_writedata         (tile_writedata),
        .tile_readdata          (tile_readdata),
        .sprite_write           (sprite_write),
        .sprite_read            (sprite_read),
        .sprite_address         (sprite_address),
        .sprite_writedata       (sprite_writedata),
        .sprite_readdata        (sprite_readdata),
        .vga_hsync              (vga_hsync),
        .vga_vsync              (vga_vsync),
        .vga_visible            (vga_visible),
        .vga_r                  (vga_r),
        .vga_g                  (vga_g),
        .vga_b                  (vga_b),
        .pixel_x                (pixel_x),
        .pixel_y                (pixel_y),
        .new_frame              (new_frame),
        .debug_tile_address     (debug_tile_address),
        .debug_tile_id          (debug_tile_id),
        .debug_sprite_hit       (debug_sprite_hit),
        .debug_hit_sprite_index (debug_hit_sprite_index)
    );

    initial forever #5 pix_clk = ~pix_clk;

    function automatic logic [31:0] pack_sprite(
        input int x,
        input int y,
        input int sprite_id,
        input bit enable
    );
        begin
            pack_sprite = {7'd0, enable, sprite_id[4:0], y[8:0], x[9:0]};
        end
    endfunction

    task automatic write_tile(input int addr, input logic [7:0] value);
        begin
            @(posedge pix_clk);
            tile_write     <= 1'b1;
            tile_read      <= 1'b0;
            tile_address   <= addr[TILE_ADDR_WIDTH-1:0];
            tile_writedata <= value;
            @(posedge pix_clk);
            tile_write     <= 1'b0;
        end
    endtask

    task automatic write_sprite(input int byte_offset, input logic [31:0] value);
        begin
            @(posedge pix_clk);
            sprite_write     <= 1'b1;
            sprite_read      <= 1'b0;
            sprite_address   <= byte_offset[SPRITE_ADDR_WIDTH-1:0];
            sprite_writedata <= value;
            @(posedge pix_clk);
            sprite_write     <= 1'b0;
        end
    endtask

    task automatic wait_for_pixel(input int x, input int y);
        begin
            do @(posedge pix_clk); while (!(vga_visible && pixel_x == x && pixel_y == y));
            #1;
        end
    endtask

    task automatic expect_rgb(
        input string label_text,
        input logic [23:0] expected_rgb,
        input bit expected_sprite_hit,
        input int expected_tile_addr,
        input int expected_tile_id
    );
        begin
            if ({vga_r, vga_g, vga_b} !== expected_rgb) begin
                $display("FAIL: %s RGB = %06h, expected %06h",
                         label_text, {vga_r, vga_g, vga_b}, expected_rgb);
                errors++;
            end
            if (debug_sprite_hit !== expected_sprite_hit) begin
                $display("FAIL: %s sprite_hit = %0b, expected %0b",
                         label_text, debug_sprite_hit, expected_sprite_hit);
                errors++;
            end
            if (debug_tile_address !== expected_tile_addr[TILE_ADDR_WIDTH-1:0]) begin
                $display("FAIL: %s tile_addr = %0d, expected %0d",
                         label_text, debug_tile_address, expected_tile_addr);
                errors++;
            end
            if (debug_tile_id !== expected_tile_id[2:0]) begin
                $display("FAIL: %s tile_id = %0d, expected %0d",
                         label_text, debug_tile_id, expected_tile_id);
                errors++;
            end
        end
    endtask

    initial begin
        $dumpfile("video_pipeline_tb.vcd");
        $dumpvars(0, video_pipeline_tb);

        errors           = 0;
        tile_write       = 1'b0;
        tile_read        = 1'b0;
        tile_address     = '0;
        tile_writedata   = '0;
        sprite_write     = 1'b0;
        sprite_read      = 1'b0;
        sprite_address   = '0;
        sprite_writedata = '0;

        repeat (3) @(posedge pix_clk);
        rst <= 1'b0;

        // Tile 0: blue background. Sprite 0: player 1 at upper-left.
        write_tile(0, 8'h01);
        write_sprite(0, pack_sprite(0, 0, 0, 1'b1));

        wait_for_pixel(0, 0);
        expect_rgb("blue grid corner", 24'h246ab8, 1'b0, 0, 1);

        wait_for_pixel(40, 0);
        expect_rgb("empty tile default", 24'h181c22, 1'b0, 1, 0);

        wait_for_pixel(6, 4);
        expect_rgb("sprite over tile", 24'h18181c, 1'b1, 0, 1);

        if (errors == 0) begin
            $display("PASS: video pipeline integration verified");
        end else begin
            $display("FAIL: %0d video pipeline errors", errors);
        end

        $finish;
    end

endmodule

`default_nettype wire
