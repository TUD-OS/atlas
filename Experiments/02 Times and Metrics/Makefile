all:: $(addsuffix .log,$(basename $(notdir $(wildcard Samples/*.*))))

BUILD_FFMPEG = true
WORKBENCH_BASE ?= ../..
include $(WORKBENCH_BASE)/Makefile

%.log: Samples/%.h264 h264_workbench
	./h264_workbench $< > $@

FFmpeg.patch: $(WORKBENCH_BASE)/FFmpeg.patch
	sed 's/#define FFMPEG_TIME 0/#define FFMPEG_TIME 1/' < $< > $@

FFmpeg/.git/config: $(WORKBENCH_BASE)/FFmpeg/.git
	git clone --local $<

$(WORKBENCH_BASE)/FFmpeg/.git:
	$(MAKE) -C $(WORKBENCH_BASE) FFmpeg/.git/config

clean::
	rm -f *.log
