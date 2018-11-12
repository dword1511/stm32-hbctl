PROGRAM    := stm32-hbctl
SRCS       := hbctl.c
CROSS      ?= arm-none-eabi-

###############################################################################

# Clear implicit rules
.SUFFIXES:

AR         := $(CROSS)ar
CC         := $(CROSS)gcc
LD         := $(CROSS)ld
OBJCOPY    := $(CROSS)objcopy
OBJDUMP    := $(CROSS)objdump
SIZE       := $(CROSS)size
NM         := $(CROSS)nm
GDB        := gdb-multiarch

ELF        := $(PROGRAM).elf
BIN        := $(PROGRAM).bin
HEX        := $(PROGRAM).hex
MAP        := $(PROGRAM).map
DMP        := $(PROGRAM).out


TOPDIR     := $(shell pwd)
DEPDIR     := $(TOPDIR)/.dep
OBJS       := $(SRCS:.c=.o)


# Debugging (code has to be bullet-proof since it can cause physical damage!)
# NOTE: Most unsigned constants is missing the U
CFLAGS     += -Wall -Wextra -Wdouble-promotion -gdwarf-4 -g3
#CFLAGS     += -Wconversion -Wno-sign-conversion
# Optimizations
# NOTE: without optimization everything will fail... Computational power is marginal.
CFLAGS     += -O3 -fbranch-target-load-optimize -fipa-pta -frename-registers -fgcse-sm -fgcse-las -fsplit-loops -fstdarg-opt
# Use these for debugging-friendly binary
#CFLAGS     += -O0
CFLAGS     += -Og
# Disabling aggressive loop optimizations since it does not work for loops longer than certain iterations
#CFLAGS     += -fno-aggressive-loop-optimizations
# Aggressive optimizations (unstable or causes huge binaries)
#CFLAGS     += -funroll-loops -fbranch-target-load-optimize2
# Includes
CFLAGS     += -Ilibopencm3/include/ -I$(TOPDIR)
# General for MCU
CFLAGS     += -fno-common -ffunction-sections -fdata-sections -fsingle-precision-constant -ffast-math -flto --specs=nano.specs

# Generate dependency information
CFLAGS     += -MT $@ -MMD -MP -MF $(DEPDIR)/$*.Td
$(shell mkdir -p $(DEPDIR) >/dev/null)
PRECOMPILE  = mkdir -p $(dir $(DEPDIR)/$*.Td)
POSTCOMPILE = mv -f $(DEPDIR)/$*.Td $(DEPDIR)/$*.d && touch $@

# Architecture-dependent
LDSCRIPT   := libopencm3/lib/stm32/f0/stm32f04xz6.ld
OPENCM3_MK := lib/stm32/f0
LIBOPENCM3 := libopencm3/lib/libopencm3_stm32f0.a
FP_FLAGS   := -msoft-float
CFLAGS     += -DSTM32F0 -mthumb -mcpu=cortex-m0 $(FP_FLAGS)
# LDPATH is required for libopencm3's ld scripts to work.
LDPATH     := libopencm3/lib/
# NOTE: the rule will ensure CFLAGS are added during linking
LDFLAGS    += -nostdlib -L$(LDPATH) -T$(LDSCRIPT) -Wl,-Map -Wl,$(MAP) -Wl,--gc-sections -Wl,--relax
LDLIBS     += $(LIBOPENCM3) -lc -lgcc


# Pass these to libopencm3's Makefile
PREFIX     := $(patsubst %-,%,$(CROSS))
V          := 0
export FP_FLAGS CFLAGS PREFIX V


default: $(BIN) $(HEX) $(DMP) size

$(ELF): $(LDSCRIPT) $(OBJS) $(LIBOPENCM3) Makefile
	$(CC) -o $@ $(CFLAGS) $(LDFLAGS) $(OBJS) $(LDLIBS)

$(DMP): $(ELF)
	$(OBJDUMP) -d $< > $@

%.hex: %.elf
	$(OBJCOPY) -S -O ihex   $< $@

%.bin: %.elf
	$(OBJCOPY) -S -O binary $< $@

%.o: %.c $(LIBOPENCM3) Makefile
	$(PRECOMPILE)
	$(CC) $(CFLAGS) -c $< -o $@
	$(POSTCOMPILE)

# NOTE: libopencm3's Makefile is unaware of top-level Makefile changes, so force remake
# ld script also obtained here. However, has to touch it to prevent unnecessary remakes
$(LIBOPENCM3) $(LDSCRIPT): Makefile
	git submodule update --init
	make -B -C libopencm3 $(OPENCM3_MK)
	touch $(LDSCRIPT)


.PHONY: default clean distclean size symbols symbols_bss flash erase debug monitor check

clean:
	rm -f $(OBJS) $(ELF) $(HEX) $(BIN) $(MAP) $(DMP)
	rm -rf $(DEPDIR)

distclean: clean
	make -C libopencm3 clean
	rm -f *.o *~ *.swp *.hex

# These targets want clean terminal output
size: $(ELF)
	@echo ""
	@$(SIZE) $<
	@echo ""

symbols: $(ELF)
	@$(NM) --demangle --size-sort -S $< | grep -v ' [bB] '

symbols_bss: $(ELF)
	@$(NM) --demangle --size-sort -S $< | grep ' [bB] '

flash: $(HEX)
	@killall st-util || echo
	@st-flash --reset --format ihex write $<

erase:
	@killall st-util || echo
	@st-flash erase

debug: $(ELF) flash
	@killall st-util || echo
	@setsid st-util &
	@-$(GDB) $< -q -ex 'target extended-remote localhost:4242'

# Dependencies
$(DEPDIR)/%.d:
.PRECIOUS: $(DEPDIR)/%.d
include $(wildcard $(patsubst %,$(DEPDIR)/%.d,$(basename $(SRCS))))
