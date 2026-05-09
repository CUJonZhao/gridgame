# =============================================================================
# de1_soc_gridbrawl.sdc -- timing constraints for the GridBrawl top level.
#
# Add this file to the Quartus project (Settings -> TimeQuest Timing Analyzer
# -> SDC files). The PLL outputs are picked up automatically once the PLL
# IP is generated, but we constrain the input oscillator and the
# chip-level VGA / audio outputs explicitly.
# =============================================================================

# -----------------------------------------------------------------------------
# Primary clocks
# -----------------------------------------------------------------------------
create_clock -name CLOCK_50 -period 20.000 [get_ports CLOCK_50]

# Derive PLL output clocks (and HPS clocks) automatically.
derive_pll_clocks -create_base_clocks
derive_clock_uncertainty

# -----------------------------------------------------------------------------
# VGA outputs.
# The ADV7123 datasheet wants ~5 ns set-up and ~2 ns hold on the RGB / sync
# pads relative to VGA_CLK. We constrain to a comfortable 6/2 budget.
# -----------------------------------------------------------------------------
set vga_clk_pin VGA_CLK
set vga_outs    [get_ports {VGA_HS VGA_VS VGA_BLANK_N VGA_SYNC_N \
                            VGA_R[*] VGA_G[*] VGA_B[*]}]

set_output_delay -clock [get_clocks {*pll_i*c0*}] -max 6.000 $vga_outs
set_output_delay -clock [get_clocks {*pll_i*c0*}] -min -2.000 $vga_outs

# Do not analyze the pixel clock pin against itself.
set_false_path -from * -to [get_ports $vga_clk_pin]

# -----------------------------------------------------------------------------
# WM8731 I2S outputs.
# The codec captures DACDAT on the rising edge of BCLK. Aim for 4 ns set-up
# and 1 ns hold relative to AUD_BCLK.
# -----------------------------------------------------------------------------
set_output_delay -clock [get_clocks {*pll_i*c1*}] -max 4.000 \
    [get_ports {AUD_DACDAT AUD_DACLRCK}]
set_output_delay -clock [get_clocks {*pll_i*c1*}] -min -1.000 \
    [get_ports {AUD_DACDAT AUD_DACLRCK}]

# AUD_XCK is the master clock for the codec; mark it as a generated clock
# off the PLL c1 output.
create_generated_clock -name AUD_XCK_CLK \
    -source [get_pins {*pll_i*altera_pll_i*outclk_wire[1]}] \
    [get_ports AUD_XCK]

# Do not analyze the codec MCLK pad against the codec data pads --
# they have a clean phase relationship by construction.
set_false_path -from [get_ports AUD_XCK] -to *

# -----------------------------------------------------------------------------
# Asynchronous push-button reset.
# -----------------------------------------------------------------------------
set_false_path -from [get_ports KEY[0]] -to *
