# AAMD@home step-1 GPU compute probe.
#
#   make host-test          # CPU saxpy on the build machine
#   make cpu                # ELF payload (needs PS5_PAYLOAD_SDK)
#   make vulkan             # native title PPSA99021 (needs PS5_VULKAN + RADV)
#   make gl                 # OpenGL 4.3 fallback title (needs PS5_OPENGL_PREFIX)

ROOT := $(abspath .)
HOST_CC ?= cc
NPROC := $(shell nproc 2>/dev/null || echo 2)

.PHONY: all host-test cpu vulkan gl distclean clean

all: host-test

host-test: build/host/test_saxpy
	./build/host/test_saxpy

build/host/test_saxpy: probes/00-vulkan-saxpy/host/test_saxpy.c probes/00-vulkan-saxpy/src/aamd_saxpy.c probes/00-vulkan-saxpy/include/aamd_probe.h
	mkdir -p build/host
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -pthread -Iprobes/00-vulkan-saxpy/include \
		-o $@ probes/00-vulkan-saxpy/host/test_saxpy.c probes/00-vulkan-saxpy/src/aamd_saxpy.c -lm

cpu:
	$(MAKE) -C probes/00-vulkan-saxpy/cpu

vulkan:
	bash probes/00-vulkan-saxpy/vulkan/build.sh

gl:
	$(MAKE) -C probes/00-vulkan-saxpy/gl

clean:
	$(MAKE) -C probes/00-vulkan-saxpy/cpu clean || true
	rm -rf build dist
	rm -f probes/00-vulkan-saxpy/cpu/aamd-probe.elf probe.log

distclean: clean
