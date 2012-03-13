WORKBENCH_BASE ?= .
include $(WORKBENCH_BASE)/Makeconf

COMPONENTS = $(patsubst %.c,%.o,$(wildcard Components/*.c))
FFMPEG_LIBS = \
	$(WORKBENCH_BASE)/FFmpeg/libavdevice/libavdevice.a \
	$(WORKBENCH_BASE)/FFmpeg/libavfilter/libavfilter.a \
	$(WORKBENCH_BASE)/FFmpeg/libavformat/libavformat.a \
	$(WORKBENCH_BASE)/FFmpeg/libavcodec/libavcodec.a \
	$(WORKBENCH_BASE)/FFmpeg/libswresample/libswresample.a \
	$(WORKBENCH_BASE)/FFmpeg/libswscale/libswscale.a \
	$(WORKBENCH_BASE)/FFmpeg/libavutil/libavutil.a
SDL_LIBS = \
	$(WORKBENCH_BASE)/SDL/build/.libs/libSDL.a \
	$(WORKBENCH_BASE)/SDL/build/.libs/libSDLmain.a

.PHONY: all debug clean cleanall update force


BUILD_WORKBENCH ?= $(wildcard h264_workbench.c)
ifneq ($(BUILD_WORKBENCH),)
h264_workbench: h264_workbench.c $(FFMPEG_LIBS) $(COMPONENTS) Makefile
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MF .$@.d -o $@ "$(realpath $<)" $(FFMPEG_LIBS) $(COMPONENTS) -lz -lm -pthread $(LDFLAGS)
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
ffplay: FFmpeg/ffplay.c FFmpeg/cmdutils.c $(FFMPEG_LIBS) $(SDL_LIBS) $(COMPONENTS)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MF .$@.d -o $@ "$(realpath $<)" FFmpeg/cmdutils.c $(FFMPEG_LIBS) $(SDL_LIBS) $(COMPONENTS) $(SDL_EXTRA_LIBS) $(LDFLAGS)
clean::
	rm -f ffplay
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
	curl 'http://git.videolan.org/?p=ffmpeg.git;a=snapshot;h=$(if $(UPDATE),HEAD,7e16636995fd6710164f7622cd77abc94c27a064);sf=tgz' | tar xz
	mv ffmpeg* FFmpeg
	patch -b -d FFmpeg -p0 < FFmpeg.patch
endif


BUILD_SDL ?= $(filter .,$(WORKBENCH_BASE))$(wildcard SDL)
ifneq ($(BUILD_SDL),)
$(SDL_LIBS): SDL
	
SDL SDL/: SDL/config.status force
	$(MAKE) -j$(CPUS) -C $@
SDL/config.status: SDL/configure
	cd SDL && CC=$(CC) CPPFLAGS= CFLAGS= ./configure --disable-assembly
SDL/configure:
	rm -rf SDL
	curl http://www.libsdl.org/release/SDL-1.2.15.tar.gz | tar xz
	mv SDL* SDL
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
	curl 'http://git.videolan.org/?p=x264.git;a=snapshot;h=$(if $(UPDATE),HEAD,02c3d5ec58d6bcbc5e22715ae80d53d8556f3c8f);sf=tgz' | tar xz
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
	-$(MAKE) -C SDL distclean

update: cleanall
	rm -f FFmpeg/configure SDL/configure x264/configure
	$(MAKE) all UPDATE=true

force:
	

-include $(wildcard *.d)
