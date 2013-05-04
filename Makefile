WORKBENCH_BASE ?= .
include $(WORKBENCH_BASE)/Makeconf

COMPONENTS = $(patsubst %.c,%.o,$(wildcard Components/*.c))
FFMPEG_LIBS = \
	FFmpeg/libavdevice/libavdevice.a \
	FFmpeg/libavfilter/libavfilter.a \
	FFmpeg/libavformat/libavformat.a \
	FFmpeg/libavcodec/libavcodec.a \
	FFmpeg/libswresample/libswresample.a \
	FFmpeg/libswscale/libswscale.a \
	FFmpeg/libavutil/libavutil.a

.PHONY: all debug clean cleanall force .Components .FFmpeg


BUILD_WORKBENCH ?= $(wildcard h264_workbench.c)
ifneq ($(BUILD_WORKBENCH),)
h264_workbench: %: %.c $(COMPONENTS) $(FFMPEG_LIBS) Makefile $(WORKBENCH_BASE)/Makefile $(WORKBENCH_BASE)/Makeconf $(wildcard $(WORKBENCH_BASE)/Makeconf.local)
	$(CC) $(CPPFLAGS) -MM $< > .$*.d 2> /dev/null
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ "$(realpath $<)" $(COMPONENTS) $(FFMPEG_LIBS) -lz -lm -pthread $(LDFLAGS)
all:: h264_workbench
clean::
	rm -f h264_workbench
endif

BUILD_FFMPEG ?= $(filter .,$(WORKBENCH_BASE))$(wildcard FFmpeg)
ifneq ($(BUILD_FFMPEG),)
ffplay: %: FFmpeg/%.c FFmpeg/cmdutils.o $(COMPONENTS) $(FFMPEG_LIBS) Makefile $(WORKBENCH_BASE)/Makefile $(WORKBENCH_BASE)/Makeconf $(wildcard $(WORKBENCH_BASE)/Makeconf.local)
	$(CC) $(CPPFLAGS) -MM $< > .$*.d 2> /dev/null
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ "$(realpath $<)" FFmpeg/cmdutils.o $(COMPONENTS) $(FFMPEG_LIBS) $(shell sdl-config --libs) $(LDFLAGS)
clean::
	rm -f ffplay
FFmpeg/ffplay.c FFmpeg/cmdutils.o $(FFMPEG_LIBS): .FFmpeg
	
FFmpeg $(wildcard FFmpeg/) .FFmpeg: FFmpeg/config.mak force
	$(MAKE) -j$(CPUS) -C FFmpeg
FFmpeg/config.mak: FFmpeg/configure
	cd $(@D) && CPPFLAGS= CFLAGS= ./configure \
		--cc=$(CC) --cpu=$(ARCH) --enable-pthreads \
		--disable-doc --disable-ffplay --disable-ffprobe --disable-ffserver \
		--disable-decoders --disable-encoders --disable-parsers --disable-demuxers --disable-muxers \
		--disable-protocols --disable-filters --disable-bsfs --disable-indevs --disable-outdevs --disable-hwaccels \
		--enable-decoder=h264 --enable-parser=h264 --enable-demuxer=h264 --enable-protocol=file --enable-rdft
FFmpeg/configure: FFmpeg/.git/config FFmpeg.patch $(WORKBENCH_BASE)/Makefile
	cd $(@D) && git diff --name-status --exit-code
	cd $(@D) && git reset --hard 39fe8033bbf94cac7935d749849fdf67ba8fc16a  # n0.11.1
	cd $(@D) && git clean -dfx
	patch -d $(@D) -p1 < FFmpeg.patch
	cd $(@D) && git add --all
	touch $@
FFmpeg/.git/config:
	git clone -n git://source.ffmpeg.org/ffmpeg.git FFmpeg
endif


BUILD_COMPONENTS ?= $(wildcard Components)
ifneq ($(BUILD_COMPONENTS),)
$(COMPONENTS): .Components
	
Components $(wildcard Components/) .Components: $(if $(BUILD_FFMPEG),.FFmpeg,) force
	$(MAKE) -j$(CPUS) -C Components
clean::
	$(MAKE) -C Components $@
endif


BUILD_X264 ?= $(filter .,$(WORKBENCH_BASE))$(wildcard x264)
ifneq ($(BUILD_X264),)
Samples Samples/:: x264
x264 $(wildcard x264/): x264/config.mak force
	$(MAKE) -j$(CPUS) -C $@
x264/config.mak: x264/configure
	cd $(@D) && CC=$(CC) CPPFLAGS= CFLAGS= ./configure --extra-cflags=-march=$(ARCH)
x264/configure: x264/.git/config $(WORKBENCH_BASE)/Makefile
	cd $(@D) && git diff --name-status --exit-code
	cd $(@D) && git reset --hard 37be55213a39db40cf159ada319bd482a1b00680
	cd $(@D) && git clean -dfx
	touch $@
x264/.git/config:
	git clone -n git://git.videolan.org/x264.git x264
endif


BUILD_LINUX ?= $(filter .,$(WORKBENCH_BASE))$(wildcard Linux)
ifneq ($(BUILD_LINUX),)
all:: Linux
Linux $(wildcard Linux/): Linux/.config force
	$(MAKE) -j$(CPUS) -C $@ KERNELVERSION=3.5.0-19-atlas bzImage
Linux/.config: Linux/debian
	cd $(@D) && unset MAKELEVEL && \
		fakeroot debian/rules clean && \
		fakeroot debian/rules binary-indep && \
		fakeroot debian/rules binary-perarch && \
		fakeroot debian/rules binary-atlas
	touch $(@D)/.scmversion  # prevent kernel build from using git for version string
	cp $(@D)/debian/build/build-atlas/.config $(@D)
	cd $(@D) && git clean -df
	touch $@
	@echo '*** Install the kernel packages and come back here afterwards. ***'
	@false
Linux/debian: Linux/.git/config Linux.patch $(WORKBENCH_BASE)/Makefile
	cd $(@D) && git diff --name-status --exit-code
	cd $(@D) && git reset --hard 7340a183c5702144b5c62cc78b78791f2783695c  # Ubuntu-lts-3.5.0-19.30
	cd $(@D) && git clean -dfx
	patch -d $(@D) -p1 < Linux.patch
	cd $(@D) && \
		git add --all debian.quantal && \
		git commit --message='build infrastructure' && \
		git add --all
	touch $@
Linux/.git/config:
	git clone -n git://kernel.ubuntu.com/ubuntu/ubuntu-precise.git Linux
endif


BUILD_SAMPLES ?= $(wildcard Samples)
ifneq ($(BUILD_SAMPLES),)
all:: Samples
.PRECIOUS: Samples/%.cfg Samples/%.h264 Samples/%.h264_metrics
Samples/%.h264: Samples/%.cfg
Samples/%.h264 Samples/%.cfg: force
	$(MAKE) -C $(@D) $(@F)
Samples Samples/:: force
	$(MAKE) -C $@
endif


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
	$(MAKE) -C Samples cleanall
	$(MAKE) -C Experiments clean
	-$(MAKE) -C FFmpeg distclean
	-$(MAKE) -C x264 clean
	-$(MAKE) -C Linux distclean

force:
	

-include $(wildcard .*.d)
