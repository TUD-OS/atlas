.PHONY: all clean cleanall environment

all clean:
	$(MAKE) environment
	$(MAKE) -C 01\ Preprocess $@
	$(MAKE) -C 02\ CFS\ with\ Load $@
	$(MAKE) -C 03\ ATLAS\ with\ Load $@
	$(MAKE) -C 04\ Prediction\ Full $@
	$(MAKE) -C 05\ Prediction\ Reduced $@
	$(MAKE) -C 06\ Prediction\ None $@
	$(MAKE) -C 07\ Full\ with\ Load $@
	$(MAKE) -C 08\ Reduced\ with\ Load $@
	$(MAKE) -C 09\ None\ with\ Load $@

%:
	$(MAKE) -C '$(wildcard $@*)'

environment:
	@uname -sr | grep 'Linux.*atlas' || { echo '*** Boot the ATLAS kernel. ***' ; false ; }
	@fgrep performance /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor || { echo '*** Use the "performance" CPU frequency governor. ***' ; false ; }
	@! grep -v 'lo$$' /proc/net/if_inet6 || { echo '*** Disable networking. ***' ; false ; }
	@! hciconfig | fgrep UP || { echo '*** Disable Bluetooth. ***' ; false ; }
	@! ps -eo pid,rtprio,cmd | fgrep -v fgrep | fgrep ' - /usr/bin/X ' || { echo '*** Boost X to real-time priority (chrt -p 5 <PID>). ***' ; false ; }

cleanall: clean
