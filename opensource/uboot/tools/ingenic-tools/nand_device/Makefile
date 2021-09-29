
SRCS := $(shell find . -name "*.c")
#OBJS := $(patsubst %.c,%.o,${AIENGINE_SRC})
CFLAGS:= $(HOSTCFLAGS) -std=c99


sfc_nand_params: $(SRCS)
	./deal_nand.sh
	$(HOSTCC) $(CFLAGS) $(HOSTLDFLAGS) -o $@ $^
	$(HOSTSTRIP) $@
	mv $@ ../

