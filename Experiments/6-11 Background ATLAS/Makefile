all:: hungergames-tlr1_h1080p.log

WORKBENCH_BASE ?= ../..
include $(WORKBENCH_BASE)/Makefile

%.log: SHELL = /bin/bash
%.log: Samples/%.h264_metrics cpuhog ffplay Makefile
	dd of=/dev/null bs=1024x1024 count=100 < $<  # load first 100MiB into buffer cache
	{ sleep 60 ; for ((i=0;i<10;i++)) ; do taskset 1 ./cpuhog & done ; sleep 60 ; killall cpuhog ; } &
	./ffplay -x 960 -y 408 -f h264 -threads 1 -nostats -noframedrop -autoexit $< > $@

cpuhog: SHELL = /bin/bash
cpuhog: Makefile
	$(CC) -x c -o $@ - <<< 'int main(void) { while (1); }'

%.h264_metrics: force
	$(MAKE) -C ../4-03\ Preprocess\ Metrics $@

clean::
	rm -f cpuhog *.log
