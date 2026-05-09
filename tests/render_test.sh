#!/usr/bin/env bash
set -e

HERE="$(cd "$(dirname "$0")" && pwd)"
cd "$HERE"

GAME_DIR="../game"
MOCK=/tmp/fake_fpga_video
rm -f "$MOCK"

gcc -Wall -Wextra -std=c99 -D_POSIX_C_SOURCE=200809L \
    -I"$GAME_DIR" -o render_test \
    render_test.c \
    "$GAME_DIR/render.c" \
    "$GAME_DIR/video_interface.c"

./render_test
