.PHONY: all clean cleanall force

X264_CBR = --input-res $(SIZE) --fps $(FPS) --bitrate $(BITRATE) --slices $(SLICES) --sliced-threads --threads $(SLICES)
X264_CRF = --input-res $(SIZE) --fps $(FPS) --crf $(QUALITY) --slices $(SLICES) --sliced-threads --threads $(SLICES)

X264_COMMON = --ref $(REFERENCES) --mixed-refs --cqm flat --direct auto --me umh --merange 16 --partitions all --subme 9 --trellis 2 --rc-lookahead 60
X264_HIGH = --bframes 8 --weightp 2 --8x8dct --b-adapt 2
X264_MAIN = --bframes 8 --weightp 2 --no-8x8dct --b-adapt 2
X264_BASELINE = --no-cabac --bframes 0 --weightp 0 --no-8x8dct

all: $(patsubst %.cfg,%.h264,$(wildcard *.cfg))

ifeq ($(INCLUDE),)

%.h264 %.mov %.y4m: %.cfg force
	$(MAKE) $@ INCLUDE=$*.cfg

else

include $(INCLUDE)

.INTERMEDIATE: $(notdir $(URL))

%.h264: %.mov %.cfg ../x264/x264 Makefile
	ffmpeg -i $< -s $(SIZE) -f rawvideo - 2> /dev/null | ../x264/x264 $(X264_$(MODE)) $(X264_$(PROFILE)) -o $@ -

%.h264: %.y4m %.cfg ../x264/x264 Makefile
	../x264/x264 $(X264_$(MODE)) $(X264_$(PROFILE)) -o $@ $<

%.h264: %.264 %.cfg
	ln -sf $< $@

%.h264: %.m4v %.cfg
	ffmpeg -i $< -vcodec copy -vbsf h264_mp4toannexb -f h264 -y $@

%.h264: %.ts %.cfg
	ffmpeg -i $< -vcodec copy -vframes $(FRAMES) -f h264 -y $@

Sintel-4k.h264: %.h264: %.cfg ../x264/x264 Makefile
	$(eval SHELL = bash)
	for ((frame=1; frame<=21312; frame++)) ; do \
		ffmpeg -i `printf $(URL) $$frame` -f rawvideo -pix_fmt yuv420p - || ((frame--)) ; \
	done 2> /dev/null | ../x264/x264 $(X264_$(MODE)) $(X264_$(PROFILE)) -o $@ -

$(notdir $(URL)):
	curl --user-agent QuickTime $(URL) > $@

endif

../x264/x264: force
	$(MAKE) -C .. x264

clean:
	rm -f *.h264 *.h264_*

cleanall: clean
	rm -f *.mov *.y4m

force:
	
