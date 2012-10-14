all:: $(addsuffix .p264,$(basename $(wildcard Samples/*.*)))

WORKBENCH_BASE ?= ../..
include $(WORKBENCH_BASE)/Makefile

%.p264: %.h264 h264_workbench
	./h264_workbench $<
