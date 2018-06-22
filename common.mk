PROGRAM     = stm32-hbctl
OBJS        = hbctl.o uart.o
SERIAL     ?= /dev/ttyUSB0
STLINKPORT ?= /dev/ttyACM0
CROSS      ?= arm-none-eabi-
QEMU       ?= ~/qemu-stm32/bin/qemu-system-arm
XTERM      ?= x-terminal-emulator --hold -e

###############################################################################

AR          = $(CROSS)ar
CC          = $(CROSS)gcc
LD          = $(CROSS)ld
OBJCOPY     = $(CROSS)objcopy
OBJDUMP     = $(CROSS)objdump
SIZE        = $(CROSS)size
NM          = $(CROSS)nm
GDB         = $(CROSS)gdb

ELF         = $(PROGRAM).elf
BIN         = $(PROGRAM).bin
HEX         = $(PROGRAM).hex
MAP         = $(PROGRAM).map
DMP         = $(PROGRAM).out

CFLAGS     += -O2 -Wall -g3 -gdwarf
# Turning aggressive loop optimizations off since it does not work for loops longer than certain iterations
CFLAGS     += -fno-common -ffunction-sections -fdata-sections -fno-aggressive-loop-optimizations
CFLAGS     += -fbranch-target-load-optimize
# Aggressive optimizations
#CFLAGS     += -O3 -fgcse-sm -fgcse-las -fgcse-after-reload -funroll-loops -funswitch-loops
# Actually won't wotk
#CFLAGS     += -flto -fipa-pta
# Architecture-dependent
CFLAGS     += $(ARCH_FLAGS) -Ilibopencm3/include/ $(EXTRA_CFLAGS)

# LDPATH is required for libopencm3 ld scripts to work.
LDPATH      = libopencm3/lib/
LDFLAGS    += $(ARCH_FLAGS) -nostdlib -L$(LDPATH) -T$(LDSCRIPT) -Wl,-Map -Wl,$(MAP) -Wl,--gc-sections -Wl,--relax -flto
LDLIBS     += $(LIBOPENCM3) -lc -lgcc


all: $(LIBOPENCM3) $(BIN) $(HEX) $(DMP) size

$(ELF): $(LDSCRIPT) $(OBJS)
	$(CC) -o $@ $(LDFLAGS) $(OBJS) $(LDLIBS)

$(DMP): $(ELF)
	$(OBJDUMP) -d $< > $@

%.hex: %.elf
	$(OBJCOPY) -S -O ihex   $< $@

%.bin: %.elf
	$(OBJCOPY) -S -O binary $< $@

%.o: %.c lib/*.h config.mk
	$(CC) $(CFLAGS) $(CFGFLAGS) -c $< -o $@

$(LIBOPENCM3):
	# TODO: pass toolchain prefix
	git submodule init
	git submodule update --remote
	make -C libopencm3 CFLAGS="$(CFLAGS)" $(OPENCM3_MK) V=1


.PHONY: clean distclean size symbols monitor

clean:
	rm -f $(OBJS) $(DOCS) $(ELF) $(HEX) $(BIN) $(MAP) $(DMP) DAC_OUT_PUT1.txt DAC_OUT_PUT2.txt

distclean: clean
	make -C libopencm3 clean
	rm -f *~ *.swp *.hex log.out

# These targets want clean terminal output
size: $(ELF)
	@echo ""
	@$(SIZE) $<
	@echo ""

symbols: $(ELF)
	@$(NM) --demangle --size-sort -S $< | grep -v ' [bB] '

monitor:
	@stty -F $(SERIAL) raw 115200; cat $(SERIAL)
