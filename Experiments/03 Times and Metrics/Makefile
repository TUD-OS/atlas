WORKBENCH_BASE ?= ../..

all:: $(addsuffix .log,$(basename $(notdir $(wildcard $(WORKBENCH_BASE)/Samples/*.*))))

include $(WORKBENCH_BASE)/Makefile

%.log: $(WORKBENCH_BASE)/Samples/%.h264 h264_workbench
	./h264_workbench $< > $@

clean::
	rm -f *.log
