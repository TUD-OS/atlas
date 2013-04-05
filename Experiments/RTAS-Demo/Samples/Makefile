WORKBENCH_BASE ?= ../../..
include $(WORKBENCH_BASE)/Samples/Makefile

ifneq ($(INCLUDE),)
%.h264: %.mov %.cfg ../x264/x264 Makefile
	ffmpeg -i $< -s $(SIZE) -f rawvideo - 2> /dev/null | ../x264/x264 $(X264_$(MODE)) $(X264_$(PROFILE)) -o $@ -
endif
