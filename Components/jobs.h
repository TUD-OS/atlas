/*
 * Copyright (C) 2006-2012 Michael Roitzsch <mroi@os.inf.tu-dresden.de>
 * economic rights: Technische Universitaet Dresden (Germany)
 */

/* toggle job communication to scheduler */
#ifdef JOB_SCHEDULING
#	define JOB_SCHEDULING 1
#else
#	define JOB_SCHEDULING 0
#endif

/* toggle time prediction */
#ifdef LLSP_PREDICT
#	define LLSP_PREDICT 1
#else
#	define LLSP_PREDICT 0
#endif

#ifndef IMPLEMENTS_HOOKS
#  define WEAK_SYMBOL __attribute__((weak))
#else
#  define WEAK_SYMBOL
#endif


/* thread registration: call before the first job from the thread that will execute the code */
void atlas_job_queue_checkin(void *code);

/* this job will never be submitted again, dispose internal state */
void atlas_job_queue_terminate(void *code);

/* job management */
void atlas_job_submit_absolute(void *code, double deadline, unsigned count, const double metrics[]);
void atlas_job_submit_relative(void *code, double deadline, unsigned count, const double metrics[]);
void atlas_job_next(void *code);

/* hooks for evaluation */
extern void hook_job_release(void *code) WEAK_SYMBOL;
extern void hook_job_complete(void *code, double thread_time, double deadline, double prediction, double execution) WEAK_SYMBOL;

/* stage info from ffplay, may be helpful in the hooks */
struct stages_s { void *code; const char *name; int id; } ffplay_stages[5];
