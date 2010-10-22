all:: Visualization.mp4

WORKBENCH_BASE ?= ../..
include $(WORKBENCH_BASE)/Makefile

Visualization.mp4: ../../Samples/bbc-r_m720p.h264 h264_workbench
	rm -f Visualization.yuv
	mkfifo Visualization.yuv
	./h264_workbench $< &
	ffmpeg -threads $(CPUS) -f rawvideo -s 1280x720 -r 6 -i Visualization.yuv -b 8M -y $@
	rm Visualization.yuv

clean::
	rm -f Visualization.yuv Visualization.mp4
