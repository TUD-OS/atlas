.PHONY: all clean cleanall environment

all: environment

all clean:
	$(MAKE) -C 1-01\ Decoding\ Times $@
	$(MAKE) -C 2-01\ GCD\ Compatibility $@
	$(MAKE) -C 2-02\ FFplay\ Characteristics $@
	$(MAKE) -C 3-01\ Background\ Load $@
	$(MAKE) -C 4-01\ Decoding\ Times\ Repeat $@
	$(MAKE) -C 4-02\ Times\ and\ Metrics $@
	$(MAKE) -C 4-03\ Preprocess\ Metrics $@
	$(MAKE) -C 4-04\ Prediction\ Full $@
	$(MAKE) -C 4-05\ Prediction\ Reduced $@
	$(MAKE) -C 4-06\ Prediction\ None $@
	$(MAKE) -C 4-07\ Prediction\ Input\ Output $@
	$(MAKE) -C 4-08\ Prediction\ Overhead $@
	$(MAKE) -C 4-09\ Prediction\ Quick\ Full $@
	$(MAKE) -C 4-10\ Prediction\ Quick\ Reduced $@
	$(MAKE) -C 4-11\ Prediction\ Quick\ None $@
	$(MAKE) -C 4-12\ Prediction\ Slow\ Full $@
	$(MAKE) -C 4-13\ Prediction\ Slow\ Reduced $@
	$(MAKE) -C 4-14\ Prediction\ Slow\ None $@
	$(MAKE) -C 4-15\ Prediction\ No\ Drop $@
	$(MAKE) -C 4-16\ Prediction\ More\ Drop $@
	$(MAKE) -C 4-17\ Prediction\ Subsetting $@
	$(MAKE) -C 6-01\ Bodytrack\ Alone $@
	$(MAKE) -C 6-02\ Bodytrack\ CFS $@
	$(MAKE) -C 6-03\ Bodytrack\ ATLAS $@
	$(MAKE) -C 6-04\ Bodytrack\ RT $@
	$(MAKE) -C 6-05\ Bodytrack\ RT\ Fail $@
	$(MAKE) -C 6-06\ Bodytrack\ ATLAS\ Fail $@
	$(MAKE) -C 6-07\ UI-Worker\ CFS\ Alone $@
	$(MAKE) -C 6-08\ UI-Worker\ ATLAS\ Alone $@
	$(MAKE) -C 6-09\ UI-Worker\ CFS\ Load $@
	$(MAKE) -C 6-10\ UI-Worker\ ATLAS\ Load $@
	$(MAKE) -C 6-11\ Background\ ATLAS $@
	$(MAKE) -C 6-12\ Scheduling\ Full $@
	$(MAKE) -C 6-13\ Scheduling\ Reduced $@
	$(MAKE) -C 6-14\ Scheduling\ None $@
	$(MAKE) -C 6-15\ Scheduling\ CFS $@
	$(MAKE) -C 6-16\ Scheduling\ Light\ Full $@
	$(MAKE) -C 6-17\ Scheduling\ Light\ Reduced $@
	$(MAKE) -C 6-18\ Scheduling\ Light\ None $@
	$(MAKE) -C 6-19\ Scheduling\ Light\ CFS $@
	$(MAKE) -C 6-20\ Scheduling\ Max\ Full $@
	$(MAKE) -C 6-21\ Scheduling\ Max\ Reduced $@
	$(MAKE) -C 6-22\ Scheduling\ Max\ None $@
	$(MAKE) -C 6-23\ Scheduling\ Precise\ Full $@
	$(MAKE) -C 6-24\ Scheduling\ Precise\ Reduced $@
	$(MAKE) -C 6-25\ Scheduling\ Precise\ None $@
	$(MAKE) -C 6-26\ Scheduling\ MSE\ Full $@
	$(MAKE) -C 6-27\ Scheduling\ MSE\ Reduced $@
	$(MAKE) -C 6-28\ Scheduling\ MSE\ None $@
	$(MAKE) -C 6-29\ Scheduling\ Tight\ Full $@
	$(MAKE) -C 6-30\ Scheduling\ Tight\ Reduced $@
	$(MAKE) -C 6-31\ Scheduling\ Tight\ None $@

%:
	for subdir in $@* ; do $(MAKE) -C "$$subdir" ; done

# suggestion: to prevent the sudo prompt, put this in /etc/sudoers.d/
# $USER ALL = NOPASSWD: /bin/dd of=/proc/sys/kernel/sched_atlas_advance_in_cfs
# $USER ALL = NOPASSWD: /sbin/modprobe msr
# $USER ALL = NOPASSWD: /usr/sbin/rdmsr
# $USER ALL = NOPASSWD: /usr/sbin/wrmsr

environment:
	$(MAKE) -C Bodytrack
	@uname -sr | grep -q 'Linux.*atlas' || { echo '*** Boot the ATLAS kernel. ***' ; false ; }
	@fgrep -q 1 /proc/sys/kernel/sched_atlas_advance_in_cfs || echo -n 1 | sudo dd of=/proc/sys/kernel/sched_atlas_advance_in_cfs
	@fgrep -q performance /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor || { echo '*** Use the "performance" CPU frequency governor. ***' ; false ; }
	@lsmod | fgrep -q msr || sudo modprobe msr
	@sudo rdmsr 0x1a0 --bitfield 38:38 | fgrep -q 1 || sudo wrmsr 0x1a0 $$((`sudo rdmsr 0x1a0 --decimal` | 0x4000000000))  # disable Intel turbo mode
	@! grep -qv 'lo$$' /proc/net/if_inet6 || { echo '*** Disable networking. ***' ; false ; }
	@! hciconfig | fgrep -q UP || { echo '*** Disable Bluetooth. ***' ; false ; }
	@! ps -eo pid,rtprio,cmd | fgrep -v fgrep | fgrep ' - /usr/bin/X ' || { echo '*** Boost X to real-time priority (chrt -p 5 <PID>). ***' ; false ; }

cleanall: clean
