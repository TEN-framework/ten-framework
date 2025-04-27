/*
 * lws-api-test-lws_sequencer
 *
 * Written in 2019 by Andy Green <andy@warmcat.com>
 *
 * This file is made available under the Creative Commons CC0 1.0
 * Universal Public Domain Dedication.
 *
 * This api test uses the lws_sequencer api to make five http client requests
 * to libwebsockets.org in sequence, from inside the event loop.  The fourth
 * fourth http client request is directed to port 22 where it stalls
 * triggering the lws_sequencer timeout flow.  The fifth is given a nonexistant
 * dns name and is expected to fail.
 */

#include <libwebsockets.h>

#include <signal.h>

static int interrupted, test_good = 0;

enum {
	SEQ1,
	SEQ2,
	SEQ3_404,
	SEQ4_TIMEOUT,		/* we expect to timeout */
	SEQ5_BAD_ADDRESS	/* we expect the connection to fail */
};

/*
 * This is the user defined struct whose space is allocated along with the
 * sequencer when that is created.
 *
 * You'd put everything your sequencer needs to do its job in here.
 */

struct myseq {
	struct lws_vhost	*vhost;
	struct lws		*cwsi;	/* client wsi for current step if any */

	int			state;	/* which test we're on */
	int			http_resp;
};

/* sequencer messages specific to this sequencer */

enum {
	SEQ_MSG_CLIENT_FAILED = LWSSEQ_USER_BASE,
	SEQ_MSG_CLIENT_DONE,
};

/* this is the sequence of GETs we will do */

static const char *url_paths[] = {
	"https://libwebsockets.org/index.html",
	"https://libwebsockets.org/lws.css",
	"https://libwebsockets.org/404.html",
	"https://libwebsockets.org:22",		/* this causes us to time out */
	"https://doesntexist.invalid/"		/* fail early in connect */
};


static void
sigint_handler(int sig)
{
	interrupted = 1;
}

/*
 * This is the sequencer-aware http protocol handler.  It monitors the client
 * http action and queues messages for the sequencer when something definitive
 * happens.
 */

static int
callback_http(struct lws *wsi, enum lws_callback_reasons reason, void *user,
	      void *in, size_t len)
{
	struct myseq *s = (struct myseq *)user;
	int seq_msg = SEQ_MSG_CLIENT_FAILED;

	switch (reason) {

	/* because we are protocols[0] ... */
	case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
		lwsl_notice("CLIENT_CONNECTION_ERROR: %s\n",
			 in ? (char *)in : "(null)");
		goto notify;

	case LWS_CALLBACK_ESTABLISHED_CLIENT_HTTP:
		if (!s)
			return 1;
		s->http_resp = (int)lws_http_client_http_response(wsi);
		lwsl_info("Connected with server response: %d\n", s->http_resp);
		break;

	/* chunks of chunked content, with header removed */
	case LWS_CALLBACK_RECEIVE_CLIENT_HTTP_READ:
		lwsl_info("RECEIVE_CLIENT_HTTP_READ: read %d\n", (int)len);
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
		lwsl_notice("LWS_CALLBACK_COMPLETED_CLIENT_HTTP: wsi %p\n",
			    wsi);
		if (!s)
			return 1;
		/*
		 * We got a definitive transaction completion
		 */
		seq_msg = SEQ_MSG_CLIENT_DONE;
		goto notify;

	case LWS_CALLBACK_CLOSED_CLIENT_HTTP:
		lwsl_info("LWS_CALLBACK_CLOSED_CLIENT_HTTP\n");
		if (!s)
			return 1;

		lwsl_user("%s: wsi %p: seq failed at CLOSED_CLIENT_HTTP\n",
			  __func__, wsi);
		goto notify;

	default:
		break;
	}

	return lws_callback_http_dummy(wsi, reason, user, in, len);

notify:
	/*
	 * We only inform the sequencer of a definitive outcome for our step.
	 *
	 * So once we have informed it, we detach ourselves from the sequencer
	 * and the sequencer from ourselves.  Wsi may want to live on but after
	 * we got our result and moved on to the next test or completed, the
	 * sequencer doesn't want to hear from it again.
	 */
	if (!s)
		return 1;

	lws_set_wsi_user(wsi, NULL);
	s->cwsi = NULL;
	lws_seq_queue_event(lws_seq_from_user(s), seq_msg,
				  NULL, NULL);

	return 0;
}

static const struct lws_protocols protocols[] = {
	{ "seq-test-http", callback_http, 0, 0, 0, NULL, 0 },
	LWS_PROTOCOL_LIST_TERM
};


static int
sequencer_start_client(struct myseq *s)
{
	struct lws_client_connect_info i;
	const char *prot, *path1;
	char uri[128], path[128];
	int n;

	lws_strncpy(uri, url_paths[s->state], sizeof(uri));

	memset(&i, 0, sizeof i);
	i.context = lws_seq_get_context(lws_seq_from_user(s));

	if (lws_parse_uri(uri, &prot, &i.address, &i.port, &path1)) {
		lwsl_err("%s: uri error %s\n", __func__, uri);
	}

	if (!strcmp(prot, "https"))
		i.ssl_connection = LCCSCF_USE_SSL;

	path[0] = '/';
	n = 1;
	if (path1[0] == '/')
		n = 0;
	lws_strncpy(&path[n], path1, sizeof(path) - 1);

	i.path = path;
	i.host = i.address;
	i.origin = i.address;
	i.method = "GET";
	i.vhost = s->vhost;
	i.userdata = s;

	i.protocol = protocols[0].name;
	i.local_protocol_name = protocols[0].name;
	i.pwsi = &s->cwsi;

	if (!lws_client_connect_via_info(&i)) {
		lwsl_notice("%s: connecting to %s://%s:%d%s failed\n",
			    __func__, prot, i.address, i.port, path);

		/* we couldn't even get started with the client connection */

		lws_seq_queue_event(lws_seq_from_user(s),
				    (lws_seq_events_t)SEQ_MSG_CLIENT_FAILED, NULL, NULL);

		return 1;
	}

	lws_seq_timeout_us(lws_seq_from_user(s), 3 * LWS_US_PER_SEC);

	lwsl_notice("%s: wsi %p: connecting to %s://%s:%d%s\n", __func__,
		    s->cwsi, prot, i.address, i.port, path);

	return 0;
}

/*
 * The sequencer callback handles queued sequencer messages in the order they
 * were queued.  The messages are presented from the event loop thread context
 * even if they were queued from a different thread.
 */

static lws_seq_cb_return_t
sequencer_cb(struct lws_sequencer *seq, void *user, int event,
	     void *data, void *aux)
{
	struct myseq *s = (struct myseq *)user;

	switch ((int)event) {
	case LWSSEQ_CREATED: /* our sequencer just got started */
		s->state = SEQ1;  /* first thing we'll do is the first url */
		goto step;

	case LWSSEQ_DESTROYED:
		/*
		 * This sequencer is about to be destroyed.  If we have any
		 * other assets in play, detach them from us.
		 */
		if (s->cwsi)
			lws_set_wsi_user(s->cwsi, NULL);

		interrupted = 1;
		break;

	case LWSSEQ_TIMED_OUT: /* current step timed out */
		if (s->state == SEQ4_TIMEOUT) {
			lwsl_user("%s: test %d got expected timeout\n",
				  __func__, s->state);
			goto done;
		}
		lwsl_user("%s: seq timed out at step %d\n", __func__, s->state);
		return LWSSEQ_RET_DESTROY;

	case SEQ_MSG_CLIENT_FAILED:
		if (s->state == SEQ5_BAD_ADDRESS) {
			/*
			 * in this specific case, we expect to fail
			 */
			lwsl_user("%s: test %d failed as expected\n",
				  __func__, s->state);
			goto done;
		}

		lwsl_user("%s: seq failed at step %d\n", __func__, s->state);

		return LWSSEQ_RET_DESTROY;

	case SEQ_MSG_CLIENT_DONE:
		if (s->state >= SEQ4_TIMEOUT) {
			/*
			 * In these specific cases, done would be a failure,
			 * we expected to timeout or fail
			 */
			lwsl_user("%s: seq failed at step %d\n", __func__,
				  s->state);

			return LWSSEQ_RET_DESTROY;
		}
		lwsl_user("%s: seq done step %d (resp %d)\n", __func__,
			  s->state, s->http_resp);

done:
		lws_seq_timeout_us(lws_seq_from_user(s), LWSSEQTO_NONE);
		s->state++;
		if (s->state == LWS_ARRAY_SIZE(url_paths)) {
			/* the sequence has completed */
			lwsl_user("%s: sequence completed OK\n", __func__);

			test_good = 1;

			return LWSSEQ_RET_DESTROY;
		}

step:
		sequencer_start_client(s);
		break;
	default:
		break;
	}

	return LWSSEQ_RET_CONTINUE;
}

int
main(int argc, const char **argv)
{
	int n = 1, logs = LLL_USER | LLL_ERR | LLL_WARN | LLL_NOTICE;
	struct lws_context_creation_info info;
	struct lws_context *context;
	struct lws_sequencer *seq;
	struct lws_vhost *vh;
	lws_seq_info_t i;
	struct myseq *s;
	const char *p;

	/* the normal lws init */

	signal(SIGINT, sigint_handler);

	if ((p = lws_cmdline_option(argc, argv, "-d")))
		logs = atoi(p);

	lws_set_log_level(logs, NULL);
	lwsl_user("LWS API selftest: lws_sequencer\n");

	memset(&info, 0, sizeof info); /* otherwise uninitialized garbage */
	info.port = CONTEXT_PORT_NO_LISTEN;
	info.options = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT |
		       LWS_SERVER_OPTION_EXPLICIT_VHOSTS;
	info.protocols = protocols;

#if defined(LWS_WITH_MBEDTLS) || defined(USE_WOLFSSL)
	/*
	 * OpenSSL uses the system trust store.  mbedTLS has to be told which
	 * CA to trust explicitly.
	 */
	info.client_ssl_ca_filepath = "./libwebsockets.org.cer";
#endif

	context = lws_create_context(&info);
	if (!context) {
		lwsl_err("lws init failed\n");
		return 1;
	}

	vh = lws_create_vhost(context, &info);
	if (!vh) {
		lwsl_err("Failed to create first vhost\n");
		goto bail1;
	}

	/*
	 * Create the sequencer... when the event loop starts, it will
	 * receive the LWSSEQ_CREATED callback
	 */

	memset(&i, 0, sizeof(i));
	i.context = context;
	i.user_size = sizeof(struct myseq);
	i.puser = (void **)&s;
	i.cb = sequencer_cb;
	i.name = "seq";

	seq = lws_seq_create(&i);
	if (!seq) {
		lwsl_err("%s: unable to create sequencer\n", __func__);
		goto bail1;
	}
	s->vhost = vh;

	/* the usual lws event loop */

	while (n >= 0 && !interrupted)
		n = lws_service(context, 0);

bail1:
	lwsl_user("Completed: %s\n", !test_good ? "FAIL" : "PASS");

	lws_context_destroy(context);

	return !test_good;
}
