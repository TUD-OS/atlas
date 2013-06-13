all:: bodytrack.log

include ../Bodytrack/Makefile

bodytrack.log: parsec/pkgs/apps/bodytrack/inst/atlas.clang-serial/bin/bodytrack
	$(call bodytrack,LOG_TIMES=yes) > $@

clean::
	rm -f *.log
