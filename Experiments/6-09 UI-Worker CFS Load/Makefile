all:: worker.log

WORKBENCH_BASE ?= ../..
include $(WORKBENCH_BASE)/Makefile

worker.log: SHELL = /bin/bash
worker.log: worker cpuhog Makefile
	{ sleep 5 ; for ((i=0;i<10;i++)) ; do taskset 1 ./cpuhog & done } &
	LC_ALL=C ./worker > $@ & sleep 300 ; killall worker cpuhog

worker: %: %.c $(COMPONENTS) Makefile
	$(CC) $(CPPFLAGS) $(shell pkg-config --cflags gtk+-3.0) $(CFLAGS) -o $@ "$(realpath $<)" $(COMPONENTS) $(shell pkg-config --libs gtk+-3.0) $(LDFLAGS)

cpuhog: SHELL = /bin/bash
cpuhog: Makefile
	$(CC) -x c -o $@ - <<< 'int main(void) { while (1); }'

clean::
	rm -f *.log
