.PHONY: all clean cleanall environment

all: environment

all clean:
	$(MAKE) -C 01\ Preprocess $@
	$(MAKE) -C 02\ CFS\ with\ Load $@
	$(MAKE) -C 03\ ATLAS\ with\ Load $@
	$(MAKE) -C 04\ Prediction\ Full $@
	$(MAKE) -C 05\ Prediction\ Reduced $@
	$(MAKE) -C 06\ Prediction\ None $@
	$(MAKE) -C 07\ Full\ with\ Load $@
	$(MAKE) -C 08\ Reduced\ with\ Load $@
	$(MAKE) -C 09\ None\ with\ Load $@
	$(MAKE) -C 10\ CFS\ with\ Load $@

%:
	$(MAKE) -C '$(wildcard $@*)'

environment:
	@uname -sr | grep -q 'Linux.*atlas' || { echo '*** Boot the ATLAS kernel. ***' ; false ; }
	@fgrep -q 1 /proc/sys/kernel/sched_atlas_advance_in_cfs || { echo '*** Enable ATLAS option to advance in CFS. (/proc/sys/kernel/sched_atlas_advance_in_cfs) ***' ; false ; }
	@fgrep -q performance /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor || { echo '*** Use the "performance" CPU frequency governor. ***' ; false ; }
	@! grep -qv 'lo$$' /proc/net/if_inet6 || { echo '*** Disable networking. ***' ; false ; }
	@! hciconfig | fgrep -q UP || { echo '*** Disable Bluetooth. ***' ; false ; }
	@! ps -eo pid,rtprio,cmd | fgrep -v fgrep | fgrep ' - /usr/bin/X ' || { echo '*** Boost X to real-time priority (chrt -p 5 <PID>). ***' ; false ; }

cleanall: clean
