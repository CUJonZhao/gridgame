#!/usr/bin/env bash
set -e

HERE="$(cd "$(dirname "$0")" && pwd)"
cd "$HERE"

GAME_DIR="../game"
MOCK=/tmp/fake_fpga_audio
rm -f "$MOCK"

gcc -Wall -Wextra -I"$GAME_DIR" -o audio_test \
    audio_test.c "$GAME_DIR/audio_interface.c"

FPGA_AUDIO_DEV="$MOCK" ./audio_test

echo
echo "---- bytes written to $MOCK ----"
cat "$MOCK"
echo "---- end ----"

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
