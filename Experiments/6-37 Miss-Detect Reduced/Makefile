include Makeconf.experiment

all:: $(addsuffix .log,$(VIDEOS))

BUILD_FFMPEG = true
WORKBENCH_BASE ?= ../..
include $(WORKBENCH_BASE)/Makefile

ffplay: Makeconf.experiment

%.log: Samples/%.h264_metrics ffplay Makefile
	./ffplay -vf scale=iw/4:-1 -threads 1 -noframedrop -nostats -autoexit $< > $@

%.h264_metrics: force
	$(MAKE) -C ../4-03\ Preprocess\ Metrics $@

FFmpeg.patch FFmpeg/.git/config: force
	$(MAKE) -C ../6-36\ Miss-Detect\ Full $@

clean::
	rm -f *.log
