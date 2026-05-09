// -----------------------------------------------------------------------------
// tile_map_ram.sv -- byte-addressed 20x15 tile map storage
//
//  The HPS-visible side follows the design document's simple memory map:
//    offset 0x000..0x12B -> one byte per tile, row-major order
//    bits [2:0]          -> tile id
//    bits [7:3]          -> reserved
//
//  The renderer gets a separate read-only port so the VGA path can fetch tile
//  ids continuously while the HPS updates the map through Avalon-MM glue.
// -----------------------------------------------------------------------------

`default_nettype none

module tile_map_ram #(
    parameter int MAP_COLS   = 20,
    parameter int MAP_ROWS   = 15,
    parameter int TILE_BITS  = 8,
    parameter int ADDR_WIDTH = $clog2(MAP_COLS * MAP_ROWS)
) (
    input  logic                    clk,
    input  logic                    rst,

    // HPS / Avalon-MM style byte interface. Address is a tile byte offset.
    input  logic                    avs_write,
    input  logic                    avs_read,
    input  logic [ADDR_WIDTH-1:0]   avs_address,
    input  logic [TILE_BITS-1:0]    avs_writedata,
    output logic [TILE_BITS-1:0]    avs_readdata,

    // VGA renderer read port.
    input  logic [ADDR_WIDTH-1:0]   render_address,
    output logic [2:0]              render_tile_id
);

    localparam int TILE_COUNT = MAP_COLS * MAP_ROWS;

    logic [TILE_BITS-1:0] mem [0:TILE_COUNT-1];

    integer i;

    always_ff @(posedge clk) begin
        if (rst) begin
            for (i = 0; i < TILE_COUNT; i = i + 1) begin
                mem[i] <= '0;
            end
            avs_readdata <= '0;
        end else begin
            if (avs_write && avs_address < TILE_COUNT) begin
                mem[avs_address] <= avs_writedata;
            end

            if (avs_read && avs_address < TILE_COUNT) begin
                avs_readdata <= mem[avs_address];
            end else if (avs_read) begin
                avs_readdata <= '0;
            end
        end
    end

    always_comb begin
        if (render_address < TILE_COUNT) begin
            render_tile_id = mem[render_address][2:0];
        end else begin
            render_tile_id = 3'd0;
        end
    end

endmodule

`default_nettype wire
