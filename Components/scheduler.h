/*
 * Copyright (C) 2006-2012 Michael Roitzsch <mroi@os.inf.tu-dresden.de>
 * economic rights: Technische Universitaet Dresden (Germany)
 */

#ifdef __linux__

#include <sys/types.h>
#include <sys/time.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <errno.h>

#if defined(__x86_64__)
#define __NR_atlas_next   313
#define __NR_atlas_submit 314
#define __NR_atlas_debug  315
#elif defined(__i386__)
#define __NR_atlas_next   350
#define __NR_atlas_submit 351
#define __NR_atlas_debug  352
#else
#error Architecture not supported.
#endif

static inline pid_t gettid(void)
{
	return syscall(SYS_gettid);
}

static inline int atlas_submit(pid_t pid, struct timeval *exectime, struct timeval *deadline)
{
	if (syscall(__NR_atlas_submit, pid, exectime, deadline))
		return errno;
	else
		return 0;
}

inline int atlas_next(void)
{
	return syscall(__NR_atlas_next);
}

inline int atlas_debug(void)
{
	return syscall(__NR_atlas_debug);
}

#else

#include <sys/types.h>
#include <sys/time.h>
#include <errno.h>

#warning Jobs will not be forwarded to the scheduler.

static inline pid_t gettid(void)
{
	return 0;
}

static inline int atlas_submit(pid_t pid, struct timeval *exectime, struct timeval *deadline)
{
	return ENOTSUP;
}

inline int atlas_next(void)
{
	return ENOTSUP;
}

inline int atlas_debug(void)
{
	return ENOTSUP;
}

#endif

