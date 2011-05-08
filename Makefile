WORKBENCH_BASE ?= .
include $(WORKBENCH_BASE)/Makeconf

PROCESSING_OBJS = $(patsubst %.c,%.o,$(wildcard Processing/*.c))
FFMPEG_LIBS = \
	$(WORKBENCH_BASE)/FFmpeg/libavformat/libavformat.a \
	$(WORKBENCH_BASE)/FFmpeg/libavcodec/libavcodec.a \
	$(WORKBENCH_BASE)/FFmpeg/libavcore/libavcore.a \
	$(WORKBENCH_BASE)/FFmpeg/libavutil/libavutil.a

.PHONY: all debug clean cleanall update force


BUILD_WORKBENCH ?= $(wildcard h264_workbench.c)
ifneq ($(BUILD_WORKBENCH),)
h264_workbench: h264_workbench.c $(FFMPEG_LIBS) $(PROCESSING_OBJS) Makefile
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MF .$@.d -o $@ $< $(FFMPEG_LIBS) $(PROCESSING_OBJS) -lz -lm -pthread
all:: h264_workbench
clean::
	rm -f h264_workbench
endif

BUILD_PROCESSING ?= $(wildcard Processing)
ifneq ($(BUILD_PROCESSING),)
$(PROCESSING_OBJS): Processing
	
Processing Processing/: force
	$(MAKE) -j$(CPUS) -C Processing
clean::
	$(MAKE) -C Processing $@
endif


BUILD_FFMPEG ?= $(filter .,$(WORKBENCH_BASE))$(wildcard FFmpeg)
ifneq ($(BUILD_FFMPEG),)
$(FFMPEG_LIBS): FFmpeg
	
FFmpeg FFmpeg/: FFmpeg/config.mak force
	$(MAKE) -j$(CPUS) -C FFmpeg
FFmpeg/config.mak: FFmpeg/configure
	cd FFmpeg && CC=$(CC) CPPFLAGS= CFLAGS= ./configure \
		--cpu=$(ARCH) --enable-pthreads --disable-doc --disable-swscale --disable-ffplay --disable-ffprobe --disable-ffserver \
		--disable-decoders --disable-encoders --disable-parsers --disable-demuxers --disable-muxers \
		--disable-protocols --disable-filters --disable-bsfs --disable-indevs --disable-outdevs \
		--enable-decoder=h264 --enable-parser=h264 --enable-demuxer=h264 --enable-protocol=file --enable-filter=buffer
FFmpeg/configure:
	rm -rf FFmpeg
	curl 'http://git.videolan.org/?p=ffmpeg.git;a=snapshot;h=HEAD;sf=tgz' | tar xz
	mv ffmpeg* FFmpeg
	patch -d FFmpeg -p0 < FFmpeg.patch
endif


BUILD_X264 ?= $(filter .,$(WORKBENCH_BASE))$(wildcard x264)
ifneq ($(BUILD_X264),)
Samples Samples/:: x264
x264 x264/: x264/config.mak force
	$(MAKE) -j$(CPUS) -C $@
x264/config.mak: x264/configure
	cd x264 && CC=$(CC) CPPFLAGS= CFLAGS= ./configure --extra-cflags=-march=$(ARCH)
x264/configure:
	rm -rf x264
	curl 'http://git.videolan.org/?p=x264.git;a=snapshot;h=HEAD;sf=tgz' | tar xz
	mv x264* x264
endif


BUILD_SAMPLES ?= $(wildcard Samples)
ifneq ($(BUILD_SAMPLES),)
all:: Samples
Samples/%.h264: Samples
Samples Samples/:: force
	$(MAKE) -C $@
endif

%.h264 %.mov:
	$(MAKE) -C $(@D) $(@F)


BUILD_EXPERIMENTS ?= $(wildcard Experiments)
ifneq ($(BUILD_EXPERIMENTS),)
all:: Experiments
Experiments Experiments/: force
	$(MAKE) -C $@
endif


debug:
	$(MAKE) DEBUG=true

clean::
	rm -f .*.d
	rm -rf *.dSYM

cleanall: clean
	$(MAKE) -C Samples clean
	$(MAKE) -C Experiments clean
	$(MAKE) -C FFmpeg distclean
	$(MAKE) -C x264 clean

update: cleanall
	rm -f FFmpeg/configure x264/configure
	$(MAKE) all

force:
	

-include $(wildcard *.d)
