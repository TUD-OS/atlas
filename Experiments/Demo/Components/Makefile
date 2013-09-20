override CPPFLAGS = -DLLSP_PREDICT -DDISPATCH_ATLAS -DJOB_SCHEDULING

WORKBENCH_BASE ?= ../../..
include $(WORKBENCH_BASE)/Components/Makefile

all: estimator.o

estimator.c: $(WORKBENCH_BASE)/Components/estimator.c estimator.patch
	cp $< $@
	patch < $*.patch
