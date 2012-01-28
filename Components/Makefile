WORKBENCH_BASE ?= ..
include $(WORKBENCH_BASE)/Makeconf

.PHONY: all debug clean

all: $(patsubst %.c,%.o,$(wildcard *.c))

%.o: %.c Makefile
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MF .$*.d -c -o $@ $(realpath $<)

debug:
	$(MAKE) DEBUG=true

clean:
	rm -f *.o .*.d

-include .*.d
