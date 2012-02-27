WORKBENCH_BASE ?= ../..
include $(WORKBENCH_BASE)/Makefile

all:: $(addsuffix .log,$(basename $(notdir $(wildcard $(WORKBENCH_BASE)/Samples/*.h264))))

%.log: $(WORKBENCH_BASE)/Samples/%.h264 h264_workbench
	./h264_workbench $< > $@

clean::
	rm -f *.log
