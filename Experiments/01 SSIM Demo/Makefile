all:: Demo.mp4

WORKBENCH_BASE ?= ../..
include $(WORKBENCH_BASE)/Makefile

ssim_demo: ssim_demo.c $(PROCESSING_OBJS)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MF .$@.d -o $@ $< $(PROCESSING_OBJS)

Demo.mp4: ../../Samples/bbc-r_m720p.mov ssim_demo
	rm -f Clip1.yuv Clip2.yuv Demo.yuv
	mkfifo Clip1.yuv Clip2.yuv Demo.yuv
	ffmpeg -i $< -f rawvideo -s 640x360 -y Clip1.yuv 2> /dev/null &
	ffmpeg -i $< -f m4v -s 640x360 -r 24 -b 192k - 2> /dev/null | ffmpeg -f m4v -s 640x360 -r 24 -i - -f rawvideo -y Clip2.yuv 2> /dev/null &
	./ssim_demo 1280x720 &
	ffmpeg -f rawvideo -s 1280x720 -r 12 -i Demo.yuv -b 8M -y $@
	tty -s && reset
	rm Clip1.yuv Clip2.yuv Demo.yuv

../../Samples/bbc-r_m720p.mov:
	$(MAKE) -C $(@D) $(@F)

clean::
	rm -f Clip1.yuv Clip2.yuv Demo.yuv Demo.mp4
	rm -f ssim_demo
