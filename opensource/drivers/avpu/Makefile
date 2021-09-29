# real (hardware) driver
CROSS_COMPILE ?= mips-linux-gnu-

AVPU_NO_DMABUF ?= 0

ifeq (${ISP_ENV_KERNEL_DIR}, )
	ISP_ENV_KERNEL_DIR = $(PWD)/../../kernel
endif

KDIR := ${ISP_ENV_KERNEL_DIR}

################### name define #########
MODULE_NAME := avpu
################### name end #########

all: modules tzctrl

.PHONY: modules tzctrl clean

EXTRA_CFLAGS += -I$(PWD)/include

$(MODULE_NAME)-objs := avpu_main.o avpu_ip.o avpu_alloc.o avpu_alloc_ioctl.o

ifeq ($(AVPU_NO_DMABUF),1)
  $(MODULE_NAME)-objs += avpu_no_dmabuf.o
else
  $(MODULE_NAME)-objs += avpu_dmabuf.o
endif

obj-m := $(MODULE_NAME).o

modules:
	@$(MAKE) -C $(KDIR) M=$(shell pwd) $@

clean:
	@find . -name '*.o' -o -name '*~' -o -name '.depend' -o -name '.*.cmd' \
		-o -name '*.mod.c' -o -name '.tmp_versions' -o -name '*.ko' \
		-o -name '*.symvers' -o -name 'modules.order' | xargs rm -rf
