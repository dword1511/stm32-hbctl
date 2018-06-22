LIBOPENCM3  = libopencm3/lib/libopencm3_stm32f0.a

include common.mk

ARCH_FLAGS  = -DSTM32F0 -mthumb -mcpu=cortex-m0 -msoft-float
LDSCRIPT    = libopencm3/lib/stm32/f0/stm32f05xz8.ld
OPENCM3_MK  = lib/stm32/f0

.PHONY: flash test

flash: $(HEX)
	@killall st-util || echo -n
	@st-flash --reset --format ihex write $<

test: $(BIN) $(ELF)
	@killall st-util || echo -n
	@$(XTERM) st-util --serial $(SERIAL) &
	@$(GDB) $(ELF) -q -x gdb-stlink.cmd

erase:
	st-flash erase
