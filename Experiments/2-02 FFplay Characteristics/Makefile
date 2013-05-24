all:: \
	720p50_parkrun_ter.log \
	FVDO_Shore_4cif.log \
	blackswan-tlr1c_h480p.log \
	hungergames-tlr1_h1080p.log \
	rush_hour_1080p25.log

override CPPFLAGS = -DDEADLINE_FAR

WORKBENCH_BASE ?= ../..
include $(WORKBENCH_BASE)/Makefile

%.log: Samples/%.h264 ffplay Makefile
	dd of=/dev/null bs=1024x1024 count=100 < $<  # load first 100MiB into buffer cache
	./ffplay -threads 1 -noframedrop -nostats -autoexit $< > $@

clean::
	rm -f *.log
