/*
 * Copyright (C) 2006-2012 Michael Roitzsch <mroi@os.inf.tu-dresden.de>
 * economic rights: Technische Universitaet Dresden (Germany)
 *
 * Copyright (C) 2012 Stefan Wächtler
 */

#ifdef __linux__

#include <sys/types.h>
#include <sys/time.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <errno.h>

#if defined(__x86_64__)
#define SYS_atlas_next   313
#define SYS_atlas_submit 314
#define SYS_atlas_debug  315
#elif defined(__i386__)
#define SYS_atlas_next   350
#define SYS_atlas_submit 351
#define SYS_atlas_debug  352
#else
#error Architecture not supported.
#endif

static inline pid_t gettid(void)
{
	return syscall(SYS_gettid);
}

static inline int atlas_submit(pid_t pid, struct timeval *exectime, struct timeval *deadline)
{
	if (syscall(SYS_atlas_submit, pid, exectime, deadline) == 0)
		return 0;
	else
		return errno;
}

inline int atlas_next(void)
{
	return syscall(SYS_atlas_next);
}

inline int atlas_debug(void)
{
	return syscall(SYS_atlas_debug);
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

