override CPPFLAGS = -DLLSP_PREDICT -DJOB_SCHEDULING

WORKBENCH_BASE ?= ../../..
include $(WORKBENCH_BASE)/Components/Makefile

all: jobs.o

jobs.c: $(WORKBENCH_BASE)/Components/jobs.c jobs.patch
	cp $< $@
	patch < $*.patch
