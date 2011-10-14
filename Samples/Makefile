.PHONY: all clean

.INTERMEDIATE: $(patsubst %.opt,%.y4m,$(wildcard *.opt))

X264_CBR = --input-res $(SIZE) --bitrate $(BITRATE) --slices $(SLICES) --sliced-threads --threads $(SLICES)
X264_CRF = --input-res $(SIZE) --crf $(QUALITY) --slices $(SLICES) --sliced-threads --threads $(SLICES)

X264_HIGH = --ref $(REFERENCES) --mixed-refs --bframes 8 --b-pyramid normal --weightb --weightp 2 --8x8dct --cqm flat --b-adapt 2 --direct auto --me umh  --merange 16 --partitions all --subme 9 --trellis 2 --rc-lookahead 60
X264_MAIN = --no-cabac --ref $(REFERENCES) --bframes 8 --b-pyramid none --no-weightb --weightp 0 --no-8x8dct --cqm flat --b-adapt 2 --direct auto --me umh --merange 16 --partitions all --subme 9 --trellis 2 --rc-lookahead 60
X264_BASELINE = --no-cabac --ref $(REFERENCES) --bframes 0 --weightp 0 --no-8x8dct --cqm flat --direct auto --me umh --merange 16 --partitions all --subme 9 --trellis 2 --rc-lookahead 60

ifneq ($(INCLUDE),)
include $(INCLUDE)
endif

all: $(patsubst %.opt,%.h264,$(if $(wildcard *.opt),$(wildcard *.opt),bbc-r_m720p.opt))

%.h264: %.mov %.opt ../x264/x264
ifeq ($(MODE),)
	$(MAKE) $@ INCLUDE=$*.opt
else
	ffmpeg -i $< -f rawvideo - 2> /dev/null | ../x264/x264 $(X264_$(MODE)) $(X264_$(PROFILE)) -o $@ -
endif

%.h264: %.y4m %.opt ../x264/x264
ifeq ($(MODE),)
	$(MAKE) $@ INCLUDE=$*.opt
else
	../x264/x264 $(X264_$(MODE)) $(X264_$(PROFILE)) -o $@ $<
endif

%.mov %.y4m:
ifeq ($(URL),)
	$(MAKE) $@ INCLUDE=$*.opt
else
	curl --user-agent QuickTime $(URL) > $@
endif

../x264/x264: ../x264
../x264:
	$(MAKE) -C $(@D) $(@F)

# some demo video
bbc-r_m720p.opt:
	echo 'MODE = CBR\nPROFILE = MAIN\nSLICES = 4\nBITRATE = 4000\nREFERENCES = 3\nSIZE = 1280x720\nURL = http://movies.apple.com/movies/us/hd_gallery/gl1800/bbc-r_m720p.mov' > $@

clean:
	rm -f *.h264 *.p264 *.prop
