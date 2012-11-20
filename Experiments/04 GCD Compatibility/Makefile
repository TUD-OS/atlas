all:: $(addsuffix .log,$(basename $(notdir $(wildcard Samples/*.*))))

WORKBENCH_BASE ?= ../..
include $(WORKBENCH_BASE)/Makefile

%.log: Samples/%.h264 ffplay Makefile
	./ffplay -threads 1 -autoexit $< && echo OK > $@

clean::
	rm -f *.log
