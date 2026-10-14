# OpenGL 4.3 compute fallback. Needs a downloaded ps5-opengl SDK:
#   PS5_OPENGL_PREFIX=/path/to/ps5-opengl-sdk/sdk
#   PS5_PAYLOAD_SDK=/opt/ps5-payload-sdk
# Packaging into PPSA99021 still requires the native-app boilerplate /
# PS5_Vulkan ps5-native-tool. This target only produces gl-probe.elf.

ifdef PS5_PAYLOAD_SDK
 include $(PS5_PAYLOAD_SDK)/toolchain/prospero.mk
else
 $(error PS5_PAYLOAD_SDK is undefined)
endif

ifndef PS5_OPENGL_PREFIX
 $(error PS5_OPENGL_PREFIX is undefined (ps5-opengl SDK sdk/ directory))
endif

include $(PS5_OPENGL_PREFIX)/share/ps5-opengl/ps5-opengl.mk

ROOT := $(abspath ../../..)
CFLAGS += -Wall -Werror -O2 -D__PROSPERO__ -I$(ROOT)/probes/00-vulkan-saxpy/include $(PS5_OPENGL_PUBLIC_CFLAGS)
SRCS := probe.c $(ROOT)/probes/00-vulkan-saxpy/src/aamd_log.c $(ROOT)/probes/00-vulkan-saxpy/src/aamd_saxpy.c

all: gl-probe.elf

gl-probe.elf: $(SRCS)
	$(CXX) $(CFLAGS) -o $@ $(SRCS) $(PS5_OPENGL_LDLIBS) $(PS5_OPENGL_LDFLAGS)

clean:
	rm -f gl-probe.elf
