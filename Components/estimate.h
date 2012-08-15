/*
 * Copyright (C) 2006-2012 Michael Roitzsch <mroi@os.inf.tu-dresden.de>
 * economic rights: Technische Universitaet Dresden (Germany)
 */

#include <stdint.h>

/* thread registration */
void atlas_thread_checkin(unsigned queue);
void atlas_thread_checkout(unsigned queue);

/* job management */
void atlas_job_submit(unsigned target, double deadline, unsigned count, const double metrics[]);
void atlas_job_next(unsigned queue);
