CROSS_COMPILE ?= mips-linux-gnu-

ifeq (${ISVP_ENV_KERNEL_DIR}, )
	ISVP_ENV_KERNEL_DIR = $(PWD)/../../../kernel
endif

KDIR := ${ISVP_ENV_KERNEL_DIR}
MODULE_NAME := audio

all: modules

.PHONY: modules clean

EXTRA_CFLAGS += -I$(PWD)/include

ifeq (${CONFIG_JZ_TS_DMIC}, y)
$(MODULE_NAME)-objs := devices/ex_codecs/codec_i2c_dev.o devices/ex_codecs/es8374_codec.o devices/codecs/jz_t10_codec.o devices/xb47xx_i2s_v12.o	\
					   devices/dmic/mic.o devices/dmic/mic_sys.o devices/dmic/mic_ops.o devices/dmic/mic_dma.o devices/dmic/mic_hrtimer.o\
					   devices/dmic/dmic_hal.o\
					   interface/xb_snd_dsp.o xb_snd_card.o
else

$(MODULE_NAME)-objs := devices/ex_codecs/codec_i2c_dev.o devices/ex_codecs/es8374_codec.o devices/codecs/jz_t10_codec.o devices/xb47xx_i2s_v12.o	\
						interface/xb_snd_dsp.o xb_snd_card.o
endif
obj-m := $(MODULE_NAME).o

modules:
	@$(MAKE) -C $(KDIR) M=$(shell pwd) $@

clean:
	@find . -name '*.o' -o -name '*~' -o -name '.depend' -o -name '.*.cmd' \
		-o -name '*.mod.c' -o -name '.tmp_versions' -o -name '*.ko' \
		-o -name '*.symvers' -o -name 'modules.order' | xargs rm -rf
