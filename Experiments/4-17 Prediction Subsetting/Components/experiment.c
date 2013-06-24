/*
 * Copyright (C) 2006-2013 Michael Roitzsch <mroi@os.inf.tu-dresden.de>
 * economic rights: Technische Universitaet Dresden (Germany)
 */

#define IMPLEMENTS_HOOKS

#include <stdio.h>
#include <string.h>

#include "dispatch.h"
#include "estimator.h"


void hook_llsp_result(const double *result, size_t count)
{
	dispatch_queue_t queue = dispatch_get_current_queue();
	const char *label = dispatch_queue_get_label(queue);
	
	if (label && strcmp(label, "video") == 0) {
		/* decoder stage */
		for (size_t i = 0; i < count; i++)
			printf("%d ", result[i] != 0.0);
		puts("");
	}
}
