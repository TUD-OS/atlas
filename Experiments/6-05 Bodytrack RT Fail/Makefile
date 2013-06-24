all:: bodytrack.log progress.log

include ../Bodytrack/Makefile

# suggestion: to prevent the sudo prompt, put this in /etc/sudoers.d/
# $USER ALL = NOPASSWD: SETENV: /usr/bin/chrt

bodytrack.lo% progress.lo%: parsec/pkgs/apps/bodytrack/inst/atlas.clang-serial/bin/bodytrack cpuhog progress
	{ sleep 15 ; for ((i=0;i<9;i++)) ; do taskset 1 ./cpuhog & done ; taskset 1 ./progress > progress.log ; } &
	$(call bodytrack,sudo FAIL=yes chrt 10) > bodytrack.log
	killall cpuhog progress

cpuhog: SHELL = /bin/bash
cpuhog: Makefile
	$(CC) -x c -o $@ - <<< 'int main(void) { while (1); }'

progress: %: %.c $(COMPONENTS) Makefile
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ "$(realpath $<)" $(COMPONENTS) $(LDFLAGS)

clean::
	rm -f cpuhog progress *.log
