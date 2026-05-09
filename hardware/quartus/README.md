# GridBrawl Quartus Integration

This folder contains everything you need to wire the GridBrawl SystemVerilog
into a Quartus / Platform Designer project on the Terasic DE1-SoC.

```
hardware/quartus/
├── audio_subsystem_hw.tcl    Platform Designer component descriptor
├── video_pipeline_hw.tcl     Platform Designer component descriptor
├── de1_soc_top.sv            Top-level FPGA wrapper (PLL + Qsys + pads)
├── de1_soc_gridbrawl.sdc     TimeQuest constraints
├── de1_soc_gridbrawl.qsf     Pin assignments (VGA + audio + clock + KEY)
├── gridbrawl_devicetree.dts  DTS fragment (informational; drivers iomap by addr)
└── README.md                 (this file)
```

## Address map (must match the kernel modules)

| Base        | Span    | Slave                                  | Header                              |
| ----------- | ------- | -------------------------------------- | ----------------------------------- |
| 0xFF20_2000 | 0x10    | `audio_subsystem.avs`                  | `kernel_modules/audio/fpga_audio.h` |
| 0xFF20_3000 | 0x200   | `video_pipeline.tile`   (8-bit slave)  | `kernel_modules/video/fpga_video.h` |
| 0xFF20_4000 | 0x80    | `video_pipeline.sprite` (32-bit slave) | `kernel_modules/video/fpga_video.h` |

If you change any address here, edit the matching `*_BASE` macros in the
two kernel headers.

## End-to-end build flow

1. **Start from the Terasic DE1-SoC golden reference** (`DE1_SoC_GHRD`
   or your course's starter project). Copy this whole `hardware/quartus`
   directory next to its `.qpf`.

2. **Add the GridBrawl IP search path** in Quartus:
   `Tools → Options → IP Search Locations → Add` the absolute path to
   `hardware/quartus`. Restart Platform Designer.

3. **Generate a PLL** with the IP catalog
   (`Tools → IP Catalog → ALTERA PLL` or `ALTPLL`):
   - Reference frequency: 50 MHz
   - Output `c0`: 25.175 MHz, 0° (`pix_clk`)
   - Output `c1`: 12.288 MHz, 0° (`mclk`)
   - Locked output: enabled
   - Save as `gridbrawl_pll.qsys` (or `.v`) inside the project, named
     `gridbrawl_pll`. The instance in `de1_soc_top.sv` already matches.

4. **Open / create the Qsys system** as `gridbrawl_qsys`. Add:
   - Existing HPS hard macro (`Cyclone V HPS`)
   - `HPS-to-FPGA AXI Bridge` (or `Lightweight HPS-to-FPGA Bridge`)
   - `GridBrawl Audio Subsystem` and `GridBrawl Video Pipeline`
     (newly visible in the IP catalog after step 2)
   - Two clock sources, exported as `pix_clk` and `mclk`
   - Two reset sources tied to those clocks, exported as `pix_reset`
     and `mclk_reset`
   - Connect both video slaves and the audio slave to the
     `lwhps2fpga` master.
   - Assign base addresses per the table above, and **lock them**
     (right-click the address column → "Lock base address").

5. **Generate the Qsys system**. The output goes to
   `gridbrawl_qsys/synthesis/gridbrawl_qsys.qip`.

6. **Add the generated wrappers to the Quartus project** by uncommenting
   the two `set_global_assignment -name QIP_FILE …` lines in
   `de1_soc_gridbrawl.qsf`, plus the HPS pin assignments from the golden
   `.qsf`.

7. **Compile**. Quartus produces `output_files/de1_soc_top.sof` and (with
   `quartus_cpf`) a `de1_soc_top.rbf` for booting from SD.

8. **Boot the DE1-SoC Linux image**, copy the kernel modules onto the
   board (`make kernel` cross-compiled), and load them:
   ```sh
   insmod fpga_audio.ko
   insmod fpga_video.ko
   ls /dev/fpga_*
   ```

9. **Run the game**:
   ```sh
   ./gridbrawl
   ```
   Use `GRIDBRAWL_DEBUG=1` for once-per-second positional logging, or
   any of `GRIDBRAWL_NO_AUDIO`, `GRIDBRAWL_NO_VIDEO`, `GRIDBRAWL_NO_USB`
   to short-circuit a subsystem during bring-up.

## Wiring inside Qsys (textual sketch)

```
HPS instance ─┬── AXI bridge (full-duplex, mostly unused)
              └── Lightweight HPS-to-FPGA bridge ─┬── audio_subsystem.avs   @ 0xFF20_2000
                                                  ├── video_pipeline.tile   @ 0xFF20_3000
                                                  └── video_pipeline.sprite @ 0xFF20_4000

Clock source pix_clk (25.175 MHz)  ─── video_pipeline.pix_clk
Clock source mclk    (12.288 MHz)  ─── audio_subsystem.clk
Reset bridge        (sync to each) ─── matching reset ports

video_pipeline.vga    conduit  ──→ exported as `vga`
audio_subsystem.codec conduit  ──→ exported as `codec`
```

The conduit names exported here (`pix_clk_clk`, `pix_reset_reset`,
`mclk_clk`, `mclk_reset_reset`, `vga_*`, `codec_*`,
`memory_*`, `hps_io_*`) are the ones referenced inside
`de1_soc_top.sv`. If you choose different conduit prefixes in Qsys,
update the wrapper to match.

## Pin notes

The `.qsf` here covers everything GridBrawl drives directly: 50 MHz
clock, KEY[3:0], the VGA RGB+sync block, and the WM8731 audio codec
(I²S + master clock + I²C control). The HPS DDR3 and USB pads come from
the Terasic golden `.qsf`; copy those lines unchanged.

If you re-target a different board (DE10-Standard, DE10-Nano, …):
- Replace `DEVICE` and the pin numbers in the `.qsf`.
- VGA RGB on DE10-Standard is 8-bit per channel as well; on DE10-Nano
  there is no VGA pad, you'll need an HDMI conversion board.
- The Cyclone V HPS hard macro pins remain the same.

## Bring-up checklist

In rough order of decreasing nuisance value when something doesn't work:

1. `pll_locked` is asserted (probe in SignalTap, or wire to an LED).
2. `vga_visible` toggles -- the timing generator is alive.
3. `tile_readdata` echoes a known-pattern write (write 0x05 to offset 0,
   read back, both via `/dev/fpga_video`).
4. Sprite register 0 reads back the value written: confirm with
   `dd if=/dev/fpga_video bs=4 skip=128 count=1 | xxd` (skip 128 4-byte
   words = byte 0x200 = sprite base).
5. Bring up the WM8731 control registers from Linux (i2cdetect /
   i2cset). Without that, no audio comes out even with a perfect I²S
   stream.
6. Check `dmesg` for `fpga_audio: loaded` and `fpga_video: loaded`.

## Hand-back to software

Once the Qsys system synthesizes and the kernel modules find their
peripherals, no software changes are needed: `gridbrawl` already calls
`render_frame()` every tick, and that pushes a 300-byte tile blob
followed by 30 sprite words to `/dev/fpga_video`, plus a 4-byte sound
ID write to `/dev/fpga_audio` whenever `game_logic` raises an event.
