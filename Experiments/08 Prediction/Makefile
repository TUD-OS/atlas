WORKBENCH_BASE ?= ../..

all:: $(addsuffix .log,$(basename $(notdir $(wildcard $(WORKBENCH_BASE)/Samples/*.p264))))

include $(WORKBENCH_BASE)/Makefile

%.log: $(WORKBENCH_BASE)/Samples/%.p264 Coefficients.dat h264_workbench
	./h264_workbench $< > $@

clean::
	rm -f *.log
