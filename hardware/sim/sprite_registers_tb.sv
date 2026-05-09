// -----------------------------------------------------------------------------
// sprite_registers_tb.sv -- checks register packing, masking, and read ports
// -----------------------------------------------------------------------------

`timescale 1ns / 1ps
`default_nettype none

module sprite_registers_tb;

    localparam int SPRITE_COUNT = 30;
    localparam int ADDR_WIDTH   = $clog2(SPRITE_COUNT * 4);
    localparam int INDEX_WIDTH  = $clog2(SPRITE_COUNT);

    logic                   clk = 1'b0;
    logic                   rst = 1'b1;
    logic                   avs_write;
    logic                   avs_read;
    logic [ADDR_WIDTH-1:0]  avs_address;
    logic [31:0]            avs_writedata;
    logic [31:0]            avs_readdata;
    logic [INDEX_WIDTH-1:0] render_index;
    logic [9:0]             render_x;
    logic [8:0]             render_y;
    logic [4:0]             render_sprite_id;
    logic                   render_enable;

    int errors;

    sprite_registers #(
        .SPRITE_COUNT (SPRITE_COUNT),
        .ADDR_WIDTH   (ADDR_WIDTH),
        .INDEX_WIDTH  (INDEX_WIDTH)
    ) dut (
        .clk              (clk),
        .rst              (rst),
        .avs_write        (avs_write),
        .avs_read         (avs_read),
        .avs_address      (avs_address),
        .avs_writedata    (avs_writedata),
        .avs_readdata     (avs_readdata),
        .render_index     (render_index),
        .render_x         (render_x),
        .render_y         (render_y),
        .render_sprite_id (render_sprite_id),
        .render_enable    (render_enable)
    );

    initial forever #5 clk = ~clk;

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

    task automatic write_reg(input int byte_offset, input logic [31:0] value);
        begin
            @(posedge clk);
            avs_write     <= 1'b1;
            avs_read      <= 1'b0;
            avs_address   <= byte_offset[ADDR_WIDTH-1:0];
            avs_writedata <= value;
            @(posedge clk);
            avs_write     <= 1'b0;
        end
    endtask

    task automatic read_reg(input int byte_offset, input logic [31:0] expected);
        begin
            @(posedge clk);
            avs_write   <= 1'b0;
            avs_read    <= 1'b1;
            avs_address <= byte_offset[ADDR_WIDTH-1:0];
            @(posedge clk);
            avs_read    <= 1'b0;
            #1;
            if (avs_readdata !== expected) begin
                $display("FAIL: read offset 0x%02h = 0x%08h, expected 0x%08h",
                         byte_offset, avs_readdata, expected);
                errors++;
            end
        end
    endtask

    task automatic check_render_port(
        input int index,
        input int expected_x,
        input int expected_y,
        input int expected_sprite_id,
        input bit expected_enable
    );
        begin
            render_index = index[INDEX_WIDTH-1:0];
            #1;
            if (render_x !== expected_x[9:0]) begin
                $display("FAIL: sprite %0d x = %0d, expected %0d",
                         index, render_x, expected_x);
                errors++;
            end
            if (render_y !== expected_y[8:0]) begin
                $display("FAIL: sprite %0d y = %0d, expected %0d",
                         index, render_y, expected_y);
                errors++;
            end
            if (render_sprite_id !== expected_sprite_id[4:0]) begin
                $display("FAIL: sprite %0d id = %0d, expected %0d",
                         index, render_sprite_id, expected_sprite_id);
                errors++;
            end
            if (render_enable !== expected_enable) begin
                $display("FAIL: sprite %0d enable = %0b, expected %0b",
                         index, render_enable, expected_enable);
                errors++;
            end
        end
    endtask

    initial begin
        $dumpfile("sprite_registers_tb.vcd");
        $dumpvars(0, sprite_registers_tb);

        errors        = 0;
        avs_write     = 1'b0;
        avs_read      = 1'b0;
        avs_address   = '0;
        avs_writedata = '0;
        render_index  = '0;

        repeat (3) @(posedge clk);
        rst <= 1'b0;

        write_reg(0,   pack_sprite(12, 34, 5, 1'b1));
        write_reg(116, pack_sprite(639, 479, 29, 1'b1) | 32'hFE00_0000);

        read_reg(0,   pack_sprite(12, 34, 5, 1'b1));
        read_reg(116, pack_sprite(639, 479, 29, 1'b1));

        check_render_port(0, 12, 34, 5, 1'b1);
        check_render_port(29, 639, 479, 29, 1'b1);

        // Unaligned writes are ignored and unaligned reads return zero.
        write_reg(2, pack_sprite(100, 100, 7, 1'b1));
        read_reg(2, 32'd0);
        check_render_port(0, 12, 34, 5, 1'b1);

        // Offsets beyond sprite 29 are outside the documented 0x000..0x077 map.
        read_reg(120, 32'd0);

        if (errors == 0) begin
            $display("PASS: sprite registers verified");
        end else begin
            $display("FAIL: %0d sprite register errors", errors);
        end

        $finish;
    end

endmodule

`default_nettype wire
