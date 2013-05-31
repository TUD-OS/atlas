.PHONY: all clean cleanall environment

all: environment

all clean:
	$(MAKE) -C 1-01\ Decoding\ Times $@
	$(MAKE) -C 2-01\ GCD\ Compatibility $@
	$(MAKE) -C 2-02\ FFplay\ Characteristics $@
	$(MAKE) -C 3-01\ Background\ Load $@
	$(MAKE) -C 3-02\ Decoding\ Times\ Repeat $@
	$(MAKE) -C 3-03\ Times\ and\ Metrics $@
	$(MAKE) -C 3-04\ Preprocess\ Metrics $@
	$(MAKE) -C 3-05\ Prediction\ Full $@
	$(MAKE) -C 3-06\ Prediction\ Reduced $@
	$(MAKE) -C 3-07\ Prediction\ None $@
	$(MAKE) -C 3-08\ Prediction\ Input\ Output $@
	$(MAKE) -C 3-09\ Prediction\ Overhead $@
	$(MAKE) -C 3-10\ Prediction\ Quick\ Full $@
	$(MAKE) -C 3-11\ Prediction\ Quick\ Reduced $@
	$(MAKE) -C 3-12\ Prediction\ Quick\ None $@
	$(MAKE) -C 3-13\ Prediction\ Slow\ Full $@
	$(MAKE) -C 3-14\ Prediction\ Slow\ Reduced $@
	$(MAKE) -C 3-15\ Prediction\ Slow\ None $@
	$(MAKE) -C 3-16\ Prediction\ No\ Drop $@
	$(MAKE) -C 3-17\ Prediction\ More\ Drop $@
	$(MAKE) -C 3-18\ Prediction\ Subsetting $@
	$(MAKE) -C 3-29\ Background\ ATLAS $@

%:
	for subdir in $@* ; do $(MAKE) -C "$$subdir" ; done

# suggestion: to prevent the sudo prompt, put this in /etc/sudoers.d/
# $USER ALL = NOPASSWD: /bin/dd of=/proc/sys/kernel/sched_atlas_advance_in_cfs
# $USER ALL = NOPASSWD: /sbin/modprobe msr
# $USER ALL = NOPASSWD: /usr/sbin/rdmsr
# $USER ALL = NOPASSWD: /usr/sbin/wrmsr

environment:
	@uname -sr | grep -q 'Linux.*atlas' || { echo '*** Boot the ATLAS kernel. ***' ; false ; }
	@fgrep -q 1 /proc/sys/kernel/sched_atlas_advance_in_cfs || echo -n 1 | sudo dd of=/proc/sys/kernel/sched_atlas_advance_in_cfs
	@fgrep -q performance /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor || { echo '*** Use the "performance" CPU frequency governor. ***' ; false ; }
	@lsmod | fgrep -q msr || sudo modprobe msr
	@sudo rdmsr 0x1a0 --bitfield 38:38 | fgrep -q 1 || sudo wrmsr 0x1a0 $$((`sudo rdmsr 0x1a0 --decimal` | 0x4000000000))  # disable Intel turbo mode
	@! grep -qv 'lo$$' /proc/net/if_inet6 || { echo '*** Disable networking. ***' ; false ; }
	@! hciconfig | fgrep -q UP || { echo '*** Disable Bluetooth. ***' ; false ; }
	@! ps -eo pid,rtprio,cmd | fgrep -v fgrep | fgrep ' - /usr/bin/X ' || { echo '*** Boost X to real-time priority (chrt -p 5 <PID>). ***' ; false ; }

cleanall: clean
