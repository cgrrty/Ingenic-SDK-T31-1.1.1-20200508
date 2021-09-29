ARCH := mips
CROSS_COMPILE := mips-linux-gnu-
KERNEL_DIR :=../../../../kernel
KSRC := $(KERNEL_DIR)
MODPATH := $(shell pwd)
DRIVER_NAME := jzfb


obj-m := $(DRIVER_NAME).o
$(DRIVER_NAME)-y += jz_fb.o
$(DRIVER_NAME)-y += cfbcopyarea.o
$(DRIVER_NAME)-y += cfbfillrect.o
$(DRIVER_NAME)-y += cfbimgblt.o

modules:
	$(MAKE) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) -C $(KSRC) M=$(MODPATH) modules

.PHONE:clean
clean:
	$(MAKE) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) -C $(KSRC) M=$(MODPATH) clean
