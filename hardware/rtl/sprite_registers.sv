// -----------------------------------------------------------------------------
// sprite_registers.sv -- 30 memory-mapped sprite control registers
//
//  Address map, relative to the sprite-register peripheral base:
//    0x000 + index*4 -> sprite[index], index 0..29
//
//  Register format, matching the design document:
//    bits [9:0]   X position, 0..639
//    bits [18:10] Y position, 0..479
//    bits [23:19] Sprite ID, 0..29
//    bit  [24]    Enable
//    bits [31:25] Reserved, read as 0
//
//  The renderer-facing port exposes one decoded sprite selected by
//  render_index. A sprite renderer can scan render_index from 0 to 29 and
//  composite enabled sprite pixels over the tile background.
// -----------------------------------------------------------------------------

`default_nettype none

module sprite_registers #(
    parameter int SPRITE_COUNT    = 30,
    parameter int SPRITE_ID_WIDTH = 5,
    parameter int ADDR_WIDTH      = $clog2(SPRITE_COUNT * 4),
    parameter int INDEX_WIDTH     = $clog2(SPRITE_COUNT)
) (
    input  logic                       clk,
    input  logic                       rst,

    // HPS / Avalon-MM style 32-bit register interface. Address is byte offset.
    input  logic                       avs_write,
    input  logic                       avs_read,
    input  logic [ADDR_WIDTH-1:0]      avs_address,
    input  logic [31:0]                avs_writedata,
    output logic [31:0]                avs_readdata,

    // Renderer debug/readback port for a selected sprite.
    input  logic [INDEX_WIDTH-1:0]     render_index,
    output logic [9:0]                 render_x,
    output logic [8:0]                 render_y,
    output logic [SPRITE_ID_WIDTH-1:0] render_sprite_id,
    output logic                       render_enable,

    // Parallel renderer port for full-sprite compositing.
    output logic [SPRITE_COUNT-1:0][9:0]                 sprite_x,
    output logic [SPRITE_COUNT-1:0][8:0]                 sprite_y,
    output logic [SPRITE_COUNT-1:0][SPRITE_ID_WIDTH-1:0] sprite_id,
    output logic [SPRITE_COUNT-1:0]                      sprite_enable
);

    localparam logic [31:0] SPRITE_REG_MASK = 32'h01FF_FFFF;

    logic [31:0] regs [0:SPRITE_COUNT-1];
    logic [INDEX_WIDTH-1:0] avs_index;
    logic avs_aligned;
    logic avs_index_valid;
    logic render_index_valid;

    integer i;
    genvar g;

    always_comb begin
        avs_index          = avs_address[ADDR_WIDTH-1:2];
        avs_aligned        = (avs_address[1:0] == 2'b00);
        avs_index_valid    = (avs_index < SPRITE_COUNT);
        render_index_valid = (render_index < SPRITE_COUNT);
    end

    always_ff @(posedge clk) begin
        if (rst) begin
            for (i = 0; i < SPRITE_COUNT; i = i + 1) begin
                regs[i] <= 32'd0;
            end
            avs_readdata <= 32'd0;
        end else begin
            if (avs_write && avs_aligned && avs_index_valid) begin
                regs[avs_index] <= avs_writedata & SPRITE_REG_MASK;
            end

            if (avs_read && avs_aligned && avs_index_valid) begin
                avs_readdata <= regs[avs_index] & SPRITE_REG_MASK;
            end else if (avs_read) begin
                avs_readdata <= 32'd0;
            end
        end
    end

    always_comb begin
        if (render_index_valid) begin
            render_x         = regs[render_index][9:0];
            render_y         = regs[render_index][18:10];
            render_sprite_id = regs[render_index][23:19];
            render_enable    = regs[render_index][24];
        end else begin
            render_x         = 10'd0;
            render_y         = 9'd0;
            render_sprite_id = '0;
            render_enable    = 1'b0;
        end
    end

    generate
        for (g = 0; g < SPRITE_COUNT; g = g + 1) begin : decoded_sprite_outputs
            assign sprite_x[g]      = regs[g][9:0];
            assign sprite_y[g]      = regs[g][18:10];
            assign sprite_id[g]     = regs[g][23:19];
            assign sprite_enable[g] = regs[g][24];
        end
    endgenerate

endmodule

`default_nettype wire
