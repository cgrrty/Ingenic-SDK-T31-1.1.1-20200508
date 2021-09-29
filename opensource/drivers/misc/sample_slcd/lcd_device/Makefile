ARCH := mips
CROSS_COMPILE := mips-linux-gnu-
KERNEL_DIR :=../../../../kernel
KSRC := $(KERNEL_DIR)
MODPATH := $(shell pwd)
DRIVER_NAME := lcd-device


obj-m := $(DRIVER_NAME).o
$(DRIVER_NAME)-y += lcd-device_truly240240.o
$(DRIVER_NAME)-y += lcd-driver_truly240240.o
$(DRIVER_NAME)-y += lcd.o

modules:
	$(MAKE) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) -C $(KSRC) M=$(MODPATH) modules

.PHONE:clean
clean:
	$(MAKE) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) -C $(KSRC) M=$(MODPATH) clean
