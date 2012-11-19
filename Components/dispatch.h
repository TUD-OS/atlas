/*
 * Copyright (C) 2006-2012 Michael Roitzsch <mroi@os.inf.tu-dresden.de>
 * economic rights: Technische Universitaet Dresden (Germany)
 */

#include <dispatch/dispatch.h>
#include "estimator.h"

/* use my ATLAS-modified minimal libdispatch or the original */
#ifdef DISPATCH_ATLAS
#	define DISPATCH_ATLAS 1
#else
#	define DISPATCH_ATLAS 0
#endif


#ifdef __cplusplus
extern "C" {
#endif

void dispatch_async_atlas(dispatch_queue_t queue, atlas_job_t job, dispatch_block_t block);

#ifdef __cplusplus
}
#endif
