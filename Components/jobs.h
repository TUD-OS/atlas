/*
 * Copyright (C) 2006-2012 Michael Roitzsch <mroi@os.inf.tu-dresden.de>
 * economic rights: Technische Universitaet Dresden (Germany)
 */

/* thread registration: call before the first job from the thread that will execute the code */
void atlas_job_queue_checkin(void *code);

/* this job will never be submitted again, dispose internal state */
void atlas_job_queue_terminate(void *code);

/* job management */
void atlas_job_submit_absolute(void *code, double deadline, unsigned count, const double metrics[]);
void atlas_job_submit_relative(void *code, double deadline, unsigned count, const double metrics[]);
void atlas_job_next(void *code);
