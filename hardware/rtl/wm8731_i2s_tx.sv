// -----------------------------------------------------------------------------
// wm8731_i2s_tx.sv -- I2S transmitter for the DE1-SoC WM8731 audio codec
//
//  FPGA-master configuration: this module generates BCLK (bit clock) and
//  LRCK (left/right clock) from a free-running MCLK and shifts a pair of
//  signed PCM samples out on DACDAT, MSB first, in standard I2S framing.
//
//  I2S timing (per Philips/NXP "I2S bus specification"):
//    * Data bit changes on the falling edge of BCLK
//    * Codec samples on the rising edge of BCLK
//    * LRCK transitions one BCLK cycle BEFORE the MSB of the next channel
//      (this module follows the simpler "left when LRCK low, right when high"
//      convention which the WM8731 understands when configured for I2S)
//
//  Frame layout for BITS_PER_CHANNEL = 16:
//        BCLK                ____      ____      ____  ...
//        LRCK     ___________|         |          |
//        bit_cnt  [0][1][2]...[15][16][17]...[31][0]...
//        DACDAT   [L15][L14]...[L0][R15][R14]...[R0]
//
//  The producer side is asked for a fresh sample pair via sample_req which
//  pulses one mclk cycle before bit_cnt rolls back to zero. This gives the
//  upstream FSM up to BCLK_DIV cycles to deliver new data.
// -----------------------------------------------------------------------------

`default_nettype none

module wm8731_i2s_tx #(
    parameter int BITS_PER_CHANNEL = 16,
    parameter int MCLK_DIV_BCLK    = 8       // BCLK = MCLK / 8
                                              // For MCLK=12.288 MHz this gives
                                              // BCLK=1.536 MHz, LRCK=48 kHz.
                                              // Use 48 for 8 kHz LRCK.
) (
    input  logic                              mclk,
    input  logic                              rst,

    // PCM inputs, signed two's complement
    input  logic signed [BITS_PER_CHANNEL-1:0] sample_l,
    input  logic signed [BITS_PER_CHANNEL-1:0] sample_r,

    // High one mclk cycle before the transmitter latches the next sample pair.
    output logic                              sample_req,

    // I2S signals to the WM8731 codec
    output logic                              bclk,
    output logic                              lrck,
    output logic                              dacdat
);

    // Half-period of BCLK in MCLK ticks.
    localparam int BCLK_HALF       = MCLK_DIV_BCLK / 2;
    localparam int BCLK_CNT_WIDTH  = (BCLK_HALF <= 1) ? 1 : $clog2(BCLK_HALF);
    // Total bits per LRCK frame (left + right, two channels).
    localparam int FRAME_BITS      = 2 * BITS_PER_CHANNEL;
    localparam int BIT_CNT_WIDTH   = $clog2(FRAME_BITS);

    logic [BCLK_CNT_WIDTH-1:0] bclk_div_cnt;
    logic                      bclk_int;
    logic                      bclk_edge_falling;
    logic                      bclk_edge_rising;

    // Generate BCLK as a square wave with period MCLK_DIV_BCLK.
    always_ff @(posedge mclk) begin
        if (rst) begin
            bclk_div_cnt <= '0;
            bclk_int     <= 1'b0;
        end else if (bclk_div_cnt == BCLK_HALF[BCLK_CNT_WIDTH-1:0] - 1) begin
            bclk_div_cnt <= '0;
            bclk_int     <= ~bclk_int;
        end else begin
            bclk_div_cnt <= bclk_div_cnt + 1'b1;
        end
    end

    assign bclk              = bclk_int;
    // The cycle that COMPLETES a half-period and toggles bclk_int. We mark
    // "falling" and "rising" relative to the value bclk_int will hold AFTER
    // the toggle.
    assign bclk_edge_falling = (bclk_div_cnt == BCLK_HALF[BCLK_CNT_WIDTH-1:0] - 1) &&  bclk_int;
    assign bclk_edge_rising  = (bclk_div_cnt == BCLK_HALF[BCLK_CNT_WIDTH-1:0] - 1) && ~bclk_int;

    // Bit counter advances on each BCLK falling edge.
    logic [BIT_CNT_WIDTH-1:0]    bit_cnt;
    logic [BITS_PER_CHANNEL-1:0] shift_l;
    logic [BITS_PER_CHANNEL-1:0] shift_r;

    always_ff @(posedge mclk) begin
        if (rst) begin
            bit_cnt <= '0;
            shift_l <= '0;
            shift_r <= '0;
        end else if (bclk_edge_falling) begin
            // Latch a fresh sample pair at the start of every left-channel slot.
            if (bit_cnt == '0) begin
                shift_l <= sample_l;
                shift_r <= sample_r;
            end

            if (bit_cnt == FRAME_BITS[BIT_CNT_WIDTH-1:0] - 1) begin
                bit_cnt <= '0;
            end else begin
                bit_cnt <= bit_cnt + 1'b1;
            end
        end
    end

    // LRCK low for the left half, high for the right half.
    assign lrck = (bit_cnt >= BITS_PER_CHANNEL[BIT_CNT_WIDTH-1:0]);

    // Position within current channel and current channel selection.
    logic                            in_right;
    logic [BIT_CNT_WIDTH-1:0]        pos_in_channel;
    logic [BITS_PER_CHANNEL-1:0]     active_word;

    assign in_right       = (bit_cnt >= BITS_PER_CHANNEL[BIT_CNT_WIDTH-1:0]);
    assign pos_in_channel = in_right ? (bit_cnt - BITS_PER_CHANNEL[BIT_CNT_WIDTH-1:0]) : bit_cnt;
    assign active_word    = in_right ? shift_r : shift_l;

    // MSB first: DACDAT bit index = (BITS_PER_CHANNEL-1) - pos_in_channel.
    assign dacdat = active_word[BITS_PER_CHANNEL - 1 - pos_in_channel];

    // Sample-request pulse: one mclk before the next left slot starts.
    assign sample_req = bclk_edge_falling &&
                        (bit_cnt == FRAME_BITS[BIT_CNT_WIDTH-1:0] - 1);

endmodule

`default_nettype wire
