WORKBENCH_BASE ?= ../..

TRAINING_VIDEOS = $(WORKBENCH_BASE)/Samples/bbc-r_m720p.p264

all:: ../Coefficients.dat

include $(WORKBENCH_BASE)/Makefile

../Coefficients.dat: $(TRAINING_VIDEOS) h264_workbench
	./h264_workbench $(TRAINING_VIDEOS)

clean::
	rm -f ../Coefficients.dat
