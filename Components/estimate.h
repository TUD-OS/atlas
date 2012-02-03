/*
 * Copyright (C) 2006-2012 Michael Roitzsch <mroi@os.inf.tu-dresden.de>
 * economic rights: Technische Universitaet Dresden (Germany)
 */

#include <stdint.h>

/* thread registration */
void thread_checkin(unsigned queue);
void thread_checkout(unsigned queue);

/* job management */
void job_submit(unsigned target, double deadline, unsigned count, const double metrics[]);
void job_next(unsigned queue);
void job_start_blocking(unsigned queue);
void job_stop_blocking(unsigned queue);
