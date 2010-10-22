all:: Preprocess_Times.log

WORKBENCH_BASE ?= ../..
include $(WORKBENCH_BASE)/Makefile

Preprocess_Times.log: $(addsuffix .p264,$(basename $(wildcard $(WORKBENCH_BASE)/Samples/*.*)))

%.p264: %.h264 h264_workbench
	echo $@ >> Preprocess_Times.log
	{ time ./h264_workbench $< ; } >> Preprocess_Times.log 2>&1
	echo >> Preprocess_Times.log

clean::
	rm -f Preprocess_Times.log
