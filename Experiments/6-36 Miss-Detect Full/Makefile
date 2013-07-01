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

FFmpeg.patch: $(WORKBENCH_BASE)/FFmpeg.patch FFmpeg_noout.patch
	cat $^ > $@

FFmpeg/.git/config: $(WORKBENCH_BASE)/FFmpeg/.git/config
	rm -rf FFmpeg
	git clone --local $(<D)

$(WORKBENCH_BASE)/FFmpeg/.git/config:
	$(MAKE) -C $(WORKBENCH_BASE) FFmpeg/.git/config

clean::
	rm -f *.log
