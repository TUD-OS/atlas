all:: $(addsuffix .log,$(basename $(notdir $(wildcard Samples/*.*))))

CPPFLAGS = -DMETRICS_FULL

WORKBENCH_BASE ?= ../..
include $(WORKBENCH_BASE)/Makefile

%.log: Samples/%.h264_metrics ffplay Makefile
	./ffplay -f h264 -threads 1 -nostats -noframedrop -autoexit $< > $@

%.h264_metrics: force
	$(MAKE) -C ../03\ Preprocess\ Metrics $@

clean::
	rm -f *.log
