#!/usr/bin/env bash
set -e

HERE="$(cd "$(dirname "$0")" && pwd)"
cd "$HERE"

GAME_DIR="../game"

gcc -Wall -Wextra -I"$GAME_DIR" -o game_logic_test \
    game_logic_test.c "$GAME_DIR/game_logic.c"

./game_logic_test
