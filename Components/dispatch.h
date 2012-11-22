/*
 * Copyright (C) 2006-2012 Michael Roitzsch <mroi@os.inf.tu-dresden.de>
 * economic rights: Technische Universitaet Dresden (Germany)
 */

#include <stddef.h>
#include <string.h>

#include <dispatch/dispatch.h>
#include "estimator.h"

#ifndef DISPATCH_QUEUE_SERIAL
#define DISPATCH_QUEUE_SERIAL 0
#endif

/* use my ATLAS-modified minimal libdispatch or the original */
#ifdef DISPATCH_ATLAS
#	define DISPATCH_ATLAS 1
#else
#	define DISPATCH_ATLAS 0
#endif


#ifdef __cplusplus
extern "C" {
#endif

/* main developer-facing API for ATLAS */
void dispatch_async_atlas(dispatch_queue_t queue, atlas_job_t job, dispatch_block_t block);

/* for experiments: given a NULL-terminated array of queue labels, get the current queue's index */
static inline size_t dispatch_queue_id(const char * const *queue_labels)
{
	dispatch_queue_t queue = dispatch_get_current_queue();
	const char *label = dispatch_queue_get_label(queue);
	size_t id;
	
	for (id = 0; queue_labels[id]; id++)
		if (label && strcmp(label, queue_labels[id]) == 0) break;
	return id;
}

#ifdef __cplusplus
}
#endif
