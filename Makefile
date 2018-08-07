LIBOPENCM3  = libopencm3/lib/libopencm3_stm32f0.a

include common.mk

# libopencm3 enables FPU on CM4 by default
ARCH_FLAGS  = -DSTM32F0 -mthumb -mcpu=cortex-m0 -msoft-float
LDSCRIPT    = libopencm3/lib/stm32/f0/stm32f05xz8.ld
OPENCM3_MK  = lib/stm32/f0

.PHONY: flash test debug

flash: $(HEX)
	@st-flash --reset --format ihex write $<

test: $(BIN) $(ELF)
	@$(XTERM) st-util --serial $(SERIAL) &
	@$(GDB) $(ELF) -q -x gdb-stlink.cmd

debug: $(BIN) $(ELF)
	@killall st-util || echo -n
	@$(XTERM) st-util &
	@$(GDB) $(ELF) -q -ex 'target extended-remote localhost:4242'
