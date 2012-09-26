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
SDL_LIBS = \
	SDL/build/.libs/libSDL.a \
	SDL/build/.libs/libSDLmain.a

.PHONY: all debug clean cleanall force


BUILD_WORKBENCH ?= $(wildcard h264_workbench.c)
ifneq ($(BUILD_WORKBENCH),)
h264_workbench: %: %.c $(FFMPEG_LIBS) $(COMPONENTS) Makefile $(WORKBENCH_BASE)/Makefile $(WORKBENCH_BASE)/Makeconf $(WORKBENCH_BASE)/Makeconf.local
	$(CC) $(CPPFLAGS) -MM $< > .$*.d
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ "$(realpath $<)" $(FFMPEG_LIBS) $(COMPONENTS) -lz -lm -pthread $(LDFLAGS)
all:: h264_workbench
clean::
	rm -f h264_workbench
endif

BUILD_COMPONENTS ?= $(wildcard Components)
ifneq ($(BUILD_COMPONENTS),)
$(COMPONENTS): Components
	
Components $(wildcard Components/): force
	$(MAKE) -j$(CPUS) -C $@
clean::
	$(MAKE) -C Components $@
endif


BUILD_FFMPEG ?= $(filter .,$(WORKBENCH_BASE))$(wildcard FFmpeg)
ifneq ($(BUILD_FFMPEG),)
ffplay: %: FFmpeg/%.c FFmpeg/cmdutils.c $(FFMPEG_LIBS) $(SDL_LIBS) $(COMPONENTS) Makefile $(WORKBENCH_BASE)/Makefile $(WORKBENCH_BASE)/Makeconf $(WORKBENCH_BASE)/Makeconf.local
	$(CC) $(CPPFLAGS) -MM $< > .$*.d
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ "$(realpath $<)" FFmpeg/cmdutils.c $(FFMPEG_LIBS) $(SDL_LIBS) $(COMPONENTS) $(SDL_EXTRA_LIBS) $(LDFLAGS)
clean::
	rm -f ffplay
FFmpeg/ffplay.c FFmpeg/cmdutils.c $(FFMPEG_LIBS): FFmpeg
	
FFmpeg $(wildcard FFmpeg/): FFmpeg/config.mak force
	$(MAKE) -j$(CPUS) -C $@
FFmpeg/config.mak: FFmpeg/configure
	cd $(@D) && CPPFLAGS= CFLAGS= ./configure \
		--cc=$(CC) --cpu=$(ARCH) --enable-pthreads \
		--disable-doc --disable-ffplay --disable-ffprobe --disable-ffserver \
		--disable-decoders --disable-encoders --disable-parsers --disable-demuxers --disable-muxers \
		--disable-protocols --disable-filters --disable-bsfs --disable-indevs --disable-outdevs --disable-hwaccels \
		--enable-decoder=h264 --enable-parser=h264 --enable-demuxer=h264 --enable-protocol=file --enable-rdft
FFmpeg/configure: FFmpeg/.git/config FFmpeg.patch $(WORKBENCH_BASE)/Makefile
	cd $(@D) && git diff --name-status --exit-code
	cd $(@D) && git checkout --force 39fe8033bbf94cac7935d749849fdf67ba8fc16a  # n0.11.1
	patch -d $(@D) -p1 < FFmpeg.patch
	cd $(@D) && git add --all
	touch $@
FFmpeg/.git/config:
	test -d FFmpeg && rm -r FFmpeg || true
	git clone -n git://git.videolan.org/ffmpeg.git FFmpeg
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
	cd $(@D) && git checkout --force 37be55213a39db40cf159ada319bd482a1b00680
	touch $@
x264/.git/config:
	test -d x264 && rm -r x264 || true
	git clone -n git://git.videolan.org/x264.git x264
endif


BUILD_SDL ?= $(filter .,$(WORKBENCH_BASE))$(wildcard SDL)
ifneq ($(BUILD_SDL),)
$(SDL_LIBS): SDL
	
SDL $(wildcard SDL/): SDL/config.status force
	$(MAKE) -j$(CPUS) -C $@
SDL/config.status: SDL/configure
	cd $(@D) && CC=$(CC) CPPFLAGS= CFLAGS= ./configure --disable-assembly
SDL/configure: $(WORKBENCH_BASE)/Makefile
	test -d SDL && rm -r SDL || true
	curl http://www.libsdl.org/release/SDL-1.2.15.tar.gz | tar xz
	mv SDL* SDL
	touch $@
endif


BUILD_LINUX ?= $(filter .,$(WORKBENCH_BASE))$(wildcard Linux)
ifneq ($(BUILD_LINUX),)
all:: Linux
Linux $(wildcard Linux/): Linux/.config force
	$(MAKE) -j$(CPUS) -C $@ KERNELVERSION=3.5.0-10-atlas bzImage
Linux/.config: Linux/debian
	cd $(@D) && unset MAKELEVEL && \
		fakeroot debian/rules clean && \
		fakeroot debian/rules binary-indep && \
		fakeroot debian/rules binary-perarch && \
		fakeroot debian/rules binary-atlas
	touch $(@D)/.scmversion  # prevent kernel build from using git for version string
	cp $(@D)/debian/build/build-atlas/.config $(@D)
	@echo *** Install the kernel packages and come back here afterwards. ***
	@false
Linux/debian: Linux/.git/config Linux.patch $(WORKBENCH_BASE)/Makefile
	cd $(@D) && git diff --name-status --exit-code
	cd $(@D) && git checkout --force 9cb78725aba534f86e09bea32257705dc89beab0
	patch -d $(@D) -p1 < Linux.patch
	cd $(@D) && \
		git add --all debian.quantal && \
		git commit --message='build infrastructure' && \
		git add --all
	touch $@
Linux/.git/config:
	test -d Linux && rm -r Linux || true
	git clone -n git://kernel.ubuntu.com/ubuntu/ubuntu-precise.git Linux
endif


BUILD_SAMPLES ?= $(wildcard Samples)
ifneq ($(BUILD_SAMPLES),)
Samples/%.h264: Samples
Samples Samples/:: force
	$(MAKE) -C $@
endif

%.h264 %.mov: %.opt
%.h264 %.mov %.opt: force
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
	-$(MAKE) -C Linux distclean

force:
	

-include $(wildcard .*.d)
