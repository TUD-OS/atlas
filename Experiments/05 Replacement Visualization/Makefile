WORKBENCH_BASE ?= ../..

all:: $(addsuffix .mp4,$(basename $(notdir $(wildcard $(WORKBENCH_BASE)/Samples/*.p264))))

include $(WORKBENCH_BASE)/Makefile

%.mp4: $(WORKBENCH_BASE)/Samples/%.p264 h264_workbench
	rm -f Visualization.yuv
	mkfifo Visualization.yuv
	./h264_workbench $< > /dev/null &
	ffmpeg -threads $(CPUS) -f rawvideo -s 2560x1440 -r 12 -i Visualization.yuv -b 8M -s 1280x720 -y $@
	rm Visualization.yuv

clean::
	rm -f Visualization.yuv *.mp4
