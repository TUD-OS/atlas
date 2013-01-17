all:: $(addsuffix .log,$(basename $(notdir $(wildcard Samples/*.*))))

include Makeconf.metrics
WORKBENCH_BASE ?= ../..
include $(WORKBENCH_BASE)/Makefile

%.log cpuhog: SHELL = /bin/bash

%.log: Samples/%.h264_metrics cpuhog ffplay Makefile
	dd of=/dev/null bs=1M count=100 < $<  # load first 100MiB into buffer cache
	{ sleep 1 ; for ((i=0;i<10;i++)) ; do taskset 1 ./cpuhog & done } &
	./ffplay -f h264 -x 960 -y 540 -threads 1 -nostats -noframedrop -autoexit $< > $@
	killall cpuhog

cpuhog: Makefile
	$(CC) -x c -o $@ - <<< 'int main(void) { while (1); }'

%.h264_metrics: force
	$(MAKE) -C ../01\ Preprocess $@

clean::
	rm -f cpuhog *.log
