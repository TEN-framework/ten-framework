/*
 * ws protocol handler plugin for "lws-minimal" demonstrating lws threadpool
 *
 * Written in 2010-2019 by Andy Green <andy@warmcat.com>
 *
 * This file is made available under the Creative Commons CC0 1.0
 * Universal Public Domain Dedication.
 *
 * The main reason some things are as they are is that the task lifecycle may
 * be unrelated to the wsi lifecycle that queued that task.
 *
 * Consider the task may call an external library and run for 30s without
 * "checking in" to see if it should stop.  The wsi that started the task may
 * have closed at any time before the 30s are up, with the browser window
 * closing or whatever.
 *
 * So data shared between the asynchronous task and the wsi must have its
 * lifecycle determined by the task, not the wsi.  That means a separate struct
 * that can be freed by the task.
 *
 * In the case the wsi outlives the task, the tasks do not get destroyed until
 * the service thread has called lws_threadpool_task_status() on the completed
 * task.  So there is no danger of the shared task private data getting randomly
 * freed.
 */

#if !defined (LWS_PLUGIN_STATIC)
#define LWS_DLL
#define LWS_INTERNAL
#include <libwebsockets.h>
#endif

#include <string.h>

struct per_vhost_data__minimal {
	struct lws_threadpool *tp;
	struct lws_context *context;
	lws_sorted_usec_list_t sul;
	const char *config;
};

struct task_data {
	char result[64];

	uint64_t pos, end;
};

#if defined(WIN32)
static void usleep(unsigned long l) { Sleep(l / 1000); }
#endif

/*
 * Create the private data for the task
 *
 * Notice we hand over responsibility for the cleanup and freeing of the
 * allocated task_data to the threadpool, because the wsi it was originally
 * bound to may close while the thread is still running.  So we allocate
 * something discrete for the task private data that can be definitively owned
 * and freed by the threadpool, not the wsi... the pss won't do, as it only
 * exists for the lifecycle of the wsi connection.
 *
 * When the task is created, we also tell it how to destroy the private data
 * by giving it args.cleanup as cleanup_task_private_data() defined below.
 */

static struct task_data *
create_task_private_data(void)
{
	struct task_data *priv = malloc(sizeof(*priv));

	return priv;
}

/*
 * Destroy the private data for the task
 *
 * Notice the wsi the task was originally bound to may be long gone, in the
 * case we are destroying the lws context and the thread was doing something
 * for a long time without checking in.
 */
static void
cleanup_task_private_data(struct lws *wsi, void *user)
{
	struct task_data *priv = (struct task_data *)user;

	free(priv);
}

/*
 * This runs in its own thread, from the threadpool.
 *
 * The implementation behind this in lws uses pthreads, but no pthreadisms are
 * required in the user code.
 *
 * The example counts to 10M, "checking in" to see if it should stop after every
 * 100K and pausing to sync with the service thread to send a ws message every
 * 1M.  It resumes after the service thread determines the wsi is writable and
 * the LWS_CALLBACK_SERVER_WRITEABLE indicates the task thread can continue by
 * calling lws_threadpool_task_sync().
 */

static enum lws_threadpool_task_return
task_function(void *user, enum lws_threadpool_task_status s)
{
	struct task_data *priv = (struct task_data *)user;
	int budget = 100 * 1000;

	if (priv->pos == priv->end)
		return LWS_TP_RETURN_FINISHED;

	/*
	 * Preferably replace this with ~100ms of your real task, so it
	 * can "check in" at short intervals to see if it has been asked to
	 * stop.
	 *
	 * You can just run tasks atomically here with the thread dedicated
	 * to it, but it will cause odd delays while shutting down etc and
	 * the task will run to completion even if the wsi that started it
	 * has since closed.
	 */

	while (budget--)
		priv->pos++;

	usleep(100000);

	if (!(priv->pos % (1000 * 1000))) {
		lws_snprintf(priv->result + LWS_PRE,
			     sizeof(priv->result) - LWS_PRE,
			     "pos %llu", (unsigned long long)priv->pos);

		return LWS_TP_RETURN_SYNC;
	}

	return LWS_TP_RETURN_CHECKING_IN;
}


static void
sul_tp_dump(struct lws_sorted_usec_list *sul)
{
	struct per_vhost_data__minimal *vhd =
		lws_container_of(sul, struct per_vhost_data__minimal, sul);
	/*
	 * in debug mode, dump the threadpool stat to the logs once
	 * a second
	 */
	lws_threadpool_dump(vhd->tp);
	lws_sul_schedule(vhd->context, 0, &vhd->sul,
			 sul_tp_dump, LWS_US_PER_SEC);
}


static int
callback_minimal(struct lws *wsi, enum lws_callback_reasons reason,
			void *user, void *in, size_t len)
{
	struct per_vhost_data__minimal *vhd =
			(struct per_vhost_data__minimal *)
			lws_protocol_vh_priv_get(lws_get_vhost(wsi),
					lws_get_protocol(wsi));
	const struct lws_protocol_vhost_options *pvo;
	struct lws_threadpool_create_args cargs;
	struct lws_threadpool_task_args args;
	struct lws_threadpool_task *task;
	struct task_data *priv;
	int n, m, r = 0;
	char name[32];
	void *_user;

	switch (reason) {
	case LWS_CALLBACK_PROTOCOL_INIT:
		/* create our per-vhost struct */
		vhd = lws_protocol_vh_priv_zalloc(lws_get_vhost(wsi),
				lws_get_protocol(wsi),
				sizeof(struct per_vhost_data__minimal));
		if (!vhd)
			return 1;

		vhd->context = lws_get_context(wsi);

		/* recover the pointer to the globals struct */
		pvo = lws_pvo_search(
			(const struct lws_protocol_vhost_options *)in,
			"config");
		if (!pvo || !pvo->value) {
			lwsl_err("%s: Can't find \"config\" pvo\n", __func__);
			return 1;
		}
		vhd->config = pvo->value;

		memset(&cargs, 0, sizeof(cargs));

		cargs.max_queue_depth = 8;
		cargs.threads = 3;
		vhd->tp = lws_threadpool_create(lws_get_context(wsi),
				&cargs, "%s",
				lws_get_vhost_name(lws_get_vhost(wsi)));
		if (!vhd->tp)
			return 1;

		lws_sul_schedule(vhd->context, 0, &vhd->sul,
				 sul_tp_dump, LWS_US_PER_SEC);
		break;

	case LWS_CALLBACK_PROTOCOL_DESTROY:
		lws_threadpool_finish(vhd->tp);
		lws_threadpool_destroy(vhd->tp);
		lws_sul_cancel(&vhd->sul);
		break;

	case LWS_CALLBACK_ESTABLISHED:

		memset(&args, 0, sizeof(args));
		priv = args.user = create_task_private_data();
		if (!args.user)
			return 1;

		priv->pos = 0;
		priv->end = 10 * 1000 * 1000;

		/* queue the task... the task takes on responsibility for
		 * destroying args.user.  pss->priv just has a copy of it */

		args.wsi = wsi;
		args.task = task_function;
		args.cleanup = cleanup_task_private_data;

		lws_get_peer_simple(wsi, name, sizeof(name));

		if (!lws_threadpool_enqueue(vhd->tp, &args, "ws %s", name)) {
			lwsl_user("%s: Couldn't enqueue task\n", __func__);
			cleanup_task_private_data(wsi, priv);
			return 1;
		}

		lws_set_timeout(wsi, PENDING_TIMEOUT_THREADPOOL, 30);

		/*
		 * so the asynchronous worker will let us know the next step
		 * by causing LWS_CALLBACK_SERVER_WRITEABLE
		 */

		break;

	case LWS_CALLBACK_CLOSED:
		break;

	case LWS_CALLBACK_WS_SERVER_DROP_PROTOCOL:
		lwsl_debug("LWS_CALLBACK_WS_SERVER_DROP_PROTOCOL: %p\n", wsi);
		lws_threadpool_dequeue_task(lws_threadpool_get_task_wsi(wsi));
		break;

	case LWS_CALLBACK_SERVER_WRITEABLE:

		/*
		 * even completed tasks wait in a queue until we call the
		 * below on them.  Then they may destroy themselves and their
		 * args.user data (by calling the cleanup callback).
		 *
		 * If you need to get things from the still-valid private task
		 * data, copy it here before calling
		 * lws_threadpool_task_status() that may free the task and the
		 * private task data.
		 */

		task = lws_threadpool_get_task_wsi(wsi);
		if (!task)
			break;
		n = (int)lws_threadpool_task_status(task, &_user);
		lwsl_debug("%s: LWS_CALLBACK_SERVER_WRITEABLE: status %d\n",
			   __func__, n);
		switch(n) {

		case LWS_TP_STATUS_FINISHED:
		case LWS_TP_STATUS_STOPPED:
		case LWS_TP_STATUS_QUEUED:
		case LWS_TP_STATUS_RUNNING:
		case LWS_TP_STATUS_STOPPING:
			return 0;

		case LWS_TP_STATUS_SYNCING:
			/* the task has paused for us to do something */
			break;
		default:
			return -1;
		}

		priv = (struct task_data *)_user;

		lws_set_timeout(wsi, PENDING_TIMEOUT_THREADPOOL_TASK, 5);

		n = (int)strlen(priv->result + LWS_PRE);
		m = lws_write(wsi, (unsigned char *)priv->result + LWS_PRE,
			      (unsigned int)n, LWS_WRITE_TEXT);
		if (m < n) {
			lwsl_err("ERROR %d writing to ws socket\n", m);
			lws_threadpool_task_sync(task, 1);
			return -1;
		}

		/*
		 * service thread has done whatever it wanted to do with the
		 * data the task produced: if it's waiting to do more it can
		 * continue now.
		 */
		lws_threadpool_task_sync(task, 0);
		break;

	default:
		break;
	}

	return r;
}

#define LWS_PLUGIN_PROTOCOL_MINIMAL \
	{ \
		"lws-minimal", \
		callback_minimal, \
		0, \
		128, \
		0, NULL, 0 \
	}
