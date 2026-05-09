# =============================================================================
# audio_subsystem_hw.tcl -- Platform Designer (Qsys) component descriptor for
# the GridBrawl FPGA audio subsystem (controller + sound ROM + I2S TX).
#
# One Avalon-MM slave (32-bit, byte-addressed, span 0x10) plus a conduit to
# the WM8731 codec pins and a small status conduit.
# =============================================================================

package require -exact qsys 16.1

set_module_property NAME         audio_subsystem
set_module_property DISPLAY_NAME "GridBrawl Audio Subsystem"
set_module_property VERSION      1.0
set_module_property GROUP        "GridBrawl"
set_module_property DESCRIPTION  "Avalon-MM-controlled audio playback FSM with WM8731 I2S TX"
set_module_property AUTHOR       "GridBrawl Team"
set_module_property INTERNAL          false
set_module_property OPAQUE_ADDRESS_MAP true

# -----------------------------------------------------------------------------
# Files
# -----------------------------------------------------------------------------
add_fileset QUARTUS_SYNTH QUARTUS_SYNTH "" ""
set_fileset_property QUARTUS_SYNTH TOP_LEVEL audio_subsystem
add_fileset_file audio_subsystem.sv  SYSTEM_VERILOG PATH ../rtl/audio_subsystem.sv  TOP_LEVEL_FILE
add_fileset_file audio_controller.sv SYSTEM_VERILOG PATH ../rtl/audio_controller.sv
add_fileset_file audio_sound_rom.sv  SYSTEM_VERILOG PATH ../rtl/audio_sound_rom.sv
add_fileset_file wm8731_i2s_tx.sv    SYSTEM_VERILOG PATH ../rtl/wm8731_i2s_tx.sv

add_fileset SIM_VERILOG SIM_VERILOG "" ""
set_fileset_property SIM_VERILOG TOP_LEVEL audio_subsystem
add_fileset_file audio_subsystem.sv  SYSTEM_VERILOG PATH ../rtl/audio_subsystem.sv  TOP_LEVEL_FILE
add_fileset_file audio_controller.sv SYSTEM_VERILOG PATH ../rtl/audio_controller.sv
add_fileset_file audio_sound_rom.sv  SYSTEM_VERILOG PATH ../rtl/audio_sound_rom.sv
add_fileset_file wm8731_i2s_tx.sv    SYSTEM_VERILOG PATH ../rtl/wm8731_i2s_tx.sv

# -----------------------------------------------------------------------------
# Parameters
# -----------------------------------------------------------------------------
add_parameter BITS          INTEGER 16
add_parameter SAMPLE_RATE   INTEGER 48000
add_parameter MCLK_DIV_BCLK INTEGER 8
add_parameter ADDR_WIDTH    INTEGER 2

foreach p {BITS SAMPLE_RATE MCLK_DIV_BCLK ADDR_WIDTH} {
    set_parameter_property $p HDL_PARAMETER true
    set_parameter_property $p DERIVED       false
    set_parameter_property $p VISIBLE       true
}

# -----------------------------------------------------------------------------
# Clock + Reset
# -----------------------------------------------------------------------------
add_interface clk clock end
set_interface_property clk clockRate 50000000
add_interface_port clk clk clk Input 1

add_interface reset reset end
set_interface_property reset associatedClock clk
set_interface_property reset synchronousEdges DEASSERT
add_interface_port reset rst reset Input 1

# -----------------------------------------------------------------------------
# Avalon-MM slave (the single 32-bit Sound Select Register)
# -----------------------------------------------------------------------------
add_interface avs avalon end
set_interface_property avs addressUnits         SYMBOLS
set_interface_property avs associatedClock      clk
set_interface_property avs associatedReset      reset
set_interface_property avs bitsPerSymbol         8
set_interface_property avs explicitAddressSpan   0
set_interface_property avs readLatency           1
set_interface_property avs readWaitTime          0
set_interface_property avs writeWaitTime         0
set_interface_property avs timingUnits           Cycles

add_interface_port avs avs_write     write     Input  1
add_interface_port avs avs_read      read      Input  1
add_interface_port avs avs_address   address   Input  2
add_interface_port avs avs_writedata writedata Input  32
add_interface_port avs avs_readdata  readdata  Output 32

# -----------------------------------------------------------------------------
# Codec conduit (drives WM8731 I2S TX pins through the top-level Verilog)
# -----------------------------------------------------------------------------
add_interface codec conduit end
set_interface_property codec associatedClock clk
add_interface_port codec aud_bclk     bclk Output 1
add_interface_port codec aud_dac_lrck lrck Output 1
add_interface_port codec aud_dac_dat  dat  Output 1

# -----------------------------------------------------------------------------
# Status conduit (debug taps)
# -----------------------------------------------------------------------------
add_interface status conduit end
set_interface_property status associatedClock clk
add_interface_port status playing_sfx   playing_sfx   Output 1
add_interface_port status current_track current_track Output 4
add_interface_port status sample_req    sample_req    Output 1
