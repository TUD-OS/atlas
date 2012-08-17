WORKBENCH_BASE ?= ..
include $(WORKBENCH_BASE)/Makeconf

.PHONY: all debug clean

all: $(patsubst %.c,%.o,$(wildcard *.c))

%.o: %.c .%.d Makefile $(WORKBENCH_BASE)/Makeconf
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ "$(realpath $<)"

debug:
	$(MAKE) DEBUG=true

clean:
	rm -f *.o .*.d

include $(wildcard .*.d)

.%.d: %.c
	$(CC) $(CPPFLAGS) -MM $< > $@
