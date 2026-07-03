/*
 * lws-minimal-http-client-attach
 *
 * Written in 2010-2019 by Andy Green <andy@warmcat.com>
 *
 * This file is made available under the Creative Commons CC0 1.0
 * Universal Public Domain Dedication.
 *
 * This demonstrates how to use the lws_system (*attach) api to allow a
 * different thread to arrange to join an existing lws event loop safely.  The
 * attached stuff does an http client GET from the lws event loop, even though
 * it was originally requested from a different thread than the lws event loop.
 */

#include <libwebsockets.h>
#include <string.h>
#include <signal.h>
#if defined(WIN32)
#define HAVE_STRUCT_TIMESPEC
#if defined(pid_t)
#undef pid_t
#endif
#endif
#include <pthread.h>

static struct lws_context *context;
static pthread_t lws_thread;
static pthread_mutex_t lock;
static int interrupted, bad = 1, status;

static int
callback_http(struct lws *wsi, enum lws_callback_reasons reason,
	      void *user, void *in, size_t len)
{
	switch (reason) {

	/* because we are protocols[0] ... */
	case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
		lwsl_err("CLIENT_CONNECTION_ERROR: %s\n",
			 in ? (char *)in : "(null)");
		interrupted = 1;
		break;

	case LWS_CALLBACK_ESTABLISHED_CLIENT_HTTP:
		{
			char buf[128];

			lws_get_peer_simple(wsi, buf, sizeof(buf));
			status = (int)lws_http_client_http_response(wsi);

			lwsl_user("Connected to %s, http response: %d\n",
					buf, status);
		}
		break;

	/* chunks of chunked content, with header removed */
	case LWS_CALLBACK_RECEIVE_CLIENT_HTTP_READ:
		lwsl_user("RECEIVE_CLIENT_HTTP_READ: read %d\n", (int)len);

#if 0  /* enable to dump the html */
		{
			const char *p = in;

			while (len--)
				if (*p < 0x7f)
					putchar(*p++);
				else
					putchar('.');
		}
#endif
		return 0; /* don't passthru */

	/* uninterpreted http content */
	case LWS_CALLBACK_RECEIVE_CLIENT_HTTP:
		{
			char buffer[1024 + LWS_PRE];
			char *px = buffer + LWS_PRE;
			int lenx = sizeof(buffer) - LWS_PRE;

			if (lws_http_client_read(wsi, &px, &lenx) < 0)
				return -1;
		}
		return 0; /* don't passthru */

	case LWS_CALLBACK_COMPLETED_CLIENT_HTTP:
		lwsl_user("LWS_CALLBACK_COMPLETED_CLIENT_HTTP\n");
		interrupted = 1;
		bad = status != 200;
		lws_cancel_service(lws_get_context(wsi)); /* abort poll wait */
		break;

	case LWS_CALLBACK_CLOSED_CLIENT_HTTP:
		interrupted = 1;
		bad = status != 200;
		lws_cancel_service(lws_get_context(wsi)); /* abort poll wait */
		break;

	default:
		break;
	}

	return lws_callback_http_dummy(wsi, reason, user, in, len);
}

static const struct lws_protocols protocols[] = {
	{
		"http",
		callback_http,
		0, 0, 0, NULL, 0
	},
	LWS_PROTOCOL_LIST_TERM
};

void sigint_handler(int sig)
{
	interrupted = 1;
}

static void
attach_callback(struct lws_context *context, int tsi, void *opaque)
{
	struct lws_client_connect_info i;

	/*
	 * Even though it was asked for from a different thread, we are called
	 * back by lws from the lws event loop thread context
	 *
	 * We can set up our operations on the lws event loop and return so
	 * they can happen asynchronously
	 */

	memset(&i, 0, sizeof i); /* otherwise uninitialized garbage */
	i.context = context;
	i.ssl_connection = LCCSCF_USE_SSL;
	i.ssl_connection |= LCCSCF_H2_QUIRK_OVERFLOWS_TXCR |
			    LCCSCF_H2_QUIRK_NGHTTP2_END_STREAM;
	i.port = 443;
	i.address = "warmcat.com";
	i.path = "/";
	i.host = i.address;
	i.origin = i.address;
	i.method = "GET";

	i.protocol = protocols[0].name;

	lws_client_connect_via_info(&i);
}


static int
lws_attach_with_pthreads_locking(struct lws_context *context, int tsi,
				 lws_attach_cb_t cb, lws_system_states_t state,
				 void *opaque, struct lws_attach_item **get)
{
	int n;

	pthread_mutex_lock(&lock);
	/*
	 * We just provide system-specific locking around the lws non-threadsafe
	 * helper that adds and removes things from the pt list
	 */
	n = __lws_system_attach(context, tsi, cb, state, opaque, get);
	pthread_mutex_unlock(&lock);

	return n;
}


lws_system_ops_t ops = {
	.attach = lws_attach_with_pthreads_locking
};

/*
 * We made this into a different thread to model it being run from completely
 * different codebase that's all linked together
 */

static void *
lws_create(void *d)
{
	struct lws_context_creation_info info;

       lwsl_user("%s: tid %p\n", __func__, (void *)(intptr_t)pthread_self());

	memset(&info, 0, sizeof info); /* otherwise uninitialized garbage */
	info.port = CONTEXT_PORT_NO_LISTEN;
	info.options = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;
	info.system_ops = &ops;
	info.protocols = protocols;

	context = lws_create_context(&info);
	if (!context) {
		lwsl_err("lws init failed\n");
		goto bail;
	}

	/* start the event loop */

	while (!interrupted)
		if (lws_service(context, 0))
			interrupted = 1;

	lws_context_destroy(context);

bail:
	pthread_exit(NULL);

	return NULL;
}

int main(int argc, const char **argv)
{
	int n = 0, logs = LLL_USER | LLL_ERR | LLL_WARN | LLL_NOTICE;
	const char *p;
	void *retval;

	signal(SIGINT, sigint_handler);

	if ((p = lws_cmdline_option(argc, argv, "-d")))
		logs = atoi(p);

	lws_set_log_level(logs, NULL);
	lwsl_user("LWS minimal http client attach\n");

	pthread_mutex_init(&lock, NULL);

	/*
	 * The idea of the example is we're going to split the lws context and
	 * event loop off to be created from its own thread... this is like it
	 * was actually started by some completely different code...
	 */

	if (pthread_create(&lws_thread, NULL, lws_create, NULL)) {
		lwsl_err("thread creation failed\n");
		goto bail1;
	}

	/*
	 * Now on the original / different thread representing a different
	 * codebase that wants to join this existing event loop, we'll ask to
	 * get a callback from the event loop context when the event loop
	 * thread is operational.  We have to wait around a bit because we
	 * may run before the lws context was created.
	 */

	while (!context && n++ < 30)
		usleep(10000);

	if (!context) {
		lwsl_err("%s: context didn't start\n", __func__);
		goto bail;
	}

	/*
	 * From our different, non event loop thread, ask for our attach
	 * callback to get called when lws system state is OPERATIONAL
	 */

	lws_system_get_ops(context)->attach(context, 0, attach_callback,
					    LWS_SYSTATE_OPERATIONAL,
					    NULL, NULL);

	/*
	 * That's all we wanted to do with our thread.  Just wait for the lws
	 * thread to exit as well.
	 */

bail:
	pthread_join(lws_thread, &retval);
bail1:
	pthread_mutex_destroy(&lock);

	lwsl_user("%s: finished\n", __func__);

	return 0;
}
