/*
 * Copyright (C) 2006-2012 Michael Roitzsch <mroi@os.inf.tu-dresden.de>
 * economic rights: Technische Universitaet Dresden (Germany)
 */

#include <libavformat/avformat.h>
#include "estimator.h"

/* empty implementation */
void atlas_job_submit(void *code, pid_t tid, atlas_job_t job) {}
void atlas_job_next(void *code) {}
void atlas_job_train(void *code) {}
double atlas_now(void) { return av_gettime() / 1000000.0; }
