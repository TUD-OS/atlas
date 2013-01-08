all:: $(addsuffix .h264_metrics,$(basename $(wildcard Samples/*.*)))

WORKBENCH_BASE ?= ../..
include $(WORKBENCH_BASE)/Makefile

%.h264_metrics: %.h264 h264_workbench
	./h264_workbench $<
