// -----------------------------------------------------------------------------
// audio_sound_rom.sv -- procedural sample source for the GridBrawl audio path
//
//  Mirrors the role sprite_image_rom plays for the video pipeline: this module
//  fakes the eventual sample ROM with combinational waveforms so the audio
//  pipeline can be wired up and verified before real .mif assets are produced.
//
//  Tracks (selected by `track`):
//    0  background music   -- ~16 s two-tone loop at SAMPLE_RATE
//    1  EXPLOSION SFX      -- ~0.5 s low square wave
//    2  PLAYER_DIES SFX    -- ~0.75 s pulsing tone (frequency drop illusion)
//    3  VICTORY SFX        -- ~1.5 s alternating high/mid square wave
//    4  WALL_BREAKS SFX    -- ~0.25 s high chirp
//    *  silence            -- single sample of 0
//
//  Outputs are signed PCM at +/- (2^(BITS-2)-1) ~= -6 dBFS to leave headroom.
//  `track_length` gives the producer-side total sample count so the caller
//  knows when an SFX completes; the controller wraps BGM modulo this length.
// -----------------------------------------------------------------------------

`default_nettype none

module audio_sound_rom #(
    parameter int BITS        = 16,
    parameter int SAMPLE_RATE = 48000
) (
    input  logic [3:0]              track,
    input  logic [23:0]             sample_idx,
    output logic signed [BITS-1:0]  sample,
    output logic [23:0]             track_length
);

    localparam logic signed [BITS-1:0] AMP_HIGH = ((1 << (BITS - 2)) - 1);
    localparam logic signed [BITS-1:0] AMP_LOW  = -((1 << (BITS - 2)) - 1);
    localparam logic signed [BITS-1:0] SILENCE  = '0;

    logic toggle;
    logic [23:0] len;

    always_comb begin
        toggle = 1'b0;
        len    = 24'd1;

        unique case (track)
            4'd0: begin // BGM, ~16 s
                toggle = sample_idx[19] ^ sample_idx[7];
                len    = 24'(16 * SAMPLE_RATE);
            end
            4'd1: begin // EXPLOSION, ~0.5 s
                toggle = sample_idx[8];
                len    = 24'(SAMPLE_RATE / 2);
            end
            4'd2: begin // PLAYER_DIES, ~0.75 s, pulsing
                toggle = sample_idx[8] ^ sample_idx[12];
                len    = 24'(SAMPLE_RATE * 3 / 4);
            end
            4'd3: begin // VICTORY, ~1.5 s, alternating tones
                toggle = sample_idx[14] ? sample_idx[6] : sample_idx[7];
                len    = 24'(SAMPLE_RATE * 3 / 2);
            end
            4'd4: begin // WALL_BREAKS, ~0.25 s
                toggle = sample_idx[5] ^ sample_idx[6];
                len    = 24'(SAMPLE_RATE / 4);
            end
            default: begin
                toggle = 1'b0;
                len    = 24'd1;
            end
        endcase
    end

    always_comb begin
        if (track > 4'd4) begin
            sample = SILENCE;
        end else if (toggle) begin
            sample = AMP_HIGH;
        end else begin
            sample = AMP_LOW;
        end
    end

    assign track_length = len;

endmodule

`default_nettype wire
