/*
 * Copyright (C) 2006-2015 Michael Roitzsch <mroi@os.inf.tu-dresden.de>
 * economic rights: Technische Universitaet Dresden (Germany)
 */

#include "process.h"


#if SLICE_SKIP

int perform_slice_skip(const AVCodecContext *c)
{
	static int local_slice_count = 0;
	static int skip = 0;
	
	/* handle replacement and error propagation of the PREVIOUS slice */
	if (c->metrics.type != PSEUDO_SLICE_FRAME_START) {
		/* skipping cannot have happened before the first slice */
		if (skip) {
			/* the last slice has been skipped, let's do the replacement */
			float immission;
			immission = do_replacement(c, (const AVPicture *)c->frame.current, local_slice_count, NULL);
			/* replacement finished */
			skip = 0;
		}
	}
	
	if (c->metrics.type != PSEUDO_SLICE_FRAME_START) {
		/* update local_slice_count to the number of the NEXT slice */
		if (c->slice.flag_last)
			local_slice_count = 0;
		else
			local_slice_count++;
	}
	
	/* check for skipping of the NEXT slice */
	if (!c->slice.flag_last) {
		/* there is nothing to skip after the last slice */
		if (schedule_skip(c, local_slice_count))
		/* we skip the next slice, the replacement will be done when we meet here again before the next slice */
		/* skip that many macroblocks */
			skip = proc.frame->slice[local_slice_count].end_index - proc.frame->slice[local_slice_count].start_index;
	}
	
#if METRICS_EXTRACT || PREPROCESS
	/* do not skip for real yet, just don't keep and cross-slice state */
	return 0;
#else
	/* actually advise FFmpeg to drop the decoding of the upcoming slice */
	return skip;
#endif
}

#ifdef FINAL_SCHEDULING
int schedule_skip(const AVCodecContext *c, int current_slice)
{
	static double frame_duration = -1.0;
	static double frame_deadline = -1.0;
	int deadlines_missed = 0;
	frame_node_t *frame;
	int slice;
	
	if (!proc.llsp.predict_coeffs || !proc.frame) return 0;
	
#if SCHEDULING_METHOD == NO_SKIP
	if (c->frame.flag_idr) frame_deadline = -1.0;
#endif
	
	if (frame_duration < 0) {
		float framerate;
		const char *const envvar = getenv("FRAMERATE");
		if (!envvar) {
			printf("the scheduler's target framerate is expected in the environment variable FRAMERATE\n");
			exit(1);
		}
		sscanf(envvar, "%f", &framerate);
		frame_duration = 1.0 / framerate;
	}
	
	if (frame_deadline < 0)
    /* initialize time */
		frame_deadline = get_time();
	
	for (frame = proc.frame; frame; frame = frame->next)
		for (slice = 0; slice < frame->slice_count; slice++)
			frame->slice[slice].skip = 0;
	
	do {
		frame_node_t *least_useful_frame = NULL;
		int least_useful_slice = 0;
		double least_useful_benefit = HUGE_VAL;
		double budget;
		
		budget = frame_deadline - get_time();
		/* we simulate the maximum output frame queue here */
		if (budget > output_queue * frame_duration) {
			frame_deadline += output_queue * frame_duration - budget;
			budget = output_queue * frame_duration;
		}
		deadlines_missed = 0;
		
		for (frame = proc.frame; frame; frame = frame->next) {
			/* refresh the time budget with one frame worth of time */
			budget += frame_duration;
			
			for (slice = (frame == proc.frame ? current_slice : 0); slice < frame->slice_count; slice++) {
				if (frame->slice[slice].skip) {
					/* deplete the budget by the estimated replacement time */
					budget -= frame->slice[slice].replacement_time;
				} else {
					/* deplete the budget by the estimated decoding time */
					budget -= frame->slice[slice].decoding_time;
					/* remember the slice with the least benefit */
					if (frame->slice[slice].benefit < least_useful_benefit) {
						least_useful_benefit = frame->slice[slice].benefit;
						least_useful_frame = frame;
						least_useful_slice = slice;
					}
				}
			}
			
			if (budget < 0.0) {
				/* we have overrun our budget, skip the least useful slice */
				if (least_useful_frame) {
					least_useful_frame->slice[least_useful_slice].skip = 1;
					deadlines_missed = 1;
				}
				break;
			}
		}
	} while (deadlines_missed && !proc.frame->slice[current_slice].skip);
	
	
	if (current_slice == proc.frame->slice_count - 1)
    /* last slice, prepare the deadline */
		frame_deadline += frame_duration;
	
#if SCHEDULING_METHOD == NO_SKIP
	printf("%d\n", 3 * ((proc.frame->replacement) && (frame_deadline - get_time() < 0)));
	return 0;
#else
	printf("%d\n", proc.frame->slice[current_slice].skip);
	return proc.frame->slice[current_slice].skip;
#endif
}
#endif

#ifdef SCHEDULE_EXECUTE
int schedule_skip(const AVCodecContext *c, int current_slice)
{
	int conceal;
	
	fscanf(stdin, "%d\n", &conceal);
	switch (conceal) {
		case 2:
			/* use FFmpeg's concealment */
			proc.schedule.conceal = 1;
			proc.schedule.first_to_drop = -1;
			break;
		case 3:
			/* drop the whole frame */
			proc.schedule.conceal = 0;
			if (proc.schedule.first_to_drop < 0)
				proc.schedule.first_to_drop = c->frame.current->coded_picture_number;
			conceal = 0;
			break;
		default:
			proc.schedule.conceal = 0;
			proc.schedule.first_to_drop = -1;
	}
	return conceal;
}
#endif

#endif
