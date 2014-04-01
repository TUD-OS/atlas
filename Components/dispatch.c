/*
 * Copyright (C) 2006-2014 Michael Roitzsch <mroi@os.inf.tu-dresden.de>
 * economic rights: Technische Universitaet Dresden (Germany)
 */

#if defined(DISPATCH_ATLAS) && DISPATCH_ATLAS

#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include <Block.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <dlfcn.h>
#include <assert.h>

#include <dispatch/dispatch.h>  // to get dispatch_semaphore_t early
#include "scheduler.h"  // for gettid()

#pragma clang diagnostic ignored "-Wpadded"

typedef struct {
	void (^block)(void);
	bool is_copied;
	bool is_realtime;
	dispatch_semaphore_t *signal_completion;
} dispatch_queue_element_t;

#define BUFFER_TYPE dispatch_queue_element_t
#include "dispatch.h"
#include "estimator.h"

struct dispatch_queue_s {
	uint32_t magic;
	pthread_mutex_t lock;
	uint_fast32_t refcount;
	char *label;
	
	pid_t tid;
	pthread_t worker;
	dispatch_semaphore_t init;
	
	dispatch_semaphore_t work;
	double previous_deadline;
	struct buffer blocks;
};

static void *dispatch_queue_worker(void *);

static pthread_key_t current_queue;

#pragma mark -


#pragma mark Original GCD API

dispatch_queue_t dispatch_queue_create(const char *label, dispatch_queue_attr_t attr)
{
	static dispatch_once_t predicate;
	dispatch_once(&predicate, ^{
		pthread_key_create(&current_queue, NULL);
	});
	
	assert(attr == DISPATCH_QUEUE_SERIAL);
	dispatch_queue_t queue = malloc(sizeof(struct dispatch_queue_s));
	if (!queue) return NULL;
	
	queue->magic = 0x61746C73;  // 'atls'
	queue->refcount = 1;
	queue->label = strdup(label);
	queue->init = dispatch_semaphore_create(0);
	queue->work = dispatch_semaphore_create(0);
	queue->previous_deadline = -INFINITY;
	
	pthread_mutex_init(&queue->lock, NULL);
	pthread_create(&queue->worker, NULL, dispatch_queue_worker, queue);
	dispatch_semaphore_wait(queue->init, DISPATCH_TIME_FOREVER);
	dispatch_release(queue->init);
	
	buffer_init(&queue->blocks);
	
	return queue;
}

const char *dispatch_queue_get_label(dispatch_queue_t queue)
{
	if (queue == DISPATCH_CURRENT_QUEUE_LABEL)
		queue = (dispatch_queue_t)pthread_getspecific(current_queue);
	return queue ? queue->label : NULL;
}

void dispatch_async(dispatch_queue_t queue, dispatch_block_t block)
{
	dispatch_queue_element_t element = {
		.block = Block_copy(block),
		.is_copied = true,
		.is_realtime = false,
		.signal_completion = NULL
	};
	pthread_mutex_lock(&queue->lock);
	assert(queue->refcount);
	buffer_put(&queue->blocks, element);
	queue->refcount++;  // shortcut for calling dispatch_retain
	pthread_mutex_unlock(&queue->lock);
	dispatch_semaphore_signal(queue->work);
}

void dispatch_sync(dispatch_queue_t queue, dispatch_block_t block)
{
	dispatch_semaphore_t complete = dispatch_semaphore_create(0);
	
	dispatch_queue_element_t element = {
		.block = block,
		.is_copied = false,
		.is_realtime = false,
		.signal_completion = &complete
	};
	pthread_mutex_lock(&queue->lock);
	assert(queue->refcount);
	buffer_put(&queue->blocks, element);
	queue->refcount++;  // shortcut for calling dispatch_retain
	pthread_mutex_unlock(&queue->lock);
	dispatch_semaphore_signal(queue->work);
	
	dispatch_semaphore_wait(complete, DISPATCH_TIME_FOREVER);
	dispatch_release(complete);
}

#define ORIGINAL_GCD(function, object) \
	if (queue->magic != 0x61746C73) { \
		static void (*original_dispatch_ ## function)(dispatch_object_t); \
		static dispatch_once_t predicate; \
		dispatch_once(&predicate, ^{ \
			original_dispatch_ ## function = (void(*)(dispatch_object_t))dlsym(RTLD_NEXT, "dispatch_" #function); \
			assert(original_dispatch_ ## function); \
		}); \
		original_dispatch_ ## function (object); \
		return; \
	}

void dispatch_retain(dispatch_object_t object)
{
	/* FIXME: assumes dispatch_object_t internals */
	dispatch_queue_t queue = object._dq;
	ORIGINAL_GCD(retain, object)
	
	pthread_mutex_lock(&queue->lock);
	assert(queue->refcount);
	queue->refcount++;
	pthread_mutex_unlock(&queue->lock);
}

void dispatch_release(dispatch_object_t object)
{
	/* FIXME: assumes dispatch_object_t internals */
	dispatch_queue_t queue = object._dq;
	ORIGINAL_GCD(release, object)
	
	pthread_mutex_lock(&queue->lock);
	assert(queue->refcount);
	if (--queue->refcount == 0) {
		dispatch_queue_element_t element = {
			.block = ^  __attribute__((noreturn)) {
				pthread_exit(NULL);
			}
		};
		buffer_put(&queue->blocks, element);
		pthread_mutex_unlock(&queue->lock);
		dispatch_semaphore_signal(queue->work);
		
		pthread_join(queue->worker, NULL);
		
		dispatch_release(queue->work);
		pthread_mutex_destroy(&queue->lock);
		free(queue->label);
		free(queue);
	} else
		pthread_mutex_unlock(&queue->lock);
}

#pragma mark -


#pragma mark ATLAS GCD Additions

/* FIXME: assumes block internals
 * copied from libclosure's/libBlocksRuntime's Block_private.h */
struct Block_layout {
	void *isa;
	int flags;
	int size;
	void (*invoke)(void *, ...);
};

void dispatch_async_atlas(dispatch_queue_t queue, atlas_job_t job, dispatch_block_t block)
{
	void *code = (void *)((struct Block_layout *)block)->invoke;
	dispatch_queue_element_t element = {
		.block = Block_copy(block),
		.is_copied = true,
		.is_realtime = true,
		.signal_completion = NULL
	};
	
	pthread_mutex_lock(&queue->lock);
	assert(queue->refcount);
	
	if (job.deadline < queue->previous_deadline)
		job.deadline = queue->previous_deadline;
	queue->previous_deadline = job.deadline;
	
	atlas_job_submit(code, queue->tid, job);
	buffer_put(&queue->blocks, element);
	queue->refcount++;  // shortcut for calling dispatch_retain
	
	pthread_mutex_unlock(&queue->lock);
	dispatch_semaphore_signal(queue->work);
}

static void *dispatch_queue_worker(void *context)
{
	dispatch_queue_t queue = (dispatch_queue_t)context;
	
	queue->tid = gettid();
	atlas_pin_cpu(0);
	pthread_setspecific(current_queue, queue);
	dispatch_semaphore_signal(queue->init);
	
	while (1) {
		dispatch_semaphore_wait(queue->work, DISPATCH_TIME_FOREVER);
		
		pthread_mutex_lock(&queue->lock);
		dispatch_queue_element_t element = buffer_get(&queue->blocks);
		pthread_mutex_unlock(&queue->lock);
		
		void *code = (void *)((struct Block_layout *)element.block)->invoke;
		if (element.is_realtime)
			atlas_job_next(code);
		
		element.block();
		
		if (element.is_copied)
			Block_release(element.block);
		dispatch_release(queue);
		if (element.signal_completion)
			dispatch_semaphore_signal(*element.signal_completion);
		if (element.is_realtime)
			atlas_job_train(code);
	}
}

#else

#include "dispatch.h"

void dispatch_async_atlas(dispatch_queue_t queue, atlas_job_t job, dispatch_block_t block)
{
	(void)job;
	dispatch_async(queue, block);
}

#endif
