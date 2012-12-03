WORKBENCH_BASE ?= ..
include $(WORKBENCH_BASE)/Makeconf

.PHONY: all debug clean

all: $(patsubst %.c,%.o,$(wildcard *.c))

%.o: %.c Makefile $(WORKBENCH_BASE)/Makeconf $(wildcard $(WORKBENCH_BASE)/Makeconf.local)
	$(CC) $(CPPFLAGS) -MM $< > .$*.d 2> /dev/null
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ "$(realpath $<)"

debug:
	$(MAKE) DEBUG=true

clean:
	rm -f *.o .*.d

-include $(wildcard .*.d)
