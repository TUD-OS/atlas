all:: $(addsuffix .h264_metrics,$(basename $(wildcard Samples/*.*)))

WORKBENCH_BASE ?= ../..
include $(WORKBENCH_BASE)/Makefile

filesize_cmd = case $$(uname) in (Darwin) stat -L -f%z $(1) ;; (Linux) stat -L -c%s $(1) ;; esac

Sizes.log: all

%.h264_metrics: %.h264 h264_workbench
	./h264_workbench $<
	touch Sizes.log
	sed '/^$(notdir $*)/d' Sizes.log > Sizes.log_
	echo "$(notdir $*) `$(call filesize_cmd,$<)` `$(call filesize_cmd,$@)`" | cat Sizes.log_ - | sort > Sizes.log
	rm Sizes.log_
