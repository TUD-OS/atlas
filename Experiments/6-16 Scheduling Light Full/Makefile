include Makeconf.experiment

all:: $(addsuffix .log,$(VIDEOS))

WORKBENCH_BASE ?= ../..
include $(WORKBENCH_BASE)/Makefile

ffplay: Makeconf.experiment

%.log: SHELL = /bin/bash
%.log: Samples/%.h264_metrics cpuhog ffplay Makefile
	dd of=/dev/null bs=1024x1024 count=100 < $<  # load first 100MiB into buffer cache
	{ sleep 1 ; for ((i=0;i<5;i++)) ; do taskset 1 ./cpuhog & done } &
	./ffplay -f h264 -threads 1 -nostats -noframedrop -autoexit $< > $@
	killall cpuhog

cpuhog: SHELL = /bin/bash
cpuhog: Makefile
	$(CC) -x c -o $@ - <<< 'int main(void) { while (1); }'

%.h264_metrics: force
	$(MAKE) -C ../4-03\ Preprocess\ Metrics $@

clean::
	rm -f cpuhog *.log
