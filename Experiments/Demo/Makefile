all:: after

BUILD_FFMPEG = true
WORKBENCH_BASE ?= ../..
include $(WORKBENCH_BASE)/Makefile

before: SHELL = /bin/bash
before: Samples/startrek-teaser2_h1080p.h264_metrics cpuhog starttime ffplay Makefile
	dd of=/dev/null bs=1024x1024 count=100 < $<  # load first 100MiB into buffer cache
	{ sleep 15 ; for ((i=0;i<10;i++)) ; do taskset 1 ./cpuhog & done } &
	export DEMO_START=`./starttime` ; echo $$DEMO_START ; \
	SDL_VIDEO_CENTERED=center ./ffplay -window_title 'Scheduled by CFS' -f h264 -x 960 -y 400 -threads 1 -nostats -noframedrop -autoexit $<
	killall cpuhog

after: SHELL = /bin/bash
after: Samples/startrek-teaser2_h1080p.h264_metrics cpuhog starttime ffplay Makefile
	dd of=/dev/null bs=1024x1024 count=100 < $<  # load first 100MiB into buffer cache
	{ sleep 15 ; for ((i=0;i<10;i++)) ; do taskset 1 ./cpuhog & done } &
	export DEMO_START=`./starttime` ; \
	SDL_VIDEO_WINDOW_POS=235,20 DEMO_ATLAS=1 ./ffplay -window_title 'Scheduled by ATLAS' -f h264 -x 960 -y 400 -threads 1 -nostats -noframedrop -autoexit $< & \
	SDL_VIDEO_WINDOW_POS=235,460 ./ffplay -window_title 'Scheduled by CFS' -f h264 -x 960 -y 400 -threads 1 -nostats -noframedrop -autoexit $<
	killall cpuhog

cpuhog: SHELL = /bin/bash
cpuhog: Makefile
	$(CC) -x c -o $@ - <<< 'int main(void) { while (1); }'

starttime: starttime.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $< $(LDFLAGS)

%.h264_metrics: %.h264
	$(MAKE) -C ../4-03\ Preprocess\ Metrics h264_workbench
	../4-03\ Preprocess\ Metrics/h264_workbench $<

FFmpeg.patch: $(WORKBENCH_BASE)/FFmpeg.patch FFmpeg-starttime.patch
	cat $^ > $@

FFmpeg/.git/config: $(WORKBENCH_BASE)/FFmpeg/.git/config
	rm -rf FFmpeg
	git clone --local $(<D)

$(WORKBENCH_BASE)/FFmpeg/.git/config:
	$(MAKE) -C $(WORKBENCH_BASE) FFmpeg/.git/config

clean::
	rm -f cpuhog starttime
