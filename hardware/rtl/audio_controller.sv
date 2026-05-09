// -----------------------------------------------------------------------------
// audio_controller.sv -- Avalon-MM-facing playback FSM for GridBrawl audio
//
//  Memory map (relative to peripheral base 0xFF202000 in the design doc):
//    0x000..0x003  Sound Select Register
//                  bits [3:0]  Sound_ID  (1=EXPLOSION, 2=PLAYER_DIES,
//                                         3=VICTORY,    4=WALL_BREAKS)
//                              writing 1..4 immediately triggers the SFX;
//                              writing 0 is treated as a no-op.
//                  bits [31:4] reserved, read as 0.
//
//  Behavior:
//    * On reset the controller starts from BGM phase 0.
//    * BGM phase increments on every sample_req from the I2S transmitter,
//      wrapping modulo audio_sound_rom's BGM track length.
//    * A successful SFX write switches the playback source to that SFX, with
//      the SFX phase counter starting at 0. The SFX runs to the track length
//      reported by audio_sound_rom and then control automatically returns to
//      BGM playback. The BGM phase keeps advancing during SFX so the music
//      "resumes" from where it would naturally have been.
//    * Mono synthesis: sample_l == sample_r. Switching to a stereo source
//      later means selecting different left/right tracks here.
// -----------------------------------------------------------------------------

`default_nettype none

module audio_controller #(
    parameter int BITS        = 16,
    parameter int SAMPLE_RATE = 48000,
    parameter int ADDR_WIDTH  = 2
) (
    input  logic                       clk,
    input  logic                       rst,

    // Avalon-MM 32-bit register slave
    input  logic                       avs_write,
    input  logic                       avs_read,
    input  logic [ADDR_WIDTH-1:0]      avs_address,
    input  logic [31:0]                avs_writedata,
    output logic [31:0]                avs_readdata,

    // Sample pull from the I2S transmitter
    input  logic                       sample_req,
    output logic signed [BITS-1:0]     sample_l,
    output logic signed [BITS-1:0]     sample_r,

    // Status taps
    output logic                       playing_sfx,
    output logic [3:0]                 current_track
);

    localparam logic [23:0] BGM_LENGTH = 24'(16 * SAMPLE_RATE);

    // State registers
    logic [23:0] bgm_phase;
    logic [23:0] sfx_phase;
    logic [3:0]  sfx_track;
    logic        in_sfx;

    // Wires to the sound ROM
    logic [3:0]                rom_track;
    logic [23:0]               rom_idx;
    logic signed [BITS-1:0]    rom_sample;
    logic [23:0]               rom_length;

    audio_sound_rom #(
        .BITS        (BITS),
        .SAMPLE_RATE (SAMPLE_RATE)
    ) sound_rom (
        .track        (rom_track),
        .sample_idx   (rom_idx),
        .sample       (rom_sample),
        .track_length (rom_length)
    );

    // Combinational selection of the active track for the ROM read.
    always_comb begin
        if (in_sfx) begin
            rom_track = sfx_track;
            rom_idx   = sfx_phase;
        end else begin
            rom_track = 4'd0;
            rom_idx   = bgm_phase;
        end
    end

    assign sample_l      = rom_sample;
    assign sample_r      = rom_sample;
    assign playing_sfx   = in_sfx;
    assign current_track = rom_track;

    // Avalon-MM read returns the current Sound_ID along with a status flag.
    always_ff @(posedge clk) begin
        if (rst) begin
            avs_readdata <= 32'd0;
        end else if (avs_read && avs_address == '0) begin
            avs_readdata <= {27'd0, in_sfx, rom_track};
        end else if (avs_read) begin
            avs_readdata <= 32'd0;
        end
    end

    // Playback FSM and HPS write side. The Avalon-write override is placed
    // AFTER the sample_req advance so that a same-cycle write wins via the
    // last-assignment rule of nonblocking updates.
    always_ff @(posedge clk) begin
        if (rst) begin
            bgm_phase <= 24'd0;
            sfx_phase <= 24'd0;
            sfx_track <= 4'd0;
            in_sfx    <= 1'b0;
        end else begin
            // 1) Sample advance.
            if (sample_req) begin
                if (bgm_phase == BGM_LENGTH - 1) begin
                    bgm_phase <= 24'd0;
                end else begin
                    bgm_phase <= bgm_phase + 1'b1;
                end

                if (in_sfx) begin
                    if (sfx_phase >= rom_length - 1) begin
                        in_sfx    <= 1'b0;
                        sfx_phase <= 24'd0;
                    end else begin
                        sfx_phase <= sfx_phase + 1'b1;
                    end
                end
            end

            // 2) Avalon write trigger overrides above for sfx_phase / track.
            if (avs_write && avs_address == '0) begin
                if (avs_writedata[3:0] >= 4'd1 && avs_writedata[3:0] <= 4'd4) begin
                    in_sfx    <= 1'b1;
                    sfx_track <= avs_writedata[3:0];
                    sfx_phase <= 24'd0;
                end
            end
        end
    end

endmodule

`default_nettype wire
