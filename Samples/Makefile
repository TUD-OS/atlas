.PHONY: all clean

X264_OPTIONS_CBR = --input-res $(SIZE) --bitrate $(BITRATE) --slices $(SLICES) --sliced-threads --ref $(REFERENCES) --mixed-refs --bframes 6 --weightb --direct auto --b-pyramid normal --me umh --subme 9 --analyse all --8x8dct --trellis 1 --psy-rd 1:1

ifneq ($(INCLUDE),)
include $(INCLUDE)
endif

all: $(patsubst %.opt,%.h264,$(if $(wildcard *.opt),$(wildcard *.opt),bbc-r_m720p.opt))

%.h264: %.mov %.opt ../x264/x264
ifeq ($(MODE),)
	$(MAKE) $@ INCLUDE=$*.opt
else
	ffmpeg -i $< -f rawvideo - 2> /dev/null | ../x264/x264 $(X264_OPTIONS_$(MODE)) -o $@ -
endif

%.h264: %.yuv %.opt ../x264/x264
ifeq ($(MODE),)
	$(MAKE) $@ INCLUDE=$*.opt
else
	../x264/x264 $(X264_OPTIONS_$(MODE)) -o $@ $< $(SIZE)
endif

../x264/x264: ../x264
../x264:
	mkdir ../x264
	$(MAKE) -C $(@D) $(@F)

# some demo video
bbc-r_m720p.opt:
	echo 'MODE = CBR\nSLICES = 4\nBITRATE = 4000\nREFERENCES = 3\nSIZE = 1280x720' > $@
bbc-r_m720p.mov:
	curl http://movies.apple.com/movies/us/hd_gallery/gl1800/bbc-r_m720p.mov > $@

clean:
	rm -f *.h264 *.p264 *.prop
