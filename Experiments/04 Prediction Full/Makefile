all:: $(addsuffix .log,$(basename $(notdir $(wildcard Samples/*.*))))

CPPFLAGS = -DMETRICS_FULL

WORKBENCH_BASE ?= ../..
include $(WORKBENCH_BASE)/Makefile

%.log: Samples/%.p264 ffplay Makefile
	./ffplay -threads 1 -nostats -noframedrop -nodisp -autoexit $< > $@

%.p264: force
	$(MAKE) -C ../01\ Preprocess $@

clean::
	rm -f *.log
