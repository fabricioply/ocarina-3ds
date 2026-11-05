# ocarina.3gx - Luma3DS plugin for Ocarina of Time 3D (microphone -> ocarina songs).
# Needs devkitARM + libctru (devkitPro) and 3gxtool on the PATH or in $(DEVKITPRO)/tools/bin. See README.md.
ifeq ($(strip $(DEVKITPRO)),)
$(error DEVKITPRO is not set (for example: export DEVKITPRO=/opt/devkitpro))
endif
ifeq ($(strip $(DEVKITARM)),)
$(error DEVKITARM is not set (for example: export DEVKITARM=$$DEVKITPRO/devkitARM))
endif

export PATH := $(DEVKITARM)/bin:$(DEVKITPRO)/tools/bin:$(PATH)

OUT   := ocarina
CTRU  := $(DEVKITPRO)/libctru
SRC   := src
RECOG := src/recog

SRCS := $(SRC)/start.s $(SRC)/ocarina.c \
        $(SRC)/hook.c $(SRC)/scan.c $(SRC)/fslog.c $(SRC)/freerange.c $(SRC)/ringcopy.c $(SRC)/logbuf.c \
        $(SRC)/inject.c $(SRC)/dlg.c $(SRC)/dlgmem.c $(SRC)/dlglog.c $(SRC)/drv.c $(SRC)/integ.c \
        $(RECOG)/pitch.c $(RECOG)/notes.c $(RECOG)/matcher.c $(RECOG)/catalog.c
HDRS := $(wildcard $(SRC)/*.h) $(wildcard $(RECOG)/*.h) plugin.ld

CFLAGS := -std=c99 -O2 -Wall -march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft -ffreestanding \
          -fno-stack-protector -fno-builtin -fstack-usage -I$(SRC) -I$(RECOG) -I$(CTRU)/include

.PHONY: all clean
all: $(OUT).3gx

$(OUT).elf: $(SRCS) $(HDRS)
	arm-none-eabi-gcc $(CFLAGS) -nostartfiles -nostdlib -T plugin.ld -Wl,--gc-sections -Wl,-Map,$(OUT).map \
		$(SRCS) -L$(CTRU)/lib -lctru -o $@

$(OUT).3gx: $(OUT).elf ocarina.plgInfo
	3gxtool -s $(OUT).elf ocarina.plgInfo $@

clean:
	rm -f $(OUT).elf $(OUT).3gx $(OUT).map *.su
