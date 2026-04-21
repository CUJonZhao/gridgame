#!/usr/bin/env bash
# audio_loopback_test.sh
#
# Purpose: verify audio_interface.c end-to-end in a VM, without a DE1-SoC.
# It points audio_interface at a regular file instead of /dev/fpga_audio,
# runs audio_test, then checks the bytes that were written match what the
# kernel driver would expect to see ("1\n2\n4\n3\n").
#
# Usage:
#   chmod +x audio_loopback_test.sh
#   ./audio_loopback_test.sh

set -e

HERE="$(cd "$(dirname "$0")" && pwd)"
cd "$HERE"

MOCK=/tmp/fake_fpga_audio
rm -f "$MOCK"

# Build: we only need the userspace parts; game.h is pure header.
gcc -Wall -Wextra -o audio_test audio_test.c audio_interface.c

# Redirect the interface to the mock file.
# Also patch audio_test to not sleep 2s between sounds in the VM.
FPGA_AUDIO_DEV="$MOCK" ./audio_test

echo
echo "---- bytes written to $MOCK ----"
cat "$MOCK"
echo "---- end ----"

# Use xxd so trailing newlines survive comparison (bash $() strips them).
EXPECTED_HEX="$(printf '1\n2\n4\n3\n' | xxd -p)"
ACTUAL_HEX="$(xxd -p < "$MOCK")"
if [ "$ACTUAL_HEX" = "$EXPECTED_HEX" ]; then
    echo "PASS: loopback bytes match what fpga_audio.c's kstrtouint would accept."
else
    echo "FAIL: unexpected bytes."
    echo "expected hex: $EXPECTED_HEX"
    echo "actual hex:   $ACTUAL_HEX"
    exit 1
fi
