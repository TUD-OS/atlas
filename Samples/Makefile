.PHONY: all clean cleanall force

.INTERMEDIATE: $(patsubst %.cfg,%.mov,$(wildcard *.cfg)) $(patsubst %.cfg,%.y4m,$(wildcard *.cfg))

X264_CBR = --input-res $(SIZE) --fps $(FPS) --bitrate $(BITRATE) --slices $(SLICES) --sliced-threads --threads $(SLICES)
X264_CRF = --input-res $(SIZE) --fps $(FPS) --crf $(QUALITY) --slices $(SLICES) --sliced-threads --threads $(SLICES)

X264_COMMON = --ref $(REFERENCES) --mixed-refs --cqm flat --direct auto --me umh --merange 16 --partitions all --subme 9 --trellis 2 --rc-lookahead 60
X264_HIGH = --bframes 8 --weightp 2 --8x8dct --b-adapt 2
X264_MAIN = --bframes 8 --weightp 2 --no-8x8dct --b-adapt 2
X264_BASELINE = --no-cabac --bframes 0 --weightp 0 --no-8x8dct

all: $(patsubst %.cfg,%.h264,$(if $(wildcard *.cfg),$(wildcard *.cfg),bbc-r_m720p.cfg))

ifeq ($(INCLUDE),)

%.h264 %.mov %.y4m: %.cfg force
	$(MAKE) $@ INCLUDE=$*.cfg

else

include $(INCLUDE)

%.h264: %.mov %.cfg ../x264/x264 Makefile
	ffmpeg -i $< -f rawvideo - 2> /dev/null | ../x264/x264 $(X264_$(MODE)) $(X264_$(PROFILE)) -o $@ -

%.h264: %.y4m %.cfg ../x264/x264 Makefile
	../x264/x264 $(X264_$(MODE)) $(X264_$(PROFILE)) -o $@ $<

$(notdir $(URL)):
	curl --user-agent QuickTime $(URL) > $@

endif

../x264/x264: force
	$(MAKE) -C .. x264

# some demo video
bbc-r_m720p.cfg: Makefile
	echo 'MODE = CBR\nPROFILE = MAIN\nSLICES = 4\nBITRATE = 4000\nREFERENCES = 3\nSIZE = 1280x720\nFPS = 24\nURL = http://movies.apple.com/movies/us/hd_gallery/gl1800/bbc-r_m720p.mov' > $@

clean:
	rm -f *.h264 *.h264_*

cleanall: clean
	rm -f *.mov

force:
	
