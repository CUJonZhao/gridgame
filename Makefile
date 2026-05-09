# GridBrawl top-level Makefile.
#
# Usage:
#   make                 # build the user-space game binary
#   make game            # same
#   make kernel          # build the FPGA audio kernel module
#   make tests           # run game logic + audio loopback tests
#   make clean           # remove all build artifacts
#
# Cross-compiling for the DE1-SoC target:
#   make game   CC=arm-linux-gnueabihf-gcc
#   make kernel KDIR=/path/to/socfpga/kernel \
#               ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf-

CROSS_COMPILE ?=
CC            := $(CROSS_COMPILE)gcc

CFLAGS  ?= -Wall -Wextra -O2 -std=c99 -D_POSIX_C_SOURCE=200809L
INCLUDE := -Igame
LDLIBS  := -lpthread -lusb-1.0

GAME_DIR  := game
GAME_SRCS := $(GAME_DIR)/main.c \
             $(GAME_DIR)/game_logic.c \
             $(GAME_DIR)/audio_interface.c \
             $(GAME_DIR)/usbcontroller.c
GAME_OBJS := $(GAME_SRCS:.c=.o)
GAME_BIN  := gridbrawl

KERNEL_AUDIO_DIR := kernel_modules/audio

.PHONY: all game kernel tests game-test audio-test clean distclean

all: game

game: $(GAME_BIN)

$(GAME_BIN): $(GAME_OBJS)
	$(CC) $(CFLAGS) $(INCLUDE) -o $@ $^ $(LDLIBS)

$(GAME_DIR)/%.o: $(GAME_DIR)/%.c
	$(CC) $(CFLAGS) $(INCLUDE) -c -o $@ $<

kernel:
	$(MAKE) -C $(KERNEL_AUDIO_DIR)

tests: game-test audio-test

game-test:
	cd tests && bash game_logic_test.sh

audio-test:
	cd tests && bash audio_loopback_test.sh

clean:
	-rm -f $(GAME_OBJS) $(GAME_BIN)
	-rm -f tests/game_logic_test tests/audio_test
	-$(MAKE) -C $(KERNEL_AUDIO_DIR) clean

distclean: clean
	rm -f /tmp/fake_fpga_audio
