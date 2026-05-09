# Linux kernel module Makefile for fpga_audio.
#
# Native build (against the running kernel):
#   make
#
# Cross build for DE1-SoC:
#   make KDIR=/path/to/socfpga-kernel \
#        ARCH=arm \
#        CROSS_COMPILE=arm-linux-gnueabihf-

obj-m += fpga_audio.o

KDIR ?= /lib/modules/$(shell uname -r)/build
PWD  := $(shell pwd)

.PHONY: all clean

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
