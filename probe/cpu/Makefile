# AAMD@home CPU saxpy ELF payload (elfldr TCP 9021).

PS5_HOST ?= ps5
PS5_PORT ?= 9021

ifdef PS5_PAYLOAD_SDK
 include $(PS5_PAYLOAD_SDK)/toolchain/prospero.mk
else
 $(error PS5_PAYLOAD_SDK is undefined)
endif

ROOT := $(abspath ../..)
ELF := aamd-probe.elf
CFLAGS := -Wall -Werror -O2 -D__PROSPERO__ -I$(ROOT)/probe/include
SRCS := main.c $(ROOT)/probe/src/aamd_log.c $(ROOT)/probe/src/aamd_saxpy.c

all: $(ELF)

$(ELF): $(SRCS) $(ROOT)/probe/include/aamd_probe.h
	$(CC) $(CFLAGS) -pthread -o $@ $(SRCS)

clean:
	rm -f $(ELF)

test: $(ELF)
	$(PS5_DEPLOY) -h $(PS5_HOST) -p $(PS5_PORT) $^
