/*
 * libwebsockets - small server side websockets and web server implementation
 *
 * Copyright (C) 2010 - 2020 Andy Green <andy@warmcat.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#if !defined(_GNU_SOURCE)
#define _GNU_SOURCE
#endif

#if defined(WIN32)
#define HAVE_STRUCT_TIMESPEC
#if defined(pid_t)
#undef pid_t
#endif
#endif
#include <pthread.h>

#include "private-lib-core.h"

#include <string.h>
#include <stdio.h>

struct lws_threadpool;

struct lws_threadpool_task {
	struct lws_threadpool_task	*task_queue_next;

	struct lws_threadpool		*tp;
	char				name[32];
	struct lws_threadpool_task_args args;

	lws_dll2_t			list;

	lws_usec_t			created;
	lws_usec_t			acquired;
	lws_usec_t			done;
	lws_usec_t			entered_state;

	lws_usec_t			acc_running;
	lws_usec_t			acc_syncing;

	pthread_cond_t			wake_idle;

	enum lws_threadpool_task_status status;

	int				late_sync_retries;

	char				wanted_writeable_cb;
	char				outlive;
};

struct lws_pool {
	struct lws_threadpool		*tp;
	pthread_t			thread;
	pthread_mutex_t			lock; /* part of task wake_idle */
	struct lws_threadpool_task	*task;
	lws_usec_t			acquired;
	int				worker_index;
};

struct lws_threadpool {
	pthread_mutex_t			lock; /* protects all pool lists */
	pthread_cond_t			wake_idle;
	struct lws_pool			*pool_list;

	struct lws_context		*context;
	struct lws_threadpool		*tp_list; /* context list of threadpools */

	struct lws_threadpool_task	*task_queue_head;
	struct lws_threadpool_task	*task_done_head;

	char				name[32];

	int				threads_in_pool;
	int				queue_depth;
	int				done_queue_depth;
	int				max_queue_depth;
	int				running_tasks;

	unsigned int			destroying:1;
};

static int
ms_delta(lws_usec_t now, lws_usec_t then)
{
	return (int)((now - then) / 1000);
}

static void
us_accrue(lws_usec_t *acc, lws_usec_t then)
{
	lws_usec_t now = lws_now_usecs();

	*acc += now - then;
}

static int
pc_delta(lws_usec_t now, lws_usec_t then, lws_usec_t us)
{
	lws_usec_t delta = (now - then) + 1;

	return (int)((us * 100) / delta);
}

static void
__lws_threadpool_task_dump(struct lws_threadpool_task *task, char *buf, int len)
{
	lws_usec_t now = lws_now_usecs();
	char *end = buf + len - 1;
	int syncms = 0, runms = 0;

	if (!task->acquired) {
		buf += lws_snprintf(buf, lws_ptr_diff_size_t(end, buf),
				    "task: %s, QUEUED queued: %dms",
				    task->name, ms_delta(now, task->created));

		return;
	}

	if (task->acc_running)
		runms = (int)task->acc_running;

	if (task->acc_syncing)
		syncms = (int)task->acc_syncing;

	if (!task->done) {
		buf += lws_snprintf(buf, lws_ptr_diff_size_t(end, buf),
			"task: %s, ONGOING state %d (%dms) alive: %dms "
			"(queued %dms, acquired: %dms, "
			"run: %d%%, sync: %d%%)", task->name, task->status,
			ms_delta(now, task->entered_state),
			ms_delta(now, task->created),
			ms_delta(task->acquired, task->created),
			ms_delta(now, task->acquired),
			pc_delta(now, task->acquired, runms),
			pc_delta(now, task->acquired, syncms));

		return;
	}

	lws_snprintf(buf, lws_ptr_diff_size_t(end, buf),
		"task: %s, DONE state %d lived: %dms "
		"(queued %dms, on thread: %dms, "
		"ran: %d%%, synced: %d%%)", task->name, task->status,
		ms_delta(task->done, task->created),
		ms_delta(task->acquired, task->created),
		ms_delta(task->done, task->acquired),
		pc_delta(task->done, task->acquired, runms),
		pc_delta(task->done, task->acquired, syncms));
}

void
lws_threadpool_dump(struct lws_threadpool *tp)
{
#if 0
	//defined(_DEBUG)
	struct lws_threadpool_task **c;
	char buf[160];
	int n, count;

	pthread_mutex_lock(&tp->lock); /* ======================== tpool lock */

	lwsl_thread("%s: tp: %s, Queued: %d, Run: %d, Done: %d\n", __func__,
		    tp->name, tp->queue_depth, tp->running_tasks,
		    tp->done_queue_depth);

	count = 0;
	c = &tp->task_queue_head;
	while (*c) {
		struct lws_threadpool_task *task = *c;
		__lws_threadpool_task_dump(task, buf, sizeof(buf));
		lwsl_thread("  - %s\n", buf);
		count++;

		c = &(*c)->task_queue_next;
	}

	if (count != tp->queue_depth)
		lwsl_err("%s: tp says queue depth %d, but actually %d\n",
			 __func__, tp->queue_depth, count);

	count = 0;
	for (n = 0; n < tp->threads_in_pool; n++) {
		struct lws_pool *pool = &tp->pool_list[n];
		struct lws_threadpool_task *task = pool->task;

		if (task) {
			__lws_threadpool_task_dump(task, buf, sizeof(buf));
			lwsl_thread("  - worker %d: %s\n", n, buf);
			count++;
		}
	}

	if (count != tp->running_tasks)
		lwsl_err("%s: tp says %d running_tasks, but actually %d\n",
			 __func__, tp->running_tasks, count);

	count = 0;
	c = &tp->task_done_head;
	while (*c) {
		struct lws_threadpool_task *task = *c;
		__lws_threadpool_task_dump(task, buf, sizeof(buf));
		lwsl_thread("  - %s\n", buf);
		count++;

		c = &(*c)->task_queue_next;
	}

	if (count != tp->done_queue_depth)
		lwsl_err("%s: tp says done_queue_depth %d, but actually %d\n",
			 __func__, tp->done_queue_depth, count);

	pthread_mutex_unlock(&tp->lock); /* --------------- tp unlock */
#endif
}

static void
state_transition(struct lws_threadpool_task *task,
		 enum lws_threadpool_task_status status)
{
	task->entered_state = lws_now_usecs();
	task->status = status;
}

static struct lws *
task_to_wsi(struct lws_threadpool_task *task)
{
#if defined(LWS_WITH_SECURE_STREAMS)
	if (task->args.ss)
		return task->args.ss->wsi;
#endif
	return task->args.wsi;
}

static void
lws_threadpool_task_cleanup_destroy(struct lws_threadpool_task *task)
{
	if (task->args.cleanup)
		task->args.cleanup(task_to_wsi(task), task->args.user);

	lws_dll2_remove(&task->list);

	lwsl_thread("%s: tp %p: cleaned finished task for %s\n",
		    __func__, task->tp, lws_wsi_tag(task_to_wsi(task)));

	lws_free(task);
}

static void
__lws_threadpool_reap(struct lws_threadpool_task *task)
{
	struct lws_threadpool_task **c, *t = NULL;
	struct lws_threadpool *tp = task->tp;

	/* remove the task from the done queue */

	if (tp) {
		c = &tp->task_done_head;

		while (*c) {
			if ((*c) == task) {
				t = *c;
				*c = t->task_queue_next;
				t->task_queue_next = NULL;
				tp->done_queue_depth--;

				lwsl_thread("%s: tp %s: reaped task %s\n", __func__,
					   tp->name, lws_wsi_tag(task_to_wsi(task)));

				break;
			}
			c = &(*c)->task_queue_next;
		}

		if (!t) {
			lwsl_err("%s: task %p not in done queue\n", __func__, task);
			/*
			 * This shouldn't occur, but in this case not really
			 * safe to assume there's a task to destroy
			 */
			return;
		}
	} else
		lwsl_err("%s: task->tp NULL already\n", __func__);

	/* call the task's cleanup and delete the task itself */

	lws_threadpool_task_cleanup_destroy(task);
}

/*
 * this gets called from each tsi service context after the service was
 * cancelled... we need to ask for the writable callback from the matching
 * tsi context for any wsis bound to a worked thread that need it
 */

int
lws_threadpool_tsi_context(struct lws_context *context, int tsi)
{
	struct lws_threadpool_task **c, *task = NULL;
	struct lws_threadpool *tp;
	struct lws *wsi;

	lws_context_lock(context, __func__);

	tp = context->tp_list_head;
	while (tp) {
		int n;

		/* for the running (syncing...) tasks... */

		for (n = 0; n < tp->threads_in_pool; n++) {
			struct lws_pool *pool = &tp->pool_list[n];

			task = pool->task;
			if (!task)
				continue;

			wsi = task_to_wsi(task);
			if (!wsi || wsi->tsi != tsi ||
			    (!task->wanted_writeable_cb &&
			     task->status != LWS_TP_STATUS_SYNCING))
				continue;

			task->wanted_writeable_cb = 0;
			lws_memory_barrier();

			/*
			 * finally... we can ask for the callback on
			 * writable from the correct service thread
			 * context
			 */

			lws_callback_on_writable(wsi);
		}

		/* for the done tasks... */

		c = &tp->task_done_head;

		while (*c) {
			task = *c;
			wsi = task_to_wsi(task);

			if (wsi && wsi->tsi == tsi &&
			    (task->wanted_writeable_cb ||
			     task->status == LWS_TP_STATUS_SYNCING)) {

				task->wanted_writeable_cb = 0;
				lws_memory_barrier();

				/*
				 * finally... we can ask for the callback on
				 * writable from the correct service thread
				 * context
				 */

				lws_callback_on_writable(wsi);
			}

			c = &task->task_queue_next;
		}

		tp = tp->tp_list;
	}

	lws_context_unlock(context);

	return 0;
}

static int
lws_threadpool_worker_sync(struct lws_pool *pool,
			   struct lws_threadpool_task *task)
{
	enum lws_threadpool_task_status temp;
	struct timespec abstime;
	struct lws *wsi;
	int tries = 15;

	/* block until writable acknowledges */
	lwsl_debug("%s: %p: LWS_TP_RETURN_SYNC in\n", __func__, task);
	pthread_mutex_lock(&pool->lock); /* ======================= pool lock */

	lwsl_info("%s: %s: task %p (%s): syncing with %s\n", __func__,
		    pool->tp->name, task, task->name, lws_wsi_tag(task_to_wsi(task)));

	temp = task->status;
	state_transition(task, LWS_TP_STATUS_SYNCING);
	while (tries--) {
		wsi = task_to_wsi(task);

		/*
		 * if the wsi is no longer attached to this task, there is
		 * nothing we can sync to usefully.  Since the work wants to
		 * sync, it means we should react to the situation by telling
		 * the task it can't continue usefully by stopping it.
		 */

		if (!wsi) {
			lwsl_thread("%s: %s: task %p (%s): No longer bound to any "
				 "wsi to sync to\n", __func__, pool->tp->name,
				 task, task->name);

			state_transition(task, LWS_TP_STATUS_STOPPING);
			goto done;
		}

		/*
		 * So "tries" times this is the maximum time between SYNC asking
		 * for a callback on writable and actually getting it we are
		 * willing to sit still for.
		 *
		 * If it is exceeded, we will stop the task.
		 */
		abstime.tv_sec = time(NULL) + 3;
		abstime.tv_nsec = 0;

		task->wanted_writeable_cb = 1;
		lws_memory_barrier();

		/*
		 * This will cause lws_threadpool_tsi_context() to get called
		 * from each tsi service context, where we can safely ask for
		 * a callback on writeable on the wsi we are associated with.
		 */
		lws_cancel_service(lws_get_context(wsi));

		/*
		 * so the danger here is that we asked for a writable callback
		 * on the wsi, but for whatever reason, we are never going to
		 * get one.  To avoid deadlocking forever, we allow a set time
		 * for the sync to happen naturally, otherwise the cond wait
		 * times out and we stop the task.
		 */

		if (pthread_cond_timedwait(&task->wake_idle, &pool->lock,
					   &abstime) == ETIMEDOUT) {
			task->late_sync_retries++;
			if (!tries) {
				lwsl_err("%s: %s: task %p (%s): SYNC timed out "
					 "(associated %s)\n",
					 __func__, pool->tp->name, task,
					 task->name, lws_wsi_tag(task_to_wsi(task)));

				pthread_mutex_unlock(&pool->lock); /* ----------------- - pool unlock */
				lws_threadpool_dequeue_task(task);
				return 1; /* destroyed task */
			}

			continue;
		} else
			break;
	}

	if (task->status == LWS_TP_STATUS_SYNCING)
		state_transition(task, temp);

	lwsl_debug("%s: %p: LWS_TP_RETURN_SYNC out\n", __func__, task);

done:
	pthread_mutex_unlock(&pool->lock); /* ----------------- - pool unlock */

	return 0;
}

#if !defined(WIN32)
static int dummy;
#endif

static void *
lws_threadpool_worker(void *d)
{
	struct lws_threadpool_task **c, **c2, *task;
	struct lws_pool *pool = d;
	struct lws_threadpool *tp = pool->tp;
	char buf[160];

	while (!tp->destroying) {

		/* we have no running task... wait and get one from the queue */

		pthread_mutex_lock(&tp->lock); /* =================== tp lock */

		/*
		 * if there's no task already waiting in the queue, wait for
		 * the wake_idle condition to signal us that might have changed
		 */
		while (!tp->task_queue_head && !tp->destroying)
			pthread_cond_wait(&tp->wake_idle, &tp->lock);

		if (tp->destroying) {
			lwsl_notice("%s: bailing\n", __func__);
			goto doneski;
		}

		c = &tp->task_queue_head;
		c2 = NULL;
		task = NULL;
		pool->task = NULL;

		/* look at the queue tail */
		while (*c) {
			c2 = c;
			c = &(*c)->task_queue_next;
		}

		/* is there a task at the queue tail? */
		if (c2 && *c2) {
			pool->task = task = *c2;
			task->acquired = pool->acquired = lws_now_usecs();
			/* remove it from the queue */
			*c2 = task->task_queue_next;
			task->task_queue_next = NULL;
			tp->queue_depth--;
			/* mark it as running */
			state_transition(task, LWS_TP_STATUS_RUNNING);
		}

		/* someone else got it first... wait and try again */
		if (!task) {
			pthread_mutex_unlock(&tp->lock);  /* ------ tp unlock */
			continue;
		}

		task->wanted_writeable_cb = 0;

		/* we have acquired a new task */

		__lws_threadpool_task_dump(task, buf, sizeof(buf));

		lwsl_thread("%s: %s: worker %d ACQUIRING: %s\n",
			    __func__, tp->name, pool->worker_index, buf);
		tp->running_tasks++;

		pthread_mutex_unlock(&tp->lock); /* --------------- tp unlock */

		/*
		 * 1) The task can return with LWS_TP_RETURN_CHECKING_IN to
		 * "resurface" periodically, and get called again with
		 * cont = 1 immediately to indicate it is picking up where it
		 * left off if the task is not being "stopped".
		 *
		 * This allows long tasks to respond to requests to stop in
		 * a clean and opaque way.
		 *
		 * 2) The task can return with LWS_TP_RETURN_SYNC to register
		 * a "callback on writable" request on the service thread and
		 * block until it hears back from the WRITABLE handler.
		 *
		 * This allows the work on the thread to be synchronized to the
		 * previous work being dispatched cleanly.
		 *
		 * 3) The task can return with LWS_TP_RETURN_FINISHED to
		 * indicate its work is completed nicely.
		 *
		 * 4) The task can return with LWS_TP_RETURN_STOPPED to indicate
		 * it stopped and cleaned up after incomplete work.
		 */

		do {
			lws_usec_t then;
			int n;

			if (tp->destroying || !task_to_wsi(task)) {
				lwsl_info("%s: stopping on wsi gone\n", __func__);
				state_transition(task, LWS_TP_STATUS_STOPPING);
			}

			then = lws_now_usecs();
			n = (int)task->args.task(task->args.user, task->status);
			lwsl_debug("   %d, status %d\n", n, task->status);
			us_accrue(&task->acc_running, then);
			if (n & LWS_TP_RETURN_FLAG_OUTLIVE)
				task->outlive = 1;
			switch (n & 7) {
			case LWS_TP_RETURN_CHECKING_IN:
				/* if not destroying the tp, continue */
				break;
			case LWS_TP_RETURN_SYNC:
				if (!task_to_wsi(task)) {
					lwsl_debug("%s: task that wants to "
						    "outlive lost wsi asked "
						    "to sync: bypassed\n",
						    __func__);
					break;
				}
				/* block until writable acknowledges */
				then = lws_now_usecs();
				if (lws_threadpool_worker_sync(pool, task)) {
					lwsl_notice("%s: Sync failed\n", __func__);
					goto doneski;
				}
				us_accrue(&task->acc_syncing, then);
				break;
			case LWS_TP_RETURN_FINISHED:
				state_transition(task, LWS_TP_STATUS_FINISHED);
				break;
			case LWS_TP_RETURN_STOPPED:
				state_transition(task, LWS_TP_STATUS_STOPPED);
				break;
			}
		} while (task->status == LWS_TP_STATUS_RUNNING);

		pthread_mutex_lock(&tp->lock); /* =================== tp lock */

		tp->running_tasks--;

		if (pool->task->status == LWS_TP_STATUS_STOPPING)
			state_transition(task, LWS_TP_STATUS_STOPPED);

		/* move the task to the done queue */

		pool->task->task_queue_next = tp->task_done_head;
		tp->task_done_head = task;
		tp->done_queue_depth++;
		pool->task->done = lws_now_usecs();

		if (!pool->task->args.wsi &&
		    (pool->task->status == LWS_TP_STATUS_STOPPED ||
		     pool->task->status == LWS_TP_STATUS_FINISHED)) {

			__lws_threadpool_task_dump(pool->task, buf, sizeof(buf));
			lwsl_thread("%s: %s: worker %d REAPING: %s\n",
				    __func__, tp->name, pool->worker_index,
				    buf);

			/*
			 * there is no longer any wsi attached, so nothing is
			 * going to take care of reaping us.  So we must take
			 * care of it ourselves.
			 */
			__lws_threadpool_reap(pool->task);
		} else {

			__lws_threadpool_task_dump(pool->task, buf, sizeof(buf));
			lwsl_thread("%s: %s: worker %d DONE: %s\n",
				    __func__, tp->name, pool->worker_index,
				    buf);

			/* signal the associated wsi to take a fresh look at
			 * task status */

			if (task_to_wsi(pool->task)) {
				task->wanted_writeable_cb = 1;

				lws_cancel_service(
					lws_get_context(task_to_wsi(pool->task)));
			}
		}

doneski:
		pool->task = NULL;
		pthread_mutex_unlock(&tp->lock); /* --------------- tp unlock */
	}

	lwsl_notice("%s: Exiting\n", __func__);

	/* threadpool is being destroyed */
#if !defined(WIN32)
	pthread_exit(&dummy);
#endif

	return NULL;
}

struct lws_threadpool *
lws_threadpool_create(struct lws_context *context,
		      const struct lws_threadpool_create_args *args,
		      const char *format, ...)
{
	struct lws_threadpool *tp;
	va_list ap;
	int n;

	tp = lws_malloc(sizeof(*tp) + (sizeof(struct lws_pool) * (unsigned int)args->threads),
			"threadpool alloc");
	if (!tp)
		return NULL;

	memset(tp, 0, sizeof(*tp) + (sizeof(struct lws_pool) * (unsigned int)args->threads));
	tp->pool_list = (struct lws_pool *)(tp + 1);
	tp->max_queue_depth = args->max_queue_depth;

	va_start(ap, format);
	n = vsnprintf(tp->name, sizeof(tp->name) - 1, format, ap);
	va_end(ap);

	lws_context_lock(context, __func__);

	tp->context = context;
	tp->tp_list = context->tp_list_head;
	context->tp_list_head = tp;

	lws_context_unlock(context);

	pthread_mutex_init(&tp->lock, NULL);
	pthread_cond_init(&tp->wake_idle, NULL);

	for (n = 0; n < args->threads; n++) {
#if defined(LWS_HAS_PTHREAD_SETNAME_NP)
		char name[16];
#endif
		tp->pool_list[n].tp = tp;
		tp->pool_list[n].worker_index = n;
		pthread_mutex_init(&tp->pool_list[n].lock, NULL);
		if (pthread_create(&tp->pool_list[n].thread, NULL,
				   lws_threadpool_worker, &tp->pool_list[n])) {
			lwsl_err("thread creation failed\n");
		} else {
#if defined(LWS_HAS_PTHREAD_SETNAME_NP)
			lws_snprintf(name, sizeof(name), "%s-%d", tp->name, n);
			pthread_setname_np(tp->pool_list[n].thread, name);
#endif
			tp->threads_in_pool++;
		}
	}

	return tp;
}

void
lws_threadpool_finish(struct lws_threadpool *tp)
{
	struct lws_threadpool_task **c, *task;

	pthread_mutex_lock(&tp->lock); /* ======================== tpool lock */

	/* nothing new can start, running jobs will abort as STOPPED and the
	 * pool threads will exit ASAP (they are joined in destroy) */
	tp->destroying = 1;

	/* stop everyone in the pending queue and move to the done queue */

	c = &tp->task_queue_head;
	while (*c) {
		task = *c;
		*c = task->task_queue_next;
		task->task_queue_next = tp->task_done_head;
		tp->task_done_head = task;
		state_transition(task, LWS_TP_STATUS_STOPPED);
		tp->queue_depth--;
		tp->done_queue_depth++;
		task->done = lws_now_usecs();

		c = &task->task_queue_next;
	}

	pthread_cond_broadcast(&tp->wake_idle);
	pthread_mutex_unlock(&tp->lock); /* -------------------- tpool unlock */
}

void
lws_threadpool_destroy(struct lws_threadpool *tp)
{
	struct lws_threadpool_task *task, *next;
	struct lws_threadpool **ptp;
	void *retval;
	int n;

	/* remove us from the context list of threadpools */

	lws_context_lock(tp->context, __func__);
	ptp = &tp->context->tp_list_head;

	while (*ptp) {
		if (*ptp == tp) {
			*ptp = tp->tp_list;
			break;
		}
		ptp = &(*ptp)->tp_list;
	}

	lws_context_unlock(tp->context);

	/*
	 * Wake up the threadpool guys and tell them to exit
	 */

	pthread_mutex_lock(&tp->lock); /* ======================== tpool lock */
	tp->destroying = 1;
	pthread_cond_broadcast(&tp->wake_idle);
	pthread_mutex_unlock(&tp->lock); /* -------------------- tpool unlock */

	lws_threadpool_dump(tp);

	lwsl_info("%s: waiting for threads to rejoin\n", __func__);
#if defined(WIN32)
	Sleep(1000);
#endif

	for (n = 0; n < tp->threads_in_pool; n++) {
		task = tp->pool_list[n].task;

		pthread_join(tp->pool_list[n].thread, &retval);
		pthread_mutex_destroy(&tp->pool_list[n].lock);
	}
	lwsl_info("%s: all threadpools exited\n", __func__);
#if defined(WIN32)
	Sleep(1000);
#endif

	task = tp->task_done_head;
	while (task) {
		next = task->task_queue_next;
		lws_threadpool_task_cleanup_destroy(task);
		tp->done_queue_depth--;
		task = next;
	}

	pthread_mutex_destroy(&tp->lock);

	memset(tp, 0xdd, sizeof(*tp));
	lws_free(tp);
}

/*
 * We want to stop and destroy the tasks and related priv.
 */

int
lws_threadpool_dequeue_task(struct lws_threadpool_task *task)
{
	struct lws_threadpool *tp;
	struct lws_threadpool_task **c;
	int n;

	tp = task->tp;
	pthread_mutex_lock(&tp->lock); /* ======================== tpool lock */

	if (task->outlive && !tp->destroying) {

		/* disconnect from wsi, and wsi from task */

		lws_dll2_remove(&task->list);
		task->args.wsi = NULL;
#if defined(LWS_WITH_SECURE_STREAMS)
		task->args.ss = NULL;
#endif

		goto bail;
	}


	c = &tp->task_queue_head;

	/* is he queued waiting for a chance to run?  Mark him as stopped and
	 * move him on to the done queue */

	while (*c) {
		if ((*c) == task) {
			*c = task->task_queue_next;
			task->task_queue_next = tp->task_done_head;
			tp->task_done_head = task;
			state_transition(task, LWS_TP_STATUS_STOPPED);
			tp->queue_depth--;
			tp->done_queue_depth++;
			task->done = lws_now_usecs();

			lwsl_debug("%s: tp %p: removed queued task %s\n",
				    __func__, tp, lws_wsi_tag(task_to_wsi(task)));

			break;
		}
		c = &(*c)->task_queue_next;
	}

	/* is he on the done queue? */

	c = &tp->task_done_head;
	while (*c) {
		if ((*c) == task) {
			*c = task->task_queue_next;
			task->task_queue_next = NULL;
			lws_threadpool_task_cleanup_destroy(task);
			tp->done_queue_depth--;
			goto bail;
		}
		c = &(*c)->task_queue_next;
	}

	/* he's not in the queue... is he already running on a thread? */

	for (n = 0; n < tp->threads_in_pool; n++) {
		if (!tp->pool_list[n].task || tp->pool_list[n].task != task)
			continue;

		/*
		 * ensure we don't collide with tests or changes in the
		 * worker thread
		 */
		pthread_mutex_lock(&tp->pool_list[n].lock);

		/*
		 * mark him as having been requested to stop...
		 * the caller will hear about it in his service thread
		 * context as a request to close
		 */
		state_transition(task, LWS_TP_STATUS_STOPPING);

		/* disconnect from wsi, and wsi from task */

		lws_dll2_remove(&task->list);
		task->args.wsi = NULL;
#if defined(LWS_WITH_SECURE_STREAMS)
		task->args.ss = NULL;
#endif

		pthread_mutex_unlock(&tp->pool_list[n].lock);

		lwsl_debug("%s: tp %p: request stop running task "
			    "for %s\n", __func__, tp,
			    lws_wsi_tag(task_to_wsi(task)));

		break;
	}

	if (n == tp->threads_in_pool) {
		/* can't find it */
		lwsl_notice("%s: tp %p: no task for %s, decoupling\n",
			    __func__, tp, lws_wsi_tag(task_to_wsi(task)));
		lws_dll2_remove(&task->list);
		task->args.wsi = NULL;
#if defined(LWS_WITH_SECURE_STREAMS)
		task->args.ss = NULL;
#endif
	}

bail:
	pthread_mutex_unlock(&tp->lock); /* -------------------- tpool unlock */

	return 0;
}

int
lws_threadpool_dequeue(struct lws *wsi) /* deprecated */
{
	struct lws_threadpool_task *task;

	if (!wsi->tp_task_owner.count)
		return 0;
	assert(wsi->tp_task_owner.count != 1);

	task = lws_container_of(wsi->tp_task_owner.head,
				struct lws_threadpool_task, list);

	return lws_threadpool_dequeue_task(task);
}

struct lws_threadpool_task *
lws_threadpool_enqueue(struct lws_threadpool *tp,
		       const struct lws_threadpool_task_args *args,
		       const char *format, ...)
{
	struct lws_threadpool_task *task = NULL;
	va_list ap;

	if (tp->destroying)
		return NULL;

#if defined(LWS_WITH_SECURE_STREAMS)
	assert(args->ss || args->wsi);
#endif

	pthread_mutex_lock(&tp->lock); /* ======================== tpool lock */

	/*
	 * if there's room on the queue, the job always goes on the queue
	 * first, then any free thread may pick it up after the wake_idle
	 */

	if (tp->queue_depth == tp->max_queue_depth) {
		lwsl_notice("%s: queue reached limit %d\n", __func__,
			    tp->max_queue_depth);

		goto bail;
	}

	/*
	 * create the task object
	 */

	task = lws_malloc(sizeof(*task), __func__);
	if (!task)
		goto bail;

	memset(task, 0, sizeof(*task));
	pthread_cond_init(&task->wake_idle, NULL);
	task->args = *args;
	task->tp = tp;
	task->created = lws_now_usecs();

	va_start(ap, format);
	vsnprintf(task->name, sizeof(task->name) - 1, format, ap);
	va_end(ap);

	/*
	 * add him on the tp task queue
	 */

	task->task_queue_next = tp->task_queue_head;
	state_transition(task, LWS_TP_STATUS_QUEUED);
	tp->task_queue_head = task;
	tp->queue_depth++;

	/*
	 * mark the wsi itself as depending on this tp (so wsi close for
	 * whatever reason can clean up)
	 */

#if defined(LWS_WITH_SECURE_STREAMS)
	if (args->ss)
		lws_dll2_add_tail(&task->list, &args->ss->wsi->tp_task_owner);
	else
#endif
		lws_dll2_add_tail(&task->list, &args->wsi->tp_task_owner);

	lwsl_thread("%s: tp %s: enqueued task %p (%s) for %s, depth %d\n",
		    __func__, tp->name, task, task->name,
		    lws_wsi_tag(task_to_wsi(task)), tp->queue_depth);

	/* alert any idle thread there's something new on the task list */

	lws_memory_barrier();
	pthread_cond_signal(&tp->wake_idle);

bail:
	pthread_mutex_unlock(&tp->lock); /* -------------------- tpool unlock */

	return task;
}

/* this should be called from the service thread */

enum lws_threadpool_task_status
lws_threadpool_task_status(struct lws_threadpool_task *task, void **user)
{
	enum lws_threadpool_task_status status;
	struct lws_threadpool *tp = task->tp;

	if (!tp)
		return LWS_TP_STATUS_FINISHED;

	*user = task->args.user;
	status = task->status;

	if (status == LWS_TP_STATUS_FINISHED ||
	    status == LWS_TP_STATUS_STOPPED) {
		char buf[160];

		pthread_mutex_lock(&tp->lock); /* ================ tpool lock */
		__lws_threadpool_task_dump(task, buf, sizeof(buf));
		lwsl_thread("%s: %s: service thread REAPING: %s\n",
			    __func__, tp->name, buf);
		__lws_threadpool_reap(task);
		lws_memory_barrier();
		pthread_mutex_unlock(&tp->lock); /* ------------ tpool unlock */
	}

	return status;
}

enum lws_threadpool_task_status
lws_threadpool_task_status_noreap(struct lws_threadpool_task *task)
{
	return task->status;
}

enum lws_threadpool_task_status
lws_threadpool_task_status_wsi(struct lws *wsi,
			       struct lws_threadpool_task **_task, void **user)
{
	struct lws_threadpool_task *task;

	if (!wsi->tp_task_owner.count) {
		lwsl_notice("%s: wsi has no task, ~=FINISHED\n", __func__);
		return LWS_TP_STATUS_FINISHED;
	}

	assert(wsi->tp_task_owner.count == 1); /* see deprecation docs in hdr */

	task = lws_container_of(wsi->tp_task_owner.head,
				struct lws_threadpool_task, list);

	*_task = task;

	return lws_threadpool_task_status(task, user);
}

void
lws_threadpool_task_sync(struct lws_threadpool_task *task, int stop)
{
	lwsl_debug("%s\n", __func__);
	if (!task)
		return;

	if (stop)
		state_transition(task, LWS_TP_STATUS_STOPPING);

	pthread_mutex_lock(&task->tp->lock);
	pthread_cond_signal(&task->wake_idle);
	pthread_mutex_unlock(&task->tp->lock);
}

int
lws_threadpool_foreach_task_wsi(struct lws *wsi, void *user,
				int (*cb)(struct lws_threadpool_task *task,
					  void *user))
{
	struct lws_threadpool_task *task1;

	if (wsi->tp_task_owner.head == NULL)
		return 0;

	task1 = lws_container_of(wsi->tp_task_owner.head,
				 struct lws_threadpool_task, list);

	pthread_mutex_lock(&task1->tp->lock); /* ================ tpool lock */

	lws_start_foreach_dll_safe(struct lws_dll2 *, d, d1,
				   wsi->tp_task_owner.head) {
		struct lws_threadpool_task *task = lws_container_of(d,
					struct lws_threadpool_task, list);

		if (cb(task, user)) {
			pthread_mutex_unlock(&task1->tp->lock); /* ------------ tpool unlock */
			return 1;
		}

	} lws_end_foreach_dll_safe(d, d1);

	pthread_mutex_unlock(&task1->tp->lock); /* ------------ tpool unlock */

	return 0;
}

#if defined(LWS_WITH_SECURE_STREAMS)
int
lws_threadpool_foreach_task_ss(struct lws_ss_handle *ss, void *user,
			       int (*cb)(struct lws_threadpool_task *task,
					 void *user))
{
	if (!ss->wsi)
		return 0;

	return lws_threadpool_foreach_task_wsi(ss->wsi, user, cb);
}
#endif

static int
disassociate_wsi(struct lws_threadpool_task *task,
		  void *user)
{
	task->args.wsi = NULL;
	lws_dll2_remove(&task->list);

	return 0;
}

void
lws_threadpool_wsi_closing(struct lws *wsi)
{
	lws_threadpool_foreach_task_wsi(wsi, NULL, disassociate_wsi);
}

struct lws_threadpool_task *
lws_threadpool_get_task_wsi(struct lws *wsi)
{
	if (wsi->tp_task_owner.head == NULL)
		return NULL;

	return lws_container_of(wsi->tp_task_owner.head,
				 struct lws_threadpool_task, list);
}

#if defined(LWS_WITH_SECURE_STREAMS)
struct lws_threadpool_task *
lws_threadpool_get_task_ss(struct lws_ss_handle *ss)
{
	return lws_threadpool_get_task_wsi(ss->wsi);
}
#endif
