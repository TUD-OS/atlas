all:: bodytrack.log progress.log

include ../Bodytrack/Makefile

bodytrack.lo% progress.lo%: parsec/pkgs/apps/bodytrack/inst/atlas.clang-serial/bin/bodytrack cpuhog progress
	{ sleep 15 ; for ((i=0;i<9;i++)) ; do taskset 1 ./cpuhog & done ; taskset 1 ./progress > progress.log & sleep 30 ; killall cpuhog progress ; } &
	$(call bodytrack,JOB_SCHEDULING=yes) > bodytrack.log

cpuhog: SHELL = /bin/bash
cpuhog: Makefile
	$(CC) -x c -o $@ - <<< 'int main(void) { while (1); }'

progress: %: %.c $(COMPONENTS) Makefile
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ "$(realpath $<)" $(COMPONENTS) $(LDFLAGS)

clean::
	rm -f cpuhog progress *.log
