// -----------------------------------------------------------------------------
// audio_subsystem.sv -- top-level glue for GridBrawl FPGA audio
//
//  Combines:
//    audio_controller  : Avalon-MM slave + BGM/SFX sequencing FSM
//    audio_sound_rom   : procedural sample source (instantiated inside the
//                        controller for a clean single-port read)
//    wm8731_i2s_tx     : I2S serial transmitter to the WM8731 codec
//
//  Clock plan (matches the DE1-SoC default):
//    * `clk` is the system / Avalon clock (50 MHz typical) -- used by the
//      controller for HPS-side register writes and for the Avalon read path.
//    * `mclk` is the codec master clock (12.288 MHz, fed from AUD_XCK or a
//      PLL output) -- used by the I2S transmitter to derive BCLK and LRCK.
//    * The controller reads sample_req on the system clock; in this first
//      cut the same clock drives both the controller and the I2S so we
//      avoid a CDC. A later top-level can attach a small dual-clock FIFO
//      between controller and transmitter when the team splits the domains.
//
//  The Avalon address space exposed here is a single 32-bit register at
//  byte offset 0x000 -- the design-doc layout for 0xFF202000.
// -----------------------------------------------------------------------------

`default_nettype none

module audio_subsystem #(
    parameter int BITS             = 16,
    parameter int SAMPLE_RATE      = 48000,
    parameter int MCLK_DIV_BCLK    = 8,        // 12.288 MHz / 8 = 1.536 MHz BCLK -> 48 kHz LRCK
    parameter int ADDR_WIDTH       = 2
) (
    input  logic                       clk,
    input  logic                       rst,

    // Avalon-MM 32-bit slave (HPS facing)
    input  logic                       avs_write,
    input  logic                       avs_read,
    input  logic [ADDR_WIDTH-1:0]      avs_address,
    input  logic [31:0]                avs_writedata,
    output logic [31:0]                avs_readdata,

    // I2S signals to the WM8731 codec
    output logic                       aud_bclk,
    output logic                       aud_dac_lrck,
    output logic                       aud_dac_dat,

    // Status taps for the integration testbench / debug
    output logic                       playing_sfx,
    output logic [3:0]                 current_track,
    output logic                       sample_req
);

    logic signed [BITS-1:0] sample_l;
    logic signed [BITS-1:0] sample_r;

    audio_controller #(
        .BITS        (BITS),
        .SAMPLE_RATE (SAMPLE_RATE),
        .ADDR_WIDTH  (ADDR_WIDTH)
    ) controller (
        .clk           (clk),
        .rst           (rst),
        .avs_write     (avs_write),
        .avs_read      (avs_read),
        .avs_address   (avs_address),
        .avs_writedata (avs_writedata),
        .avs_readdata  (avs_readdata),
        .sample_req    (sample_req),
        .sample_l      (sample_l),
        .sample_r      (sample_r),
        .playing_sfx   (playing_sfx),
        .current_track (current_track)
    );

    wm8731_i2s_tx #(
        .BITS_PER_CHANNEL (BITS),
        .MCLK_DIV_BCLK    (MCLK_DIV_BCLK)
    ) i2s_tx (
        .mclk        (clk),
        .rst         (rst),
        .sample_l    (sample_l),
        .sample_r    (sample_r),
        .sample_req  (sample_req),
        .bclk        (aud_bclk),
        .lrck        (aud_dac_lrck),
        .dacdat      (aud_dac_dat)
    );

endmodule

`default_nettype wire
