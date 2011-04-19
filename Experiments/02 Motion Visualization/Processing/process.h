/*
 * Copyright (C) 2006 Michael Roitzsch <mroi@os.inf.tu-dresden.de>
 * economic rights: Technische Universitaet Dresden (Germany)
 */

#include "libavcodec/avcodec.h"

/* FFmpeg wants to replace those, but I want to use them */
#undef printf
#undef fprintf

void process_init(AVCodecContext *c, const char *file);
void process_finish(AVCodecContext *c);
