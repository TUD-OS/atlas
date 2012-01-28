/*
 * Copyright (C) 2006-2010 Michael Roitzsch <mroi@os.inf.tu-dresden.de>
 * economic rights: Technische Universitaet Dresden (Germany)
 */

#include "process.h"


int perform_slice_skip(const AVCodecContext *c)
{
	/* never skip slices in the actual decoding stream,
	 * we'll do that in separate visualization code */
	return 0;
}
