# GridBrawl hardware simulation Makefile.
#
# Usage:
#   make vga_timing            # compile + run iverilog testbench
#   make tile_map              # verify tile map RAM + background renderer
#   make sprite_registers      # verify sprite register packing/readback
#   make sprite_renderer       # verify one-sprite RGB compositing
#   make sprite_scan_renderer  # verify 30-sprite priority compositing
#   make video_pipeline        # verify integrated VGA pipeline smoke test
#   make vga_timing_wave       # same, then open .vcd in gtkwave
#   make clean
#
# Each test is one entry in TESTS below; for new modules, copy the pattern.
#
# Quartus Prime Lite users: ModelSim Starter ships with Quartus and can
# also load these files. From a "Quartus Prime ModelSim" shell:
#   vlib work
#   vlog ../rtl/vga_timing.sv vga_timing_tb.sv
#   vsim -c -do "run -all; quit" vga_timing_tb

IVERILOG ?= iverilog
VVP      ?= vvp
GTKWAVE  ?= gtkwave

RTL_DIR  := ../rtl

TESTS := vga_timing tile_map sprite_registers sprite_renderer sprite_scan_renderer video_pipeline

.PHONY: all clean $(TESTS) $(addsuffix _wave,$(TESTS))

all: $(TESTS)

# vga_timing test
vga_timing: vga_timing_tb.vvp
	$(VVP) $<

vga_timing_wave: vga_timing
	$(GTKWAVE) vga_timing_tb.vcd &

vga_timing_tb.vvp: $(RTL_DIR)/vga_timing.sv vga_timing_tb.sv
	$(IVERILOG) -g2012 -o $@ $^

# Tile map RAM + renderer test
tile_map: tile_map_tb.vvp
	$(VVP) $<

tile_map_wave: tile_map
	$(GTKWAVE) tile_map_tb.vcd &

tile_map_tb.vvp: $(RTL_DIR)/tile_map_ram.sv $(RTL_DIR)/tile_map_renderer.sv tile_map_tb.sv
	$(IVERILOG) -g2012 -o $@ $^

# Sprite register test
sprite_registers: sprite_registers_tb.vvp
	$(VVP) $<

sprite_registers_wave: sprite_registers
	$(GTKWAVE) sprite_registers_tb.vcd &

sprite_registers_tb.vvp: $(RTL_DIR)/sprite_registers.sv sprite_registers_tb.sv
	$(IVERILOG) -g2012 -o $@ $^

# Sprite renderer test
sprite_renderer: sprite_renderer_tb.vvp
	$(VVP) $<

sprite_renderer_wave: sprite_renderer
	$(GTKWAVE) sprite_renderer_tb.vcd &

sprite_renderer_tb.vvp: $(RTL_DIR)/sprite_image_rom.sv $(RTL_DIR)/sprite_renderer.sv sprite_renderer_tb.sv
	$(IVERILOG) -g2012 -o $@ $^

# Full sprite scan renderer test
sprite_scan_renderer: sprite_scan_renderer_tb.vvp
	$(VVP) $<

sprite_scan_renderer_wave: sprite_scan_renderer
	$(GTKWAVE) sprite_scan_renderer_tb.vcd &

sprite_scan_renderer_tb.vvp: $(RTL_DIR)/sprite_image_rom.sv $(RTL_DIR)/sprite_scan_renderer.sv sprite_scan_renderer_tb.sv
	$(IVERILOG) -g2012 -o $@ $^

# Integrated video pipeline smoke test
video_pipeline: video_pipeline_tb.vvp
	$(VVP) $<

video_pipeline_wave: video_pipeline
	$(GTKWAVE) video_pipeline_tb.vcd &

video_pipeline_tb.vvp: $(RTL_DIR)/vga_timing.sv $(RTL_DIR)/tile_map_ram.sv $(RTL_DIR)/tile_map_renderer.sv $(RTL_DIR)/sprite_registers.sv $(RTL_DIR)/sprite_image_rom.sv $(RTL_DIR)/sprite_scan_renderer.sv $(RTL_DIR)/video_pipeline.sv video_pipeline_tb.sv
	$(IVERILOG) -g2012 -o $@ $^

clean:
	rm -f *.vvp *.vcd
