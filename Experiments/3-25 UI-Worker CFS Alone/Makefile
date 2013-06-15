all:: worker.log

WORKBENCH_BASE ?= ../..
include $(WORKBENCH_BASE)/Makefile

worker.log: worker Makefile
	LC_ALL=C ./worker > $@ & sleep 300 ; killall worker

worker: %: %.c $(COMPONENTS) Makefile
	$(CC) $(CPPFLAGS) $(shell pkg-config --cflags gtk+-3.0) $(CFLAGS) -o $@ "$(realpath $<)" $(COMPONENTS) $(shell pkg-config --libs gtk+-3.0) $(LDFLAGS)

clean::
	rm -f *.log
