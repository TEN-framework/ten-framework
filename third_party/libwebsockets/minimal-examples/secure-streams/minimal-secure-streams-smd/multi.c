/*
 * lws-minimal-secure-streams-smd
 *
 * Written in 2010-2021 by Andy Green <andy@warmcat.com>
 *
 * This file is made available under the Creative Commons CC0 1.0
 * Universal Public Domain Dedication.
 *
 *
 * This demonstrates a minimal http client using secure streams to access the
 * SMD api.  This file is only built when LWS_SS_USE_SSPC defined.
 *
 * This is an alternative test implementation selected by --multi at runtime,
 * it's in its own file to stop muddying up the main test sources.  It's only
 * available when built with SSPC / produces -client executable.
 *
 * We will fork several times, the original thread and the forks hook up to
 * the proxy with smd SS, each fork waits a second for everyone to have joined,
 * and then each fork (NOT the original process) sends a bunch of user messages
 * that all the forks should receive, having been distributed by SMD and the
 * ss proxy.
 *
 * The participants check they received all the messages expected from everyone
 * and then send a final message indicating success and exits.  The original
 * fork is watching for these to arrive before the timeout, if so it's a PASS.
 */

#include <libwebsockets.h>
#include <string.h>
#include <signal.h>

static int bad = 1, interrupted;

/* number of forks */
#define FORKS 4
/* number of messages each will send, eg, 4 forks 64 message == 256 messages */
#define MSGCOUNT 64

typedef struct myss {
	struct lws_ss_handle 		*ss;
	void				*opaque_data;
	/* ... application specific state ... */
	uint64_t			seen_mask[FORKS];
	int				seen_msgs[FORKS];
	lws_sorted_usec_list_t		sul;
	int				count;
	char				seen_all;
	char				send_seen_all;
	char				starting;
} myss_t;


/* secure streams payload interface */

static lws_ss_state_return_t
multi_myss_rx(void *userobj, const uint8_t *buf, size_t len, int flags)
{
	myss_t *m = (myss_t *)userobj;
	const char *p;
	int fk, t, n;
	size_t al;

	/* ignore our and other forks announcing their result */

	if (lws_json_simple_find((const char *)buf, len, "\"seen_all\":", &al))
		return LWSSSSRET_OK;

	/*
	 * otherwise once we saw the expected messages, any other messages
	 * coming in this class are wrong
	 */

	if (m->seen_all) {
		lwsl_err("%s: unexpected extra messages\n", __func__);
		return LWSSSSRET_DESTROY_ME;
	}

	p = lws_json_simple_find((const char *)buf, len, "\"fork\":", &al);
	if (!p)
		return LWSSSSRET_DESTROY_ME;
	fk = atoi(p);
	if (fk < 1 || fk > FORKS)
		return LWSSSSRET_DESTROY_ME;

	p = lws_json_simple_find((const char *)buf, len, "\"test\":", &al);
	if (!p)
		return LWSSSSRET_DESTROY_ME;
	t = atoi(p);

	if (t < 0 || t >= MSGCOUNT)
		return LWSSSSRET_DESTROY_ME;

	m->seen_mask[fk - 1] |= 1ull << t;
	m->seen_msgs[fk - 1]++; /* keep an eye on dupes */

	/* Have we seen a full set of messages from everyone? */

	for (n = 0; n < FORKS; n++) {
		if (m->seen_msgs[n] != (int)MSGCOUNT)
			return LWSSSSRET_OK;
		if (m->seen_mask[n] != 0xffffffffffffffffull)
			return LWSSSSRET_OK;
	}

	/*
	 * Oh... so we have finished collecting messages
	 */

	lwsl_user("%s: test thread %d: %s received all messages\n", __func__,
			(int)(intptr_t)lws_context_user(lws_ss_get_context(m->ss)),
			lws_ss_tag(m->ss));
	m->seen_all = m->send_seen_all = 1;

	/*
	 * Prepare to inform the original process we saw everything
	 * from everyone OK
	 */

	lws_ss_request_tx(m->ss);

	return LWSSSSRET_OK;
}

static void
sul_multi_tx_periodic_cb(lws_sorted_usec_list_t *sul)
{
	myss_t *m = lws_container_of(sul, myss_t, sul);

	if (!m->send_seen_all && m->seen_all) {
		lws_ss_destroy(&m->ss);
		return;
	}

	m->starting = 1;
	if (m->count < MSGCOUNT ||  m->send_seen_all)
		lws_ss_request_tx(m->ss);
}

static lws_ss_state_return_t
multi_myss_tx(void *userobj, lws_ss_tx_ordinal_t ord, uint8_t *buf, size_t *len,
	int *flags)
{
	myss_t *m = (myss_t *)userobj;

	/*
	 * We want to send exactly MSGCOUNT user class smd messages
	 */

	if (!m->starting || (m->count == MSGCOUNT && !m->send_seen_all))
		return LWSSSSRET_TX_DONT_SEND;

//	lwsl_notice("%s: sending SS smd\n", __func__);

	lws_ser_wu64be(buf, 1 << LWSSMDCL_USER_BASE_BITNUM);
	lws_ser_wu64be(buf + 8, 0); /* valgrind notices uninitialized if left */

	if (m->send_seen_all) {
		*len = LWS_SMD_SS_RX_HEADER_LEN + (unsigned int)
			lws_snprintf((char *)buf + LWS_SMD_SS_RX_HEADER_LEN, *len,
			     "{\"class\":\"user\",\"fork\": %d,\"seen_all\":true}",
			     (int)(intptr_t)lws_context_user(lws_ss_get_context(m->ss)));

		m->send_seen_all = 0;
		lwsl_info("%s: test thread %d: sent summary message\n", __func__,
				(int)(intptr_t)lws_context_user(lws_ss_get_context(m->ss)));
	} else
		*len = LWS_SMD_SS_RX_HEADER_LEN + (unsigned int)
			lws_snprintf((char *)buf + LWS_SMD_SS_RX_HEADER_LEN, *len,
			     "{\"class\":\"user\",\"fork\": %d,\"test\":%u}",
			     (int)(intptr_t)lws_context_user(lws_ss_get_context(m->ss)),
			     m->count++);

	*flags = LWSSS_FLAG_SOM | LWSSS_FLAG_EOM;

	lws_sul_schedule(lws_ss_get_context(m->ss), 0, &m->sul,
			sul_multi_tx_periodic_cb, 25 * LWS_US_PER_MS);

	return LWSSSSRET_OK;
}

static lws_ss_state_return_t
multi_myss_state(void *userobj, void *h_src, lws_ss_constate_t state,
	   lws_ss_tx_ordinal_t ack)
{
	myss_t *m = (myss_t *)userobj;
	int n;

	lwsl_notice("%s: %s: %s (%d), ord 0x%x\n", __func__, lws_ss_tag(m->ss),
		    lws_ss_state_name((int)state), state, (unsigned int)ack);

	switch (state) {
	case LWSSSCS_DESTROYING:
		lws_sul_cancel(&m->sul);
		interrupted = 1;
		return 0;

	case LWSSSCS_CONNECTED:
		lwsl_notice("%s: CONNECTED: test fork %d\n", __func__,
				(int)(intptr_t)lws_context_user(lws_ss_get_context(m->ss)));
		/*
		 * Because in this test everybody is watching and counting
		 * everybody else's messages from different forks, we have to
		 * hold off starting sending for 2s so all forks can join the
		 * proxy first and not miss anything
		 */
		lws_sul_schedule(lws_ss_get_context(m->ss), 0, &m->sul,
				sul_multi_tx_periodic_cb, 2 * LWS_US_PER_SEC);
		m->starting = 0;
		return 0;
	case LWSSSCS_DISCONNECTED:
		for (n = 0; n < FORKS; n++)
			lwsl_notice("%s: testfork %d: peer %d: seen_msg = %d, "
				    "seen make = 0x%llx\n", __func__,
				    (int)(intptr_t)lws_context_user(lws_ss_get_context(m->ss)),
				    n, m->seen_msgs[n],
				    (unsigned long long)m->seen_mask[n]);
		break;
	default:
		break;
	}

	return 0;
}

static const lws_ss_info_t ssi_multi_lws_smd = {
	.handle_offset		  = offsetof(myss_t, ss),
	.opaque_user_data_offset  = offsetof(myss_t, opaque_data),
	.rx			  = multi_myss_rx,
	.tx			  = multi_myss_tx,
	.state			  = multi_myss_state,
	.user_alloc		  = sizeof(myss_t),
	.streamtype		  = LWS_SMD_STREAMTYPENAME,
	.manual_initial_tx_credit = 1 << LWSSMDCL_USER_BASE_BITNUM,
};

static lws_ss_state_return_t
multi_myss_rx_monitor(void *userobj, const uint8_t *buf, size_t len, int flags)
{
	myss_t *m = (myss_t *)userobj;
	const char *p;
	size_t al;
	int fk, n;

	/* ignore our and other forks announcing their result */

	if (!lws_json_simple_find((const char *)buf, len, "\"seen_all\":", &al))
		return LWSSSSRET_OK;

	p = lws_json_simple_find((const char *)buf, len, "\"fork\":", &al);
	if (!p)
		return LWSSSSRET_DESTROY_ME;
	fk = atoi(p);
	if (fk < 1 || fk > FORKS)
		return LWSSSSRET_DESTROY_ME;

	if (m->seen_msgs[fk - 1])
		/* expected only once ... dupe */
		return LWSSSSRET_DESTROY_ME;

	m->seen_msgs[fk - 1] = 1;

	for (n = 0; n < FORKS; n++)
		if (!m->seen_msgs[n])
			return LWSSSSRET_OK;

	/* the test has succeeded */

	bad = 0;
	interrupted = 1;

	return LWSSSSRET_OK;
}

static const lws_ss_info_t ssi_multi_lws_smd_monitor = {
	.handle_offset		  = offsetof(myss_t, ss),
	.opaque_user_data_offset  = offsetof(myss_t, opaque_data),
	.rx			  = multi_myss_rx_monitor,
//	.state			  = multi_myss_state_monitor,
	.user_alloc		  = sizeof(myss_t),
	.streamtype		  = LWS_SMD_STREAMTYPENAME,
	.manual_initial_tx_credit = 1 << LWSSMDCL_USER_BASE_BITNUM,
};

/* for comparison, this is a non-SS lws_smd participant */

static int
direct_smd_cb(void *opaque, lws_smd_class_t _class, lws_usec_t timestamp,
	      void *buf, size_t len)
{
	struct lws_context **pctx = (struct lws_context **)opaque;

	if (_class != LWSSMDCL_SYSTEM_STATE)
		return 0;

	if (!lws_json_simple_strcmp(buf, len, "\"state\":", "OPERATIONAL")) {

		/*
		 * Create the SSPC link to lws_smd... notice in ssi_lws_smd
		 * above, we tell this link to use the user class filter.
		 *
		 * If context->user is zero, we are the original process
		 * monitoring the progress of the others, otherwise we are
		 * 1 .. FORKS and producing / checking the smd messages
		 */

		lwsl_info("%s: starting ss for test fork %d\n", __func__,
				(int)(intptr_t)lws_context_user(*pctx));

		if (lws_ss_create(*pctx, 0, lws_context_user(*pctx) ?
				&ssi_multi_lws_smd /* forked process send / check */:
				&ssi_multi_lws_smd_monitor /* original monitors */,
				NULL, NULL, NULL, NULL)) {
			lwsl_err("%s: failed to create secure stream\n",
				 __func__);

			return -1;
		}
	}

	return 0;
}


static void
sul_timeout_cb(lws_sorted_usec_list_t *sul)
{
	interrupted = 1;
}

int
smd_ss_multi_test(int argc, const char **argv)
{
	struct lws_context_creation_info info;
	lws_sorted_usec_list_t sul_timeout;
	struct lws_context *context;
	pid_t pid;
	int n;

	lwsl_user("LWS Secure Streams SMD MULTI test client [-d<verb>]\n");

	for (n = 0; n < FORKS; n++) {
		pid = fork();
		if (!pid) /* forked child */ {
			break;
		}
		lwsl_notice("%s: forked test process %u\n", __func__, pid);
	}

	if (n == FORKS)
		/* the original process */
		n = -1; /* so original ends up with context.user as 0 below */

	memset(&info, 0, sizeof info);
	memset(&sul_timeout, 0, sizeof sul_timeout);

	lws_cmdline_option_handle_builtin(argc, argv, &info);

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

	info.fd_limit_per_thread	= 1 + 6 + 1;
	info.port			= CONTEXT_PORT_NO_LISTEN;
	info.protocols			= lws_sspc_protocols;
	info.options			= LWS_SERVER_OPTION_EXPLICIT_VHOSTS |
					  LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;

	info.early_smd_cb		= direct_smd_cb;
	info.early_smd_class_filter	= 0xffffffff;
	info.early_smd_opaque		= &context;

	info.user			= (void *)(intptr_t)(n + 1);

	/* create the context */

	context = lws_create_context(&info);
	if (!context) {
		lwsl_err("lws init failed\n");
		return 1;
	}

	if (!lws_create_vhost(context, &info)) {
		lwsl_err("%s: failed to create default vhost\n", __func__);
		goto bail;
	}

	/* set up the test timeout */

	lws_sul_schedule(context, 0, &sul_timeout, sul_timeout_cb,
			 10 * LWS_US_PER_SEC);

	/* the event loop */

	while (lws_service(context, 0) >= 0 && !interrupted)
		;

bail:
	lws_context_destroy(context);

	if (n == -1)
		lwsl_user("%s: finished %s\n", __func__, bad ? "FAIL" : "PASS");

	return bad;
}
