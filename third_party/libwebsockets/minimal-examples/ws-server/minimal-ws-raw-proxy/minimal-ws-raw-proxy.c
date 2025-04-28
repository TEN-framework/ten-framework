/*
 * lws-minimal-ws-raw-proxy
 *
 * Written in 2010-2021 by Andy Green <andy@warmcat.com>
 *
 * This file is made available under the Creative Commons CC0 1.0
 * Universal Public Domain Dedication.
 *
 * This demonstrates a ws (server) -> raw (client) proxy,  it's a ws server
 * that accepts connections, creates an onward client connection to some other
 * no-protocol server, eg, nc -l 127.0.0.1 1234
 *
 * The idea is to show the general approach for making async proxies using lws
 * that are robust and valgrind-clean.
 *
 * There's no vhd or pss on either side.  Instead when the ws server gets an
 * incoming connection and negotiates the ws link, he creates an object
 * representing the proxied connection, it is not destroyed automatically when
 * any particular wsi is closed, instead the last wsi that is part of the
 * proxied connection destroys it when he is closed.
 */

#include <libwebsockets.h>
#include <string.h>
#include <signal.h>
#include <string.h>

/* one of these created for each pending message that is to be forwarded */

typedef struct proxy_msg {
	lws_dll2_t		list;
	size_t			len;
	/*
	 * the packet content is overallocated here, if p is a pointer to
	 * this struct, you can get a pointer to the message contents by
	 * ((uint8_t)&p[1]) + LWS_PRE.
	 *
	 * Notice we additionally take care to overallocate LWS_PRE before the
	 * actual message data, so we can simplify sending it.
	 */
} proxy_msg_t;

/*
 * One of these is created when a inbound ws connection joins, it represents
 * the proxy action provoked by that.
 */

typedef struct proxy_conn {
	struct lws		*wsi_ws; /* wsi for the inbound ws conn */
	struct lws		*wsi_raw; /* wsi for the outbound raw conn */

	lws_dll2_owner_t	pending_msg_to_ws;
	lws_dll2_owner_t	pending_msg_to_raw;
} proxy_conn_t;


static int
proxy_ws_raw_msg_destroy(struct lws_dll2 *d, void *user)
{
	proxy_msg_t *msg = lws_container_of(d, proxy_msg_t, list);

	lws_dll2_remove(d);
	free(msg);

	return 0;
}

/*
 * First the ws server side
 */

static int
callback_proxy_ws_server(struct lws *wsi, enum lws_callback_reasons reason,
			 void *user, void *in, size_t len)
{
	proxy_conn_t *pc = (proxy_conn_t *)lws_get_opaque_user_data(wsi);
	struct lws_client_connect_info i;
	proxy_msg_t *msg;
	uint8_t *data;
	int m, a;

	switch (reason) {
	case LWS_CALLBACK_ESTABLISHED:
		/* so let's create the proxy connection object */
		pc = malloc(sizeof(*pc));
		memset(pc, 0, sizeof(*pc));

		/* mark this accepted ws connection with the proxy conn obj */
		lws_set_opaque_user_data(wsi, pc);
		/* tell the proxy conn object that we are the ws side of it */
		pc->wsi_ws = wsi;

		/*
		 * For this example proxy, our job is to create a new, onward,
		 * raw client connection to proxy stuff on to
		 */

		memset(&i, 0, sizeof(i));

		i.method = "RAW";
		i.context = lws_get_context(wsi);
		i.port = 1234;
		i.address = "127.0.0.1";
		i.ssl_connection = 0;
		i.local_protocol_name = "lws-ws-raw-raw";

		/* also mark the onward, raw client conn with the proxy_conn */
		i.opaque_user_data = pc;
		/* if it succeeds, set the wsi into the proxy_conn */
		i.pwsi = &pc->wsi_raw;

		if (!lws_client_connect_via_info(&i)) {
			lwsl_warn("%s: onward connection failed\n", __func__);
			return -1; /* hang up on the ws client, triggering
				    * _CLOSE flow */
		}

		break;

	case LWS_CALLBACK_CLOSED:
		/*
		 * Clean up any pending messages to us that are never going
		 * to get delivered now, we are in the middle of closing
		 */
		lws_dll2_foreach_safe(&pc->pending_msg_to_ws, NULL,
				      proxy_ws_raw_msg_destroy);

		/*
		 * Remove our pointer from the proxy_conn... we are about to
		 * be destroyed.
		 */
		pc->wsi_ws = NULL;
		lws_set_opaque_user_data(wsi, NULL);

		if (!pc->wsi_raw) {
			/*
			 * The onward raw conn either never got started or is
			 * already closed... then we are the last guy still
			 * holding on to the proxy_conn... and we're going away
			 * so let's destroy it
			 */

			free(pc);
			break;
		}

		/*
		 * Onward conn still alive...
		 * does he have stuff left to deliver?
		 */
		if (pc->pending_msg_to_raw.count) {
			/*
			 * Yes, let him get on with trying to send
			 * the remaining pieces... but put a time limit
			 * on how hard he will try now the ws part is
			 * disappearing... give him 3s
			 */
			lws_set_timeout(pc->wsi_raw,
				PENDING_TIMEOUT_KILLED_BY_PROXY_CLIENT_CLOSE, 3);
			break;
		}
		/*
		 * Onward raw client conn doesn't have anything left
		 * to do, let's close him right after this, he will take care to
		 * destroy the proxy_conn when he goes down after he sees we
		 * have already been closed
		 */

		lws_wsi_close(pc->wsi_raw, LWS_TO_KILL_ASYNC);
		break;

	case LWS_CALLBACK_SERVER_WRITEABLE:
		if (!pc || !pc->pending_msg_to_ws.count)
			break;

		msg = lws_container_of(pc->pending_msg_to_ws.head,
				       proxy_msg_t, list);
		data = (uint8_t *)&msg[1] + LWS_PRE;

		/* notice we allowed for LWS_PRE in the payload already */
		m = lws_write(wsi, data, msg->len, LWS_WRITE_TEXT);
		a = (int)msg->len;
		lws_dll2_remove(&msg->list);
		free(msg);

		if (m < a) {
			lwsl_err("ERROR %d writing to ws\n", m);
			return -1;
		}

		/*
		 * If more to do...
		 */
		if (pc->pending_msg_to_ws.count)
			lws_callback_on_writable(wsi);
		break;

	case LWS_CALLBACK_RECEIVE:
		if (!pc || !pc->wsi_raw)
			break;

		/* notice we over-allocate by LWS_PRE + rx len */
		msg = (proxy_msg_t *)malloc(sizeof(*msg) + LWS_PRE + len);
		data = (uint8_t *)&msg[1] + LWS_PRE;

		if (!msg) {
			lwsl_user("OOM: dropping\n");
			break;
		}

		memset(msg, 0, sizeof(*msg));
		msg->len = len;
		memcpy(data, in, len);

		/* add us on to the list of packets to send to the onward conn */
		lws_dll2_add_tail(&msg->list, &pc->pending_msg_to_raw);

		/* ask to send on the onward proxy client conn */
		lws_callback_on_writable(pc->wsi_raw);
		break;

	default:
		break;
	}

	return 0;
}

/*
 * Then the onward, raw client side
 */

static int
callback_proxy_raw_client(struct lws *wsi, enum lws_callback_reasons reason,
			  void *user, void *in, size_t len)
{
	proxy_conn_t *pc = (proxy_conn_t *)lws_get_opaque_user_data(wsi);
	proxy_msg_t *msg;
	uint8_t *data;
	int m, a;

	switch (reason) {
	case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
		lwsl_warn("%s: onward raw connection failed\n", __func__);
		pc->wsi_raw = NULL;
		break;

	case LWS_CALLBACK_RAW_ADOPT:
		lwsl_user("LWS_CALLBACK_RAW_ADOPT\n");
		pc->wsi_raw = wsi;
		lws_callback_on_writable(wsi);
		break;

	case LWS_CALLBACK_RAW_CLOSE:
		lwsl_user("LWS_CALLBACK_RAW_CLOSE\n");
		/*
		 * Clean up any pending messages to us that are never going
		 * to get delivered now, we are in the middle of closing
		 */
		lws_dll2_foreach_safe(&pc->pending_msg_to_raw, NULL,
				      proxy_ws_raw_msg_destroy);

		/*
		 * Remove our pointer from the proxy_conn... we are about to
		 * be destroyed.
		 */
		pc->wsi_raw = NULL;
		lws_set_opaque_user_data(wsi, NULL);

		if (!pc->wsi_ws) {
			/*
			 * The original ws conn is already closed... then we are
			 * the last guy still holding on to the proxy_conn...
			 * and we're going away, so let's destroy it
			 */

			free(pc);
			break;
		}

		/*
		 * Original ws conn still alive...
		 * does he have stuff left to deliver?
		 */
		if (pc->pending_msg_to_ws.count) {
			/*
			 * Yes, let him get on with trying to send
			 * the remaining pieces... but put a time limit
			 * on how hard he will try now the raw part is
			 * disappearing... give him 3s
			 */
			lws_set_timeout(pc->wsi_ws,
				PENDING_TIMEOUT_KILLED_BY_PROXY_CLIENT_CLOSE, 3);
			break;
		}
		/*
		 * Original ws client conn doesn't have anything left
		 * to do, let's close him right after this, he will take care to
		 * destroy the proxy_conn when he goes down after he sees we
		 * have already been closed
		 */

		lws_wsi_close(pc->wsi_ws, LWS_TO_KILL_ASYNC);
		break;

	case LWS_CALLBACK_RAW_RX:
		lwsl_user("LWS_CALLBACK_RAW_RX (%d)\n", (int)len);
		if (!pc || !pc->wsi_ws)
			break;

		/* notice we over-allocate by LWS_PRE + rx len */
		msg = (proxy_msg_t *)malloc(sizeof(*msg) + LWS_PRE + len);
		data = (uint8_t *)&msg[1] + LWS_PRE;

		if (!msg) {
			lwsl_user("OOM: dropping\n");
			break;
		}

		memset(msg, 0, sizeof(*msg));
		msg->len = len;
		memcpy(data, in, len);

		/* add us on to the list of packets to send to the onward conn */
		lws_dll2_add_tail(&msg->list, &pc->pending_msg_to_ws);

		/* ask to send on the onward proxy client conn */
		lws_callback_on_writable(pc->wsi_ws);
		break;

	case LWS_CALLBACK_RAW_WRITEABLE:
		lwsl_user("LWS_CALLBACK_RAW_WRITEABLE\n");
		if (!pc || !pc->pending_msg_to_raw.count)
			break;

		msg = lws_container_of(pc->pending_msg_to_raw.head,
				       proxy_msg_t, list);
		data = (uint8_t *)&msg[1] + LWS_PRE;

		/* notice we allowed for LWS_PRE in the payload already */
		m = lws_write(wsi, data, msg->len, LWS_WRITE_TEXT);
		a = (int)msg->len;
		lws_dll2_remove(&msg->list);
		free(msg);

		if (m < a) {
			lwsl_err("ERROR %d writing to raw\n", m);
			return -1;
		}

		/*
		 * If more to do...
		 */
		if (pc->pending_msg_to_raw.count)
			lws_callback_on_writable(wsi);
		break;
	default:
		break;
	}

	return 0;
}

static struct lws_protocols protocols[] = {
	{ "http", lws_callback_http_dummy, 0, 0, 0, NULL, 0 },
	{ "lws-ws-raw-ws", callback_proxy_ws_server, 0, 1024, 0, NULL, 0 },
	{ "lws-ws-raw-raw", callback_proxy_raw_client, 0, 1024, 0, NULL, 0 },
	LWS_PROTOCOL_LIST_TERM
};

static const lws_retry_bo_t retry = {
	.secs_since_valid_ping = 3,
	.secs_since_valid_hangup = 10,
};

static int interrupted;

static const struct lws_http_mount mount = {
	/* .mount_next */		NULL,		/* linked-list "next" */
	/* .mountpoint */		"/",		/* mountpoint URL */
	/* .origin */			"./mount-origin",  /* serve from dir */
	/* .def */			"index.html",	/* default filename */
	/* .protocol */			NULL,
	/* .cgienv */			NULL,
	/* .extra_mimetypes */		NULL,
	/* .interpret */		NULL,
	/* .cgi_timeout */		0,
	/* .cache_max_age */		0,
	/* .auth_mask */		0,
	/* .cache_reusable */		0,
	/* .cache_revalidate */		0,
	/* .cache_intermediaries */	0,
	/* .origin_protocol */		LWSMPRO_FILE,	/* files in a dir */
	/* .mountpoint_len */		1,		/* char count */
	/* .basic_auth_login_file */	NULL,
};

void sigint_handler(int sig)
{
	interrupted = 1;
}

int main(int argc, const char **argv)
{
	struct lws_context_creation_info info;
	struct lws_context *context;
	const char *p;
	int n = 0, logs = LLL_USER | LLL_ERR | LLL_WARN | LLL_NOTICE
			/* for LLL_ verbosity above NOTICE to be built into lws,
			 * lws must have been configured and built with
			 * -DCMAKE_BUILD_TYPE=DEBUG instead of =RELEASE */
			/* | LLL_INFO */ /* | LLL_PARSER */ /* | LLL_HEADER */
			/* | LLL_EXT */ /* | LLL_CLIENT */ /* | LLL_LATENCY */
			/* | LLL_DEBUG */;

	signal(SIGINT, sigint_handler);

	if ((p = lws_cmdline_option(argc, argv, "-d")))
		logs = atoi(p);

	lws_set_log_level(logs, NULL);
	lwsl_user("LWS minimal ws-raw proxy | visit http://localhost:7681 (-s = use TLS / https)\n");

	memset(&info, 0, sizeof info); /* otherwise uninitialized garbage */
	info.port = 7681;
	info.mounts = &mount;
	info.protocols = protocols;
	info.vhost_name = "localhost";
	info.options =
		LWS_SERVER_OPTION_HTTP_HEADERS_SECURITY_BEST_PRACTICES_ENFORCE;

#if defined(LWS_WITH_TLS)
	if (lws_cmdline_option(argc, argv, "-s")) {
		lwsl_user("Server using TLS\n");
		info.options |= LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;
		info.ssl_cert_filepath = "localhost-100y.cert";
		info.ssl_private_key_filepath = "localhost-100y.key";
	}
#endif

	if (lws_cmdline_option(argc, argv, "-h"))
		info.options |= LWS_SERVER_OPTION_VHOST_UPG_STRICT_HOST_CHECK;

	if (lws_cmdline_option(argc, argv, "-v"))
		info.retry_and_idle_policy = &retry;

	context = lws_create_context(&info);
	if (!context) {
		lwsl_err("lws init failed\n");
		return 1;
	}

	while (n >= 0 && !interrupted)
		n = lws_service(context, 0);

	lws_context_destroy(context);

	return 0;
}
