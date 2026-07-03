/*
 * lws-minimal-secure-streams-threads
 *
 * Written in 2010-2021 by Andy Green <andy@warmcat.com>
 *
 * This file is made available under the Creative Commons CC0 1.0
 * Universal Public Domain Dedication.
 *
 *
 * This demonstrates how other threads can wake the lws event loop and ask it
 * to do things via lws_cancel_service(), notifying Secure Streams using the
 * LWSSSCS_EVENT_WAIT_CANCELLED state callback.
 *
 * Because of what we're testing, we don't actually connect the SS just create
 * it and wait for the states we are testing for to come at 10Hz.
 *
 * We run the test for 3s and check we got an appropriate amount of wakes
 * to call it a success.
 *
 * You can use the same pattern to have any amount of shared data protected by
 * the mutex, containing whatever the other threads want the lws event loop
 * thread to do for them.
 */

#include <libwebsockets.h>
#include <string.h>
#include <signal.h>

#include <pthread.h>

/*
 * Define this to cause an ss api access from a foreign thread, it will
 * assert.  This is for testing lws, don't do this in your code.
 */
// #define DO_ILLEGAL_API_THREAD

static int interrupted, bad = 1, finished;
static lws_sorted_usec_list_t sul_timeout;
static struct lws_context *context;
static pthread_t pthread_spam;
static int wakes, started_thread;

#if defined(DO_ILLEGAL_API_THREAD)
static struct lws_ss_handle *ss; /* only needed for DO_ILLEGAL_API_THREAD */
#endif

/* the data shared between the spam thread and the lws event loop */

static pthread_mutex_t lock_shared;
static int shared_counter;


#if !defined(LWS_SS_USE_SSPC)
static const char * const default_ss_policy =
	"{"
		"\"schema-version\":1,"
		"\"s\": ["
			"{"
				"\"mintest\": {"
					"\"endpoint\": \"connectivitycheck.android.com\","
					"\"http_url\": \"generate_204\","
					"\"port\": 80,"
					"\"protocol\": \"h1\","
					"\"http_method\": \"GET\","
					"\"opportunistic\": true,"
					"\"http_expect\": 204,"
					"\"http_fail_redirect\": true"
				"}"
			"}"
		"]"
	"}"
;

#endif

typedef struct myss {
	struct lws_ss_handle 		*ss;
	void				*opaque_data;
	/* ... application specific state ... */
} myss_t;

static void *
thread_spam(void *d)
{

	do {
		pthread_mutex_lock(&lock_shared); /* --------- shared lock { */

		/*
		 * prepare the shared data area to indicate whatever it is that
		 * we want doing on the main event loop.  In this case, we just
		 * bump a counter, but it can be any amount of data prepared,
		 * eg, whole info struct for a connection we want.
		 */

		shared_counter++;

		lwsl_notice("%s: cancelling wait from spam thread: %d\n",
				__func__, shared_counter);
		lws_cancel_service(context);

#if defined(DO_ILLEGAL_API_THREAD)
		/*
		 * ILLEGAL...
		 * We cannot call any other lws api from a foreign thread
		 */

		if (ss)
			lws_ss_request_tx(ss);
#endif

		pthread_mutex_unlock(&lock_shared); /* } shared lock ------- */

		usleep(100000); /* wait 100ms and signal main thread again */

	} while (!finished);

	pthread_exit(NULL);

	return NULL;
}


static lws_ss_state_return_t
myss_state(void *userobj, void *h_src, lws_ss_constate_t state,
	   lws_ss_tx_ordinal_t ack)
{
	// myss_t *m = (myss_t *)userobj;
	void *retval;

	switch (state) {
	case LWSSSCS_CREATING:
		if (pthread_create(&pthread_spam, NULL, thread_spam, NULL)) {
			lwsl_err("thread creation failed\n");
			return LWSSSSRET_DESTROY_ME;
		}
		started_thread = 1;
		break;
	case LWSSSCS_DESTROYING:
		finished = 1;
		if (started_thread)
			pthread_join(pthread_spam, &retval);
		break;

	case LWSSSCS_EVENT_WAIT_CANCELLED:
		pthread_mutex_lock(&lock_shared); /* --------- shared lock { */
		lwsl_notice("%s: LWSSSCS_EVENT_WAIT_CANCELLED: %d, shared: %d\n",
			    __func__, ++wakes, shared_counter);
		pthread_mutex_unlock(&lock_shared); /* } shared lock ------- */
		break;

	default:
		break;
	}

	return LWSSSSRET_OK;
}

static const lws_ss_info_t ssi_lws_threads = {
	.handle_offset		  = offsetof(myss_t, ss),
	.opaque_user_data_offset  = offsetof(myss_t, opaque_data),
	/* we don't actually do any rx or tx in this test */
	.state			  = myss_state,
	.user_alloc		  = sizeof(myss_t),
	.streamtype		  = "mintest",
	.manual_initial_tx_credit = 0,
};

static void
sul_timeout_cb(lws_sorted_usec_list_t *sul)
{
	lwsl_notice("%s: test finishing\n", __func__);
	interrupted = 1;
}


static void
sigint_handler(int sig)
{
	interrupted = 1;
}

static int
system_notify_cb(lws_state_manager_t *mgr, lws_state_notify_link_t *link,
		   int current, int target)
{
	if (current != LWS_SYSTATE_OPERATIONAL || target != LWS_SYSTATE_OPERATIONAL)
		return 0;

	/* the test SS.. not going to connect it, just see if the cancel_service
	 * messages are coming
	 */

	if (lws_ss_create(context, 0, &ssi_lws_threads, NULL,
#if defined(DO_ILLEGAL_API_THREAD)
			&ss,
#else
			NULL,
#endif
			NULL, NULL)) {
		lwsl_err("%s: failed to create secure stream\n",
			 __func__);

		return -1;
	}

	/* set up the test timeout */

	lws_sul_schedule(context, 0, &sul_timeout, sul_timeout_cb,
			 3 * LWS_US_PER_SEC);

	return 0;
}

int main(int argc, const char **argv)
{
	lws_state_notify_link_t notifier = { { NULL, NULL, NULL},
					     system_notify_cb, "app" };
	lws_state_notify_link_t *na[] = { &notifier, NULL };
	struct lws_context_creation_info info;

	signal(SIGINT, sigint_handler);

	memset(&info, 0, sizeof info);

	lws_cmdline_option_handle_builtin(argc, argv, &info);

	lwsl_user("LWS Secure Streams threads test client [-d<verb>]\n");

	info.fd_limit_per_thread	= 1 + 6 + 1;
	info.port			= CONTEXT_PORT_NO_LISTEN;
#if !defined(LWS_SS_USE_SSPC)
	info.pss_policies_json		= default_ss_policy;
#else
	info.protocols			= lws_sspc_protocols;
	{
		const char *p;

		/* connect to ssproxy via UDS by default, else via
		 * tcp connection to this port */
		if ((p = lws_cmdline_option(argc, argv, "-p")))
			info.ss_proxy_port = (uint16_t)atoi(p);

		/* UDS "proxy.ss.lws" in abstract namespace, else this socket
		 * path; when -p given this can specify the network interface
		 * to bind to */
		if ((p = lws_cmdline_option(argc, argv, "-i")))
			info.ss_proxy_bind = p;

		/* if -p given, -a specifies the proxy address to connect to */
		if ((p = lws_cmdline_option(argc, argv, "-a")))
			info.ss_proxy_address = p;
	}
#endif
	info.options			= LWS_SERVER_OPTION_EXPLICIT_VHOSTS |
					  LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;
	info.register_notifier_list = na;

	/* create the context */

	context = lws_create_context(&info);
	if (!context) {
		lwsl_err("lws init failed\n");
		return 1;
	}

#if defined(LWS_SS_USE_SSPC)
	if (!lws_create_vhost(context, &info)) {
		lwsl_err("%s: failed to create default vhost\n", __func__);
		goto bail;
	}
#endif

	/* the event loop */

	while (lws_service(context, 0) >= 0 && !interrupted)
		;

	/* compare what happened with what we expect */

	if (wakes > 10)
		/* OSX can do the usleep thread slower than 100ms */
		bad = 0;

	lwsl_notice("wakes %d\n", wakes);

#if defined(LWS_SS_USE_SSPC)
bail:
#endif
	lws_sul_cancel(&sul_timeout);
	lws_context_destroy(context);

	lwsl_user("Completed: %s\n", bad ? "failed" : "OK");

	return bad;
}
