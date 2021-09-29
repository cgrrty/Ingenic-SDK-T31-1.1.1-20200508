CONFIG_UCLIBC_BUILD=y

CROSS_COMPILE ?= mips-linux-gnu-

CC = $(CROSS_COMPILE)gcc
CPLUSPLUS = $(CROSS_COMPILE)g++
LD = $(CROSS_COMPILE)ld
AR = $(CROSS_COMPILE)ar cr
STRIP = $(CROSS_COMPILE)strip

CFLAGS = $(INCLUDES) -O2 -Wall -march=mips32r2

ifeq ($(CONFIG_UCLIBC_BUILD), y)
CFLAGS += -muclibc
LDFLAG += -muclibc
endif

ifeq ($(CONFIG_UCLIBC_BUILD), y)
SDK_LIB_DIR = ../../lib/uclibc
else
SDK_LIB_DIR = ../../lib/glibc
endif

SDK_INC_DIR = ../../include

INCLUDES = -I$(SDK_INC_DIR)

LIBS =	$(SDK_LIB_DIR)/libsysutils.a $(SDK_LIB_DIR)/libalog.a

LDFLAG += -Wl,-gc-sections

SAMPLES = sample-keyevent \
	sample-alarm \
	sample-led \
	sample-pir \
	sample-cipher \
	sample-chipid \
	sample-adc \
	sample-wdt

all: 	$(SAMPLES)

sample-keyevent: $(SDK_LIB_DIR)/libsysutils.a $(SDK_LIB_DIR)/libalog.a sample-keyevent.o
	$(CC)  $(LDFLAG) -o $@ $^ $(LIBS) -lpthread -lm -lrt
	$(STRIP) $@

sample-alarm: $(SDK_LIB_DIR)/libsysutils.a $(SDK_LIB_DIR)/libalog.a sample-alarm.o
	$(CC)  $(LDFLAG) -o $@ $^ $(LIBS) -lpthread -lm -lrt

sample-led: $(SDK_LIB_DIR)/libsysutils.a $(SDK_LIB_DIR)/libalog.a sample-led.o
	$(CC) $(LDFLAG) -o $@ $^ $(LIBS) -lpthread -lm -lrt

sample-pir: $(SDK_LIB_DIR)/libsysutils.a $(SDK_LIB_DIR)/libalog.a sample-pir.o
	$(CC) $(LDFLAG) -o $@ $^ $(LIBS) -lpthread -lm -lrt

sample-cipher: $(SDK_LIB_DIR)/libsysutils.a $(SDK_LIB_DIR)/libalog.a sample-cipher.o
	$(CC) $(LDFLAG) -o $@ $^ $(LIBS) -lpthread -lm -lrt

sample-chipid: $(SDK_LIB_DIR)/libsysutils.a $(SDK_LIB_DIR)/libalog.a sample-chipid.o
	$(CC) $(LDFLAG) -o $@ $^ $(LIBS) -lpthread -lm -lrt

sample-adc: $(SDK_LIB_DIR)/libsysutils.a $(SDK_LIB_DIR)/libalog.a sample-adc.o
	$(CC) $(LDFLAG) -o $@ $^ $(LIBS) -lpthread -lm -lrt

sample-wdt: $(SDK_LIB_DIR)/libsysutils.a $(SDK_LIB_DIR)/libalog.a sample-wdt.o
	$(CC) $(LDFLAG) -o $@ $^ $(LIBS) -lpthread -lm -lrt

%.o:%.c
	$(CC) -c $(CFLAGS) $< -o $@

clean:
	rm -f *.o *~

distclean: clean
	rm -f $(SAMPLES)
