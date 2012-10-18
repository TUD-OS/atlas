all:: hungergames-tlr1_h1080p.log

CPPFLAGS = -DMETRICS_REDUCED

WORKBENCH_BASE ?= ../..
include $(WORKBENCH_BASE)/Makefile

%.log: SHELL = /bin/bash
%.log: Samples/%.p264 cpuhog ffplay Makefile
	{ sleep 10 ; for ((i=0;i<10;i++)) ; do taskset 1 ./cpuhog & done ; sleep 120 ; killall cpuhog ; } & \
	./ffplay -x 960 -y 540 -threads 1 -nostats -noframedrop -autoexit $< > $@

cpuhog: Makefile
	$(CC) -x c -o $@ - <<< 'int main(void) { while (1); }'

%.p264: force
	$(MAKE) -C ../01\ Preprocess $@

clean::
	rm -f cpuhog *.log
