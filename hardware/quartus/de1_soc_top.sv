// =============================================================================
// de1_soc_top.sv -- DE1-SoC top-level for GridBrawl.
//
//   FPGA-side blocks:
//     * gridbrawl_pll  : 50 MHz -> 25.175 MHz (pix_clk) + 12.288 MHz (mclk)
//                        (instantiate from the IP catalog ALTPLL megafunction;
//                         pin assignments + .mif baked into the generated IP)
//     * gridbrawl_qsys : Platform Designer system containing the HPS hard
//                        macro, the HPS-to-FPGA lightweight bridge, and the
//                        two custom components defined in:
//                            hardware/quartus/video_pipeline_hw.tcl
//                            hardware/quartus/audio_subsystem_hw.tcl
//
//   Qsys address map (must match kernel_modules/{audio,video}/*.h):
//     0xFF20_2000  audio_subsystem.avs    (span 0x10)
//     0xFF20_3000  video_pipeline.tile    (span 0x200)
//     0xFF20_4000  video_pipeline.sprite  (span 0x80)
//
//   Pin names below follow the Terasic DE1-SoC reference design (Quartus
//   17.x). Drop the matching .qsf into the project to bind these to the
//   physical pads.
// =============================================================================

`default_nettype none

module de1_soc_top (
    // ---- 50 MHz oscillator and reset push-button ------------------------
    input  wire        CLOCK_50,
    input  wire [3:0]  KEY,                // KEY[0] = active-low reset

    // ---- VGA -----------------------------------------------------------
    output wire        VGA_CLK,            // pixel clock to the VGA DAC
    output wire        VGA_HS,
    output wire        VGA_VS,
    output wire        VGA_BLANK_N,        // active-low blank
    output wire        VGA_SYNC_N,         // tied off (no sync-on-green)
    output wire [7:0]  VGA_R,
    output wire [7:0]  VGA_G,
    output wire [7:0]  VGA_B,

    // ---- WM8731 audio codec -------------------------------------------
    output wire        AUD_XCK,            // master clock to the codec
    output wire        AUD_BCLK,
    output wire        AUD_DACLRCK,
    output wire        AUD_DACDAT,
    inout  wire        FPGA_I2C_SDAT,      // I2C control bus (codec config)
    output wire        FPGA_I2C_SCLK,

    // ---- HPS hard macro pins (passed straight through to the Qsys
    //      instance; the Qsys-generated wrapper drives them all). Only the
    //      ones GridBrawl exercises are listed here as input/inout/output;
    //      add the rest from your DE1-SoC golden reference as needed. ----
    inout  wire [14:0] HPS_DDR3_ADDR,
    inout  wire [2:0]  HPS_DDR3_BA,
    inout  wire        HPS_DDR3_CAS_N,
    inout  wire        HPS_DDR3_CKE,
    inout  wire        HPS_DDR3_CK_N,
    inout  wire        HPS_DDR3_CK_P,
    inout  wire        HPS_DDR3_CS_N,
    inout  wire [3:0]  HPS_DDR3_DM,
    inout  wire [31:0] HPS_DDR3_DQ,
    inout  wire [3:0]  HPS_DDR3_DQS_N,
    inout  wire [3:0]  HPS_DDR3_DQS_P,
    inout  wire        HPS_DDR3_ODT,
    inout  wire        HPS_DDR3_RAS_N,
    inout  wire        HPS_DDR3_RESET_N,
    inout  wire        HPS_DDR3_RZQ,
    inout  wire        HPS_DDR3_WE_N,

    inout  wire        HPS_USB_CLKOUT,
    inout  wire [7:0]  HPS_USB_DATA,
    inout  wire        HPS_USB_DIR,
    inout  wire        HPS_USB_NXT,
    inout  wire        HPS_USB_STP
);

    // -------------------------------------------------------------------
    // Reset (synchronous, active-high) for the FPGA-only blocks.
    // -------------------------------------------------------------------
    wire pix_clk;
    wire mclk;
    wire pll_locked;

    wire fpga_rst_async = ~KEY[0] | ~pll_locked;

    reg [3:0] rst_pix_sync;
    reg [3:0] rst_mclk_sync;
    always @(posedge pix_clk or posedge fpga_rst_async) begin
        if (fpga_rst_async) rst_pix_sync <= 4'hF;
        else                rst_pix_sync <= {rst_pix_sync[2:0], 1'b0};
    end
    always @(posedge mclk or posedge fpga_rst_async) begin
        if (fpga_rst_async) rst_mclk_sync <= 4'hF;
        else                rst_mclk_sync <= {rst_mclk_sync[2:0], 1'b0};
    end
    wire pix_rst = rst_pix_sync[3];
    wire mclk_rst = rst_mclk_sync[3];

    // -------------------------------------------------------------------
    // PLL: regenerate this with the IP catalog "ALTERA_PLL" or "ALTPLL"
    // wizard. Inputs/outputs:
    //   inclk0 = CLOCK_50 (50 MHz)
    //   c0     = 25.175 MHz, 0 deg phase (pix_clk for VGA)
    //   c1     = 12.288 MHz, 0 deg phase (mclk for WM8731)
    //   locked = pll_locked
    // -------------------------------------------------------------------
    gridbrawl_pll pll_i (
        .refclk   (CLOCK_50),
        .rst      (1'b0),
        .outclk_0 (pix_clk),
        .outclk_1 (mclk),
        .locked   (pll_locked)
    );

    // The codec's master clock is just the PLL mclk output piped to the
    // chip pin. Some teams gate this with `pll_locked` -- not necessary
    // because the WM8731 tolerates startup glitches.
    assign AUD_XCK = mclk;

    // -------------------------------------------------------------------
    // VGA conduit out of Qsys.
    // -------------------------------------------------------------------
    wire        vga_hsync;
    wire        vga_vsync;
    wire        vga_visible;
    wire [7:0]  vga_r;
    wire [7:0]  vga_g;
    wire [7:0]  vga_b;

    // The DE1-SoC VGA DAC (ADV7123) latches RGB on the rising edge of
    // VGA_CLK. Drive that pin with the same pix_clk that produced the
    // sync signals so set/hold are met by construction.
    assign VGA_CLK     = pix_clk;
    assign VGA_HS      = vga_hsync;
    assign VGA_VS      = vga_vsync;
    assign VGA_BLANK_N = vga_visible;
    assign VGA_SYNC_N  = 1'b0;
    assign VGA_R       = vga_visible ? vga_r : 8'h00;
    assign VGA_G       = vga_visible ? vga_g : 8'h00;
    assign VGA_B       = vga_visible ? vga_b : 8'h00;

    // -------------------------------------------------------------------
    // Codec conduit out of Qsys.
    // -------------------------------------------------------------------
    wire aud_bclk;
    wire aud_dac_lrck;
    wire aud_dac_dat;

    assign AUD_BCLK     = aud_bclk;
    assign AUD_DACLRCK  = aud_dac_lrck;
    assign AUD_DACDAT   = aud_dac_dat;

    // I2C bus to the codec is driven by the HPS Linux side (i2c-tools or
    // a small init script that programs WM8731's control registers).
    // The Qsys system's HPS instance brings out FPGA_I2C_* internally on
    // most golden top files; if your reference design doesn't, replace
    // the two assigns below with the real wiring.
    assign FPGA_I2C_SDAT  = 1'bz;
    assign FPGA_I2C_SCLK  = 1'b1;

    // -------------------------------------------------------------------
    // Qsys system. Generated from gridbrawl_qsys.qsys; once Qsys produces
    // gridbrawl_qsys.v / .qip, the auto-generated port list goes here.
    // The signal names below are the canonical Qsys conduit names that
    // the two _hw.tcl files in this directory declare.
    // -------------------------------------------------------------------
    gridbrawl_qsys u_qsys (
        // HPS hard macro pins
        .memory_mem_a            (HPS_DDR3_ADDR),
        .memory_mem_ba           (HPS_DDR3_BA),
        .memory_mem_ck           (HPS_DDR3_CK_P),
        .memory_mem_ck_n         (HPS_DDR3_CK_N),
        .memory_mem_cke          (HPS_DDR3_CKE),
        .memory_mem_cs_n         (HPS_DDR3_CS_N),
        .memory_mem_ras_n        (HPS_DDR3_RAS_N),
        .memory_mem_cas_n        (HPS_DDR3_CAS_N),
        .memory_mem_we_n         (HPS_DDR3_WE_N),
        .memory_mem_reset_n      (HPS_DDR3_RESET_N),
        .memory_mem_dq           (HPS_DDR3_DQ),
        .memory_mem_dqs          (HPS_DDR3_DQS_P),
        .memory_mem_dqs_n        (HPS_DDR3_DQS_N),
        .memory_mem_odt          (HPS_DDR3_ODT),
        .memory_mem_dm           (HPS_DDR3_DM),
        .memory_oct_rzqin        (HPS_DDR3_RZQ),

        .hps_io_hps_io_usb1_inst_D0  (HPS_USB_DATA[0]),
        .hps_io_hps_io_usb1_inst_D1  (HPS_USB_DATA[1]),
        .hps_io_hps_io_usb1_inst_D2  (HPS_USB_DATA[2]),
        .hps_io_hps_io_usb1_inst_D3  (HPS_USB_DATA[3]),
        .hps_io_hps_io_usb1_inst_D4  (HPS_USB_DATA[4]),
        .hps_io_hps_io_usb1_inst_D5  (HPS_USB_DATA[5]),
        .hps_io_hps_io_usb1_inst_D6  (HPS_USB_DATA[6]),
        .hps_io_hps_io_usb1_inst_D7  (HPS_USB_DATA[7]),
        .hps_io_hps_io_usb1_inst_CLK (HPS_USB_CLKOUT),
        .hps_io_hps_io_usb1_inst_STP (HPS_USB_STP),
        .hps_io_hps_io_usb1_inst_DIR (HPS_USB_DIR),
        .hps_io_hps_io_usb1_inst_NXT (HPS_USB_NXT),

        // Pixel-clock domain
        .pix_clk_clk             (pix_clk),
        .pix_reset_reset         (pix_rst),

        // Audio mclk domain
        .mclk_clk                (mclk),
        .mclk_reset_reset        (mclk_rst),

        // Video conduit
        .vga_hsync               (vga_hsync),
        .vga_vsync               (vga_vsync),
        .vga_visible             (vga_visible),
        .vga_r                   (vga_r),
        .vga_g                   (vga_g),
        .vga_b                   (vga_b),

        // Audio conduit
        .codec_bclk              (aud_bclk),
        .codec_lrck              (aud_dac_lrck),
        .codec_dat               (aud_dac_dat)
    );

endmodule

`default_nettype wire
