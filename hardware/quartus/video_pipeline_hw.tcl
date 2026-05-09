# =============================================================================
# video_pipeline_hw.tcl -- Platform Designer (Qsys) component descriptor for
# the GridBrawl integrated video pipeline.
#
# Two Avalon-MM slave ports are exposed:
#   tile   : 8-bit data, byte-addressed, span 0x200 (300 bytes used).
#   sprite : 32-bit data, byte-addressed, span 0x80  (120 bytes used).
#
# Plus a single conduit carrying the VGA output and a few debug taps.
#
# Place this file under hardware/quartus/ and add that directory to the
# IP search path inside Quartus / Platform Designer:
#   Tools -> Options -> IP Search Locations -> Add ../hardware/quartus
#
# After adding, the component appears as "GridBrawl Video Pipeline" inside
# the Qsys library and can be dragged into the system, then connected to:
#   * the HPS-to-FPGA Lightweight bridge (both slaves)
#   * the pixel clock + reset
#   * the chip-level VGA pins via a conduit
# =============================================================================

package require -exact qsys 16.1

# -----------------------------------------------------------------------------
# Module
# -----------------------------------------------------------------------------
set_module_property NAME         video_pipeline
set_module_property DISPLAY_NAME "GridBrawl Video Pipeline"
set_module_property VERSION      1.0
set_module_property GROUP        "GridBrawl"
set_module_property DESCRIPTION  "VGA timing + tile RAM + 30 sprite registers + scan compositor"
set_module_property AUTHOR       "GridBrawl Team"
set_module_property INTERNAL          false
set_module_property OPAQUE_ADDRESS_MAP true

# -----------------------------------------------------------------------------
# Files
# -----------------------------------------------------------------------------
add_fileset QUARTUS_SYNTH QUARTUS_SYNTH "" ""
set_fileset_property QUARTUS_SYNTH TOP_LEVEL video_pipeline
add_fileset_file video_pipeline.sv     SYSTEM_VERILOG PATH ../rtl/video_pipeline.sv     TOP_LEVEL_FILE
add_fileset_file vga_timing.sv         SYSTEM_VERILOG PATH ../rtl/vga_timing.sv
add_fileset_file tile_map_ram.sv       SYSTEM_VERILOG PATH ../rtl/tile_map_ram.sv
add_fileset_file tile_map_renderer.sv  SYSTEM_VERILOG PATH ../rtl/tile_map_renderer.sv
add_fileset_file sprite_registers.sv   SYSTEM_VERILOG PATH ../rtl/sprite_registers.sv
add_fileset_file sprite_image_rom.sv   SYSTEM_VERILOG PATH ../rtl/sprite_image_rom.sv
add_fileset_file sprite_scan_renderer.sv SYSTEM_VERILOG PATH ../rtl/sprite_scan_renderer.sv

add_fileset SIM_VERILOG SIM_VERILOG "" ""
set_fileset_property SIM_VERILOG TOP_LEVEL video_pipeline
add_fileset_file video_pipeline.sv     SYSTEM_VERILOG PATH ../rtl/video_pipeline.sv     TOP_LEVEL_FILE
add_fileset_file vga_timing.sv         SYSTEM_VERILOG PATH ../rtl/vga_timing.sv
add_fileset_file tile_map_ram.sv       SYSTEM_VERILOG PATH ../rtl/tile_map_ram.sv
add_fileset_file tile_map_renderer.sv  SYSTEM_VERILOG PATH ../rtl/tile_map_renderer.sv
add_fileset_file sprite_registers.sv   SYSTEM_VERILOG PATH ../rtl/sprite_registers.sv
add_fileset_file sprite_image_rom.sv   SYSTEM_VERILOG PATH ../rtl/sprite_image_rom.sv
add_fileset_file sprite_scan_renderer.sv SYSTEM_VERILOG PATH ../rtl/sprite_scan_renderer.sv

# -----------------------------------------------------------------------------
# Parameters (kept identical to the SystemVerilog defaults so a fresh drop in
# Qsys "just works" without manual editing)
# -----------------------------------------------------------------------------
add_parameter SCREEN_WIDTH      INTEGER 640
add_parameter SCREEN_HEIGHT     INTEGER 480
add_parameter MAP_COLS          INTEGER 20
add_parameter MAP_ROWS          INTEGER 15
add_parameter SPRITE_COUNT      INTEGER 30

set_parameter_property SCREEN_WIDTH    DEFAULT_VALUE 640
set_parameter_property SCREEN_HEIGHT   DEFAULT_VALUE 480
set_parameter_property MAP_COLS        DEFAULT_VALUE 20
set_parameter_property MAP_ROWS        DEFAULT_VALUE 15
set_parameter_property SPRITE_COUNT    DEFAULT_VALUE 30

foreach p {SCREEN_WIDTH SCREEN_HEIGHT MAP_COLS MAP_ROWS SPRITE_COUNT} {
    set_parameter_property $p HDL_PARAMETER true
    set_parameter_property $p DERIVED       false
    set_parameter_property $p VISIBLE       true
}

# Derived widths -- not user-visible, computed from the visible parameters.
add_parameter TILE_ADDR_WIDTH    INTEGER 9
add_parameter SPRITE_ADDR_WIDTH  INTEGER 7
add_parameter SPRITE_INDEX_WIDTH INTEGER 5
foreach p {TILE_ADDR_WIDTH SPRITE_ADDR_WIDTH SPRITE_INDEX_WIDTH} {
    set_parameter_property $p HDL_PARAMETER true
    set_parameter_property $p DERIVED       true
    set_parameter_property $p VISIBLE       false
}

# -----------------------------------------------------------------------------
# Clock + Reset
# -----------------------------------------------------------------------------
add_interface pix_clk clock end
set_interface_property pix_clk clockRate 25175000
add_interface_port pix_clk pix_clk clk Input 1

add_interface reset reset end
set_interface_property reset associatedClock pix_clk
set_interface_property reset synchronousEdges DEASSERT
add_interface_port reset rst reset Input 1

# -----------------------------------------------------------------------------
# Tile Map RAM Avalon-MM slave (8-bit byte slave)
# -----------------------------------------------------------------------------
add_interface tile avalon end
set_interface_property tile addressUnits         SYMBOLS
set_interface_property tile associatedClock      pix_clk
set_interface_property tile associatedReset      reset
set_interface_property tile bridgesToMaster      ""
set_interface_property tile burstOnBurstBoundariesOnly false
set_interface_property tile bitsPerSymbol         8
set_interface_property tile explicitAddressSpan   0
set_interface_property tile holdTime              0
set_interface_property tile linewrapBursts        false
set_interface_property tile maximumPendingReadTransactions 0
set_interface_property tile readLatency           1
set_interface_property tile readWaitTime          0
set_interface_property tile setupTime             0
set_interface_property tile timingUnits           Cycles
set_interface_property tile writeWaitTime         0

add_interface_port tile tile_write     write     Input  1
add_interface_port tile tile_read      read      Input  1
add_interface_port tile tile_address   address   Input  9
add_interface_port tile tile_writedata writedata Input  8
add_interface_port tile tile_readdata  readdata  Output 8

# -----------------------------------------------------------------------------
# Sprite Registers Avalon-MM slave (32-bit, byte-addressed)
# -----------------------------------------------------------------------------
add_interface sprite avalon end
set_interface_property sprite addressUnits         SYMBOLS
set_interface_property sprite associatedClock      pix_clk
set_interface_property sprite associatedReset      reset
set_interface_property sprite bitsPerSymbol         8
set_interface_property sprite explicitAddressSpan   0
set_interface_property sprite readLatency           1
set_interface_property sprite readWaitTime          0
set_interface_property sprite writeWaitTime         0
set_interface_property sprite timingUnits           Cycles

add_interface_port sprite sprite_write     write     Input  1
add_interface_port sprite sprite_read      read      Input  1
add_interface_port sprite sprite_address   address   Input  7
add_interface_port sprite sprite_writedata writedata Input  32
add_interface_port sprite sprite_readdata  readdata  Output 32

# -----------------------------------------------------------------------------
# VGA conduit (drives the chip-level VGA pins through a top-level Verilog file)
# -----------------------------------------------------------------------------
add_interface vga conduit end
set_interface_property vga associatedClock pix_clk
set_interface_property vga associatedReset reset
add_interface_port vga vga_hsync   hsync   Output 1
add_interface_port vga vga_vsync   vsync   Output 1
add_interface_port vga vga_visible visible Output 1
add_interface_port vga vga_r       r       Output 8
add_interface_port vga vga_g       g       Output 8
add_interface_port vga vga_b       b       Output 8

# -----------------------------------------------------------------------------
# Debug conduit -- pixel coordinates, frame strobe, sprite hit info.
# Optional: leave unconnected in production.
# -----------------------------------------------------------------------------
add_interface debug conduit end
set_interface_property debug associatedClock pix_clk
add_interface_port debug pixel_x                pixel_x         Output 10
add_interface_port debug pixel_y                pixel_y         Output 10
add_interface_port debug new_frame              new_frame       Output 1
add_interface_port debug debug_tile_address     tile_address    Output 9
add_interface_port debug debug_tile_id          tile_id         Output 3
add_interface_port debug debug_sprite_hit       sprite_hit      Output 1
add_interface_port debug debug_hit_sprite_index hit_index       Output 5
