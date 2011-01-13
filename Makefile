WORKBENCH_BASE ?= .
include $(WORKBENCH_BASE)/Makeconf

PROCESSING_OBJS = $(patsubst %.c,%.o,$(wildcard Processing/*.c))
FFMPEG_LIBS = \
	$(WORKBENCH_BASE)/FFmpeg/libavformat/libavformat.a \
	$(WORKBENCH_BASE)/FFmpeg/libavcodec/libavcodec.a \
	$(WORKBENCH_BASE)/FFmpeg/libavcore/libavcore.a \
	$(WORKBENCH_BASE)/FFmpeg/libavutil/libavutil.a

.PHONY: all debug clean cleanall update dist force


ifeq ($(wildcard h264_workbench.c),h264_workbench.c)
h264_workbench: h264_workbench.c $(FFMPEG_LIBS) $(PROCESSING_OBJS) Makefile
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MF .$@.d -o $@ $< $(FFMPEG_LIBS) $(PROCESSING_OBJS) -lz -lm -pthread
all:: h264_workbench
endif

ifeq ($(wildcard Processing),Processing)
$(PROCESSING_OBJS): Processing
Processing Processing/: force
	$(MAKE) -j$(CPUS) -C Processing
endif


ifeq ($(wildcard FFmpeg),FFmpeg)
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
	cd FFmpeg && svn up -r BASE
endif


ifeq ($(wildcard x264),x264)
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


ifeq ($(wildcard Samples),Samples)
all:: Samples
Samples/%.h264: Samples
Samples Samples/:: force
	$(MAKE) -C $@
endif

%.h264 %.mov:
	$(MAKE) -C $(@D) $(@F)

ifeq ($(wildcard Experiments),Experiments)
all:: Experiments
Experiments Experiments/: force
	$(MAKE) -C $@
endif


debug:
	$(MAKE) DEBUG=true

clean::
	$(MAKE) -C Processing $@
	rm -f .*.d
	rm -rf *.dSYM
	rm -f h264_workbench

cleanall: clean
	$(MAKE) -C Samples clean
	$(MAKE) -C Experiments clean
	$(MAKE) -C FFmpeg distclean
	$(MAKE) -C x264 clean

update: cleanall
	rm -f FFmpeg/configure x264/configure
	$(MAKE) all

dist: Workbench.tar.gz
	scp Workbench.tar.gz os:Sites/
	rm Workbench.tar.gz

Workbench.tar.gz: force
	svn co --depth=empty http://os.inf.tu-dresden.de/~mroi/svn/video/trunk Workbench
	mkdir Workbench/FFmpeg Workbench/FFmpeg/libavcodec
	echo 'all:\n\trm Makefile\n\tsvn up -r BASE --set-depth infinity\n\t$$(MAKE)' > Workbench/Makefile
	cp FFmpeg/Makefile Workbench/FFmpeg
	cp FFmpeg/libavcodec/avcodec.h FFmpeg/libavcodec/h264.c FFmpeg/libavcodec/h264_loopfilter.c Workbench/FFmpeg/libavcodec
	cp -R FFmpeg/.svn Workbench/FFmpeg/
	cp -R FFmpeg/libavcodec/.svn Workbench/FFmpeg/libavcodec/
	tar -czf $@ Workbench/
	rm -rf Workbench

force:
	

-include $(wildcard *.d)
