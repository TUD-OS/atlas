all:: $(foreach video,blackswan-tlr1c_h480p FVDO_Shore_4cif 720p50_parkrun_ter,$(video)-ffplay.log $(video)-worker.log)

WORKBENCH_BASE ?= ../..
include $(WORKBENCH_BASE)/Makefile

%-ffplay.log %-worker.log: SHELL = /bin/bash
%-ffplay.log %-worker.log: Samples/%.h264_metrics ffplay worker cpuhog
	dd of=/dev/null bs=1024x1024 count=100 < $<  # load first 100MiB into buffer cache
	{ sleep 2 ; taskset 1 ./cpuhog ; } &
	LC_ALL=C sudo chrt 10 su $$USER -c ./worker > $*-worker.log &
	./ffplay -f h264 -threads 1 -nostats -noframedrop -autoexit $< > $*-ffplay.log
	killall worker cpuhog

ffplay: force
	$(MAKE) -C ../6-15\ Scheduling\ CFS $@

worker: force
	$(MAKE) -C ../6-09\ UI-Worker\ CFS\ Load $@

cpuhog: SHELL = /bin/bash
cpuhog: Makefile
	$(CC) -x c -o $@ - <<< 'int main(void) { while (1); }'

%.h264_metrics: force
	$(MAKE) -C ../4-03\ Preprocess\ Metrics $@

clean::
	rm -f cpuhog *.log
