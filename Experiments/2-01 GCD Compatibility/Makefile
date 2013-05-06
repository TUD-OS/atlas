all:: $(addsuffix .log,$(basename $(notdir $(wildcard Samples/*.*))))

WORKBENCH_BASE ?= ../..
include $(WORKBENCH_BASE)/Makefile

%.log: Samples/%.h264 ffplay Makefile
	dd of=/dev/null bs=1024x1024 count=100 < $<  # load first 100MiB into buffer cache
	./ffplay -threads 1 -noframedrop -nostats -autoexit $< > $@

clean::
	rm -f *.log
