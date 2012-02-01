WORKBENCH_BASE ?= .
include $(WORKBENCH_BASE)/Makeconf

COMPONENTS = $(patsubst %.c,%.o,$(wildcard Components/*.c))
FFMPEG_LIBS = \
	$(WORKBENCH_BASE)/FFmpeg/libavutil/libavutil.a \
	$(WORKBENCH_BASE)/FFmpeg/libavdevice/libavdevice.a \
	$(WORKBENCH_BASE)/FFmpeg/libavfilter/libavfilter.a \
	$(WORKBENCH_BASE)/FFmpeg/libavformat/libavformat.a \
	$(WORKBENCH_BASE)/FFmpeg/libavcodec/libavcodec.a \
	$(WORKBENCH_BASE)/FFmpeg/libswresample/libswresample.a \
	$(WORKBENCH_BASE)/FFmpeg/libswscale/libswscale.a

.PHONY: all debug clean cleanall update force


BUILD_WORKBENCH ?= $(wildcard h264_workbench.c)
ifneq ($(BUILD_WORKBENCH),)
h264_workbench: h264_workbench.c $(FFMPEG_LIBS) $(COMPONENTS) Makefile
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MF .$@.d -o $@ $(realpath $<) $(FFMPEG_LIBS) $(COMPONENTS) -lz -lm -pthread
all:: h264_workbench
clean::
	rm -f h264_workbench
endif

BUILD_COMPONENTS ?= $(wildcard Components)
ifneq ($(BUILD_COMPONENTS),)
$(COMPONENTS): Components
	
Components Components/: force
	$(MAKE) -j$(CPUS) -C $@
clean::
	$(MAKE) -C Components $@
endif


BUILD_FFMPEG ?= $(filter .,$(WORKBENCH_BASE))$(wildcard FFmpeg)
ifneq ($(BUILD_FFMPEG),)
$(FFMPEG_LIBS): FFmpeg
	
FFmpeg FFmpeg/: FFmpeg/config.mak force
	$(MAKE) -j$(CPUS) -C $@
FFmpeg/config.mak: FFmpeg/configure
	cd FFmpeg && CPPFLAGS= CFLAGS= ./configure \
		--cc=$(CC) --cpu=$(ARCH) --enable-pthreads \
		--disable-doc --disable-ffplay --disable-ffprobe --disable-ffserver \
		--disable-decoders --disable-encoders --disable-parsers --disable-demuxers --disable-muxers \
		--disable-protocols --disable-filters --disable-bsfs --disable-indevs --disable-outdevs --disable-hwaccels \
		--enable-decoder=h264 --enable-parser=h264 --enable-demuxer=h264 --enable-protocol=file --enable-rdft
FFmpeg/configure:
	rm -rf FFmpeg
	curl 'http://git.videolan.org/?p=ffmpeg.git;a=snapshot;h=7e16636995fd6710164f7622cd77abc94c27a064;sf=tgz' | tar xz
	mv ffmpeg* FFmpeg
	patch -d FFmpeg -p0 < FFmpeg.patch
endif


BUILD_X264 ?= $(filter .,$(WORKBENCH_BASE))$(wildcard x264)
ifneq ($(BUILD_X264),)
Samples Samples/:: x264
x264 x264/: x264/config.mak force
	$(MAKE) -j$(CPUS) -C $@
x264/config.mak: x264/configure
	# FIXME: clang-compiled x264 crashes when encoding the demo BBC video
	cd x264 && CC=gcc CPPFLAGS= CFLAGS= ./configure --extra-cflags=-march=$(ARCH)
x264/configure:
	rm -rf x264
	curl 'http://git.videolan.org/?p=x264.git;a=snapshot;h=f33c8cb0f8fff7a83100b3e9d15baba53c6f6a35;sf=tgz' | tar xz
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
	-$(MAKE) -C FFmpeg distclean
	-$(MAKE) -C x264 clean

update: cleanall
	rm -f FFmpeg/configure x264/configure
	$(MAKE) all

force:
	

-include $(wildcard *.d)
