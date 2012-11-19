/*
 * Copyright (C) 2006-2012 Michael Roitzsch <mroi@os.inf.tu-dresden.de>
 * economic rights: Technische Universitaet Dresden (Germany)
 */

#ifdef __linux__
#define _GNU_SOURCE
#include <sched.h>
#include <signal.h>
#endif

#include <stdint.h>
#include <stdbool.h>
#include <Block.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <dlfcn.h>
#include <assert.h>

typedef struct {
	void (^block)(void);
	bool is_copied;
	bool is_realtime;
} dispatch_queue_element_t;

#define BUFFER_TYPE dispatch_queue_element_t
#include "dispatch.h"
#include "estimator.h"

#if DISPATCH_ATLAS

struct dispatch_queue_s {
	uint32_t magic;
	pthread_mutex_t lock;
	uint_fast32_t refcount;
	char *label;
	
	pid_t tid;
	pthread_t worker;
	dispatch_semaphore_t init;
	
	dispatch_semaphore_t work;
	struct buffer blocks;
};

static inline void dispatch_queue_enqueue(dispatch_queue_t queue, dispatch_queue_element_t element)
{
	pthread_mutex_lock(&queue->lock);
	buffer_put(&queue->blocks, element);
	queue->refcount++;  // shortcut for calling dispatch_retain
	pthread_mutex_unlock(&queue->lock);
	dispatch_semaphore_signal(queue->work);
}

static void *dispatch_queue_worker(void *);

#pragma mark -


#pragma mark Original GCD API

dispatch_queue_t dispatch_queue_create(const char *label, dispatch_queue_attr_t attr)
{
	assert(attr == DISPATCH_QUEUE_SERIAL);
	dispatch_queue_t queue = malloc(sizeof(struct dispatch_queue_s));
	if (!queue) return NULL;
	
	queue->magic = 'atls';
	queue->refcount = 1;
	queue->label = strdup(label);
	queue->init = dispatch_semaphore_create(0);
	queue->work = dispatch_semaphore_create(0);
	
	pthread_mutex_init(&queue->lock, NULL);
	pthread_create(&queue->worker, NULL, dispatch_queue_worker, queue);
	dispatch_semaphore_wait(queue->init, DISPATCH_TIME_FOREVER);
	dispatch_release(queue->init);
	
	buffer_init(&queue->blocks);
	
	return queue;
}

void dispatch_async(dispatch_queue_t queue, dispatch_block_t block)
{
	dispatch_queue_element_t element = {
		.block = Block_copy(block),
		.is_copied = true,
		.is_realtime = false
	};
	dispatch_queue_enqueue(queue, element);
}

void dispatch_sync(dispatch_queue_t queue, dispatch_block_t block)
{
	dispatch_semaphore_t complete = dispatch_semaphore_create(0);
	
	dispatch_queue_element_t element = {
		.block = ^{
			block();
			dispatch_semaphore_signal(complete);
		},
		.is_copied = false,
		.is_realtime = false
	};
	dispatch_queue_enqueue(queue, element);
	
	dispatch_semaphore_wait(complete, DISPATCH_TIME_FOREVER);
	dispatch_release(complete);
}

#define ORIGINAL_GCD(function, object) \
	if (queue->magic != 'atls') { \
		static void (*original_dispatch_ ## function)(dispatch_object_t); \
		static dispatch_once_t predicate; \
		dispatch_once(&predicate, ^{ \
			original_dispatch_ ## function = dlsym(RTLD_NEXT, "dispatch_" #function); \
			assert(original_dispatch_ ## function); \
		}); \
		return original_dispatch_ ## function (object); \
	}

void dispatch_retain(dispatch_object_t object)
{
	/* FIXME: assumes dispatch_object_t internals */
	dispatch_queue_t queue = object._dq;
	ORIGINAL_GCD(retain, object)
	
	pthread_mutex_lock(&queue->lock);
	queue->refcount++;
	pthread_mutex_unlock(&queue->lock);
}

void dispatch_release(dispatch_object_t object)
{
	/* FIXME: assumes dispatch_object_t internals */
	dispatch_queue_t queue = object._dq;
	ORIGINAL_GCD(release, object)
	
	pthread_mutex_lock(&queue->lock);
	if (--queue->refcount == 0) {
		dispatch_queue_element_t element = {
			.block = ^{
				pthread_exit(NULL);
			},
			.is_copied = false
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
	void *code = ((struct Block_layout *)block)->invoke;
	atlas_job_submit(code, queue->tid, job);
	
	dispatch_queue_element_t element = {
		.block = Block_copy(block),
		.is_copied = true,
		.is_realtime = true
	};
	dispatch_queue_enqueue(queue, element);
}

static void *dispatch_queue_worker(void *context)
{
	dispatch_queue_t queue = (dispatch_queue_t)context;
	
#ifdef __linux__
	/* pin to CPU 0 */
	cpu_set_t cpu_set;
	CPU_ZERO(&cpu_set);
	CPU_SET(0, &cpu_set);
	if (sched_setaffinity(0, sizeof(cpu_set), &cpu_set) != 0) abort();
	
	const struct sigaction action = { .sa_handler = SIG_IGN };
	if (sigaction(SIGXCPU, &action, NULL) != 0) abort();
#endif
	
	queue->tid = gettid();
	dispatch_semaphore_signal(queue->init);
	
	while (1) {
		dispatch_semaphore_wait(queue->work, DISPATCH_TIME_FOREVER);
		
		pthread_mutex_lock(&queue->lock);
		dispatch_queue_element_t element = buffer_get(&queue->blocks);
		pthread_mutex_unlock(&queue->lock);
		
		void *code = ((struct Block_layout *)element.block)->invoke;
		if (element.is_realtime)
			atlas_job_next(code);
		
		element.block();
		
		if (element.is_copied)
			Block_release(element.block);
		if (element.is_realtime)
			atlas_job_train(code);
		
		dispatch_release(queue);
	}
}

#else

void dispatch_async_atlas(dispatch_queue_t queue, atlas_job_t job, dispatch_block_t block)
{
	dispatch_async(queue, block);
}

#endif
