all:: hungergames-tlr1_h1080p_nice-0.log hungergames-tlr1_h1080p_nice-5.log

WORKBENCH_BASE ?= ../..
include $(WORKBENCH_BASE)/Makefile

experiment = \
	dd of=/dev/null bs=1024x1024 count=100 < $< ; : load first 100MiB into buffer cache ; \
	{ sleep 60 ; for ((i=0;i<10;i++)) ; do taskset 1 ./cpuhog & done ; sleep 60 ; killall cpuhog ; } & \
	$(if $(1),sudo -n nice -n $(1)) ./ffplay -x 960 -y 408 -threads 1 -nostats -noframedrop -autoexit $< > $@

# suggestion: to prevent the sudo prompt, put this in /etc/sudoers.d/
# $USER ALL = NOPASSWD: /usr/bin/nice

%.log: SHELL = /bin/bash

%_nice-0.log: Samples/%.h264 cpuhog ffplay Makefile
	$(call experiment)

%_nice-5.log: Samples/%.h264 cpuhog ffplay Makefile
	$(call experiment,-5)

cpuhog: SHELL = /bin/bash
cpuhog: Makefile
	$(CC) -x c -o $@ - <<< 'int main(void) { while (1); }'

clean::
	rm -f cpuhog *.log
