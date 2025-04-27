/*
 * lws-minimal-secure-streams-tx
 *
 * Written in 2010-2021 by Andy Green <andy@warmcat.com>
 *
 * This file is made available under the Creative Commons CC0 1.0
 * Universal Public Domain Dedication.
 *
 *
 * This demonstrates proxied mass tx from secure streams, this example is a
 * client that has no policy of its own, but gets stuff done via the ss proxy.
 *
 * It opens a websocket stream and fires 100 x small 80-byte payloads on it
 * at 20Hz (50ms)
 */

#define LWS_SS_USE_SSPC

#include <libwebsockets.h>
#include <string.h>
#include <signal.h>

#define PKT_SIZE 80
#define RATE_US 50000

static int interrupted, bad = 1, reads = 100;

typedef struct myss {
	struct lws_ss_handle 	*ss;
	void			*opaque_data;
	/* ... application specific state ... */
	lws_sorted_usec_list_t	sul;

	int			count;
	char			due;
} myss_t;

/* secure streams payload interface */

static lws_ss_state_return_t
myss_rx(void *userobj, const uint8_t *buf, size_t len, int flags)
{
	/* this example isn't interested in rx */
	return LWSSSSRET_OK;
}

static void
txcb(struct lws_sorted_usec_list *sul)
{
	myss_t *m = lws_container_of(sul, myss_t, sul);

	/*
	 * We want to do 100 of these ws messages, and then exit, so we can run
	 * this as a pass / fail test.
	 */

	if (m->count == reads) {
		interrupted = 1;
		bad = 0;
	} else {
		m->due = 1;
		lws_ss_request_tx(m->ss);
	}

	lws_sul_schedule(lws_ss_get_context(m->ss), 0, &m->sul, txcb, RATE_US);
}

static lws_ss_state_return_t
myss_tx(void *userobj, lws_ss_tx_ordinal_t ord, uint8_t *buf, size_t *len,
	int *flags)
{
	myss_t *m = (myss_t *)userobj;

	if (!m->due)
		return LWSSSSRET_TX_DONT_SEND;

	m->due = 0;

	if (lws_get_random(lws_ss_get_context(m->ss), buf, PKT_SIZE) != PKT_SIZE)
		return LWSSSSRET_TX_DONT_SEND;

	*len = PKT_SIZE;
	*flags = LWSSS_FLAG_SOM | LWSSS_FLAG_EOM;

	m->count++;

	lws_sul_schedule(lws_ss_get_context(m->ss), 0, &m->sul, txcb, RATE_US);

	lwsl_user("%s: sending pkt %d\n", __func__, m->count);

	return LWSSSSRET_OK;
}

static lws_ss_state_return_t
myss_state(void *userobj, void *sh, lws_ss_constate_t state,
		lws_ss_tx_ordinal_t ack)
{
	myss_t *m = (myss_t *)userobj;
	struct lws_context *context = lws_ss_get_context(m->ss);

	lwsl_user("%s: %s, ord 0x%x\n", __func__, lws_ss_state_name((int)state),
		  (unsigned int)ack);

	switch (state) {
	case LWSSSCS_CREATING:
		return lws_ss_client_connect(m->ss);

	case LWSSSCS_CONNECTED:
		lws_sul_schedule(context, 0, &m->sul, txcb, RATE_US);
		break;
	case LWSSSCS_DISCONNECTED:
		lws_sul_cancel(&m->sul);
		break;
	case LWSSSCS_ALL_RETRIES_FAILED:
		/* if we're out of retries, we want to close the app and FAIL */
		interrupted = 1;
		break;
	default:
		break;
	}

	return 0;
}

static void
sigint_handler(int sig)
{
	interrupted = 1;
}

static const lws_ss_info_t ssi = {
	.handle_offset			= offsetof(myss_t, ss),
	.opaque_user_data_offset	= offsetof(myss_t, opaque_data),
	.rx				= myss_rx,
	.tx				= myss_tx,
	.state				= myss_state,
	.user_alloc			= sizeof(myss_t),
	.streamtype			= "spam"
};

int main(int argc, const char **argv)
{
	int n = 0, logs = LLL_USER | LLL_ERR | LLL_WARN | LLL_NOTICE;
	struct lws_context_creation_info info;
	struct lws_context *context;
	const char *p;

	signal(SIGINT, sigint_handler);

	if ((p = lws_cmdline_option(argc, argv, "-d")))
		logs = atoi(p);

	if ((p = lws_cmdline_option(argc, argv, "-c")))
		reads = atoi(p);

	lws_set_log_level(logs, NULL);
	lwsl_user("LWS secure streams client TX [-d<verb>]\n");

	memset(&info, 0, sizeof info); /* otherwise uninitialized garbage */

	info.options = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;
	info.fd_limit_per_thread = 1 + 6 + 1;
	info.port = CONTEXT_PORT_NO_LISTEN;
	info.protocols = lws_sspc_protocols;
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

	context = lws_create_context(&info);
	if (!context) {
		lwsl_err("lws init failed\n");
		goto bail1;
	}

	if (lws_ss_create(context, 0, &ssi, NULL, NULL, NULL, NULL)) {
		lwsl_err("%s: create secure stream failed\n", __func__);
		goto bail;
	}

	/* the event loop */

	while (n >= 0 && !interrupted)
		n = lws_service(context, 0);

bail:
	lws_context_destroy(context);

bail1:
	lwsl_user("Completed: %s\n", bad ? "failed" : "OK");

	return bad;
}
