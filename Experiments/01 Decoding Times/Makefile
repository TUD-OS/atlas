all:: $(addsuffix .log,$(basename $(notdir $(wildcard Samples/*.h264))))

WORKBENCH_BASE ?= ../..
include $(WORKBENCH_BASE)/Makefile

%.log: Samples/%.h264 h264_workbench
	./h264_workbench $< > $@

clean::
	rm -f *.log
