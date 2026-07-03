/*
 * lws-minimal-dbus-ws-proxy-testclient
 *
 * Written in 2010-2019 by Andy Green <andy@warmcat.com>
 *
 * This file is made available under the Creative Commons CC0 1.0
 * Universal Public Domain Dedication.
 *
 * This acts as a test client over DBUS, opening a session with
 * minimal-dbus-ws-proxy and sending and receiving data on the libwebsockets
 * mirror demo page.
 */

#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>

#include <libwebsockets.h>
#include <libwebsockets/lws-dbus.h>

/*
 * These are the various states our connection can be in, both with regards
 * to the direct connection to the proxy, and the state of the onward ws
 * connection the proxy opens at our request.
 */

enum lws_dbus_client_state {
	LDCS_NOTHING,		  /* no connection yet */
	LDCS_CONN,		  /* conn to proxy */
	LDCS_CONN_WAITING_ONWARD, /* conn to proxy, awaiting proxied conn */
	LDCS_CONN_ONWARD,	  /* conn to proxy and onward conn OK */
	LDCS_CONN_CLOSED,	  /* conn to proxy but onward conn closed */
	LDCS_CLOSED,		  /* connection to proxy is closed */
};

/*
 * our expanded dbus context
 */

struct lws_dbus_ctx_wsproxy_client {
	struct lws_dbus_ctx ctx;

	lws_sorted_usec_list_t sul;

	enum lws_dbus_client_state state;
};

static struct lws_dbus_ctx_wsproxy_client *dbus_ctx;
static struct lws_context *context;
static int interrupted, autoexit_budget = -1, count_rx, count_tx;

#define THIS_INTERFACE	 "org.libwebsockets.wsclientproxy"
#define THIS_OBJECT	 "/org/libwebsockets/wsclientproxy"
#define THIS_BUSNAME	 "org.libwebsockets.wsclientproxy"

#define THIS_LISTEN_PATH "unix:abstract=org.libwebsockets.wsclientproxy"

static void
state_transition(struct lws_dbus_ctx_wsproxy_client *dcwc,
		 enum lws_dbus_client_state state)
{
	lwsl_notice("%s: %p: from state %d -> %d\n", __func__,
		    dcwc,dcwc->state, state);
	dcwc->state = state;
}

static DBusHandlerResult
filter(DBusConnection *conn, DBusMessage *message, void *data)
{
	struct lws_dbus_ctx_wsproxy_client *dcwc =
			(struct lws_dbus_ctx_wsproxy_client *)data;
	const char *str;

	if (!dbus_message_get_args(message, NULL,
				   DBUS_TYPE_STRING, &str,
				   DBUS_TYPE_INVALID))
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

	/* received ws data */

	if (dbus_message_is_signal(message, THIS_INTERFACE, "Receive")) {
		lwsl_user("%s: Received '%s'\n", __func__, str);
		count_rx++;
	}

	/* proxy ws connection failed */

	if (dbus_message_is_signal(message, THIS_INTERFACE, "Status") &&
	    !strcmp(str, "ws client connection error"))
		state_transition(dcwc, LDCS_CONN_CLOSED);

	/* proxy ws connection succeeded */

	if (dbus_message_is_signal(message, THIS_INTERFACE, "Status") &&
	    !strcmp(str, "ws client connection established"))
		state_transition(dcwc, LDCS_CONN_ONWARD);

	/* proxy ws connection has closed */

	if (dbus_message_is_signal(message, THIS_INTERFACE, "Status") &&
	    !strcmp(str, "ws client connection closed"))
		state_transition(dcwc, LDCS_CONN_CLOSED);

	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

static void
destroy_dbus_client_conn(struct lws_dbus_ctx_wsproxy_client **pdcwc)
{
	struct lws_dbus_ctx_wsproxy_client *dcwc = *pdcwc;

	if (!dcwc || !dcwc->ctx.conn)
		return;

	lwsl_notice("%s\n", __func__);

	dbus_connection_remove_filter(dcwc->ctx.conn, filter, &dcwc->ctx);
	dbus_connection_close(dcwc->ctx.conn);
	dbus_connection_unref(dcwc->ctx.conn);

	free(dcwc);

	*pdcwc = NULL;
}

/*
 * This callback is coming when lws has noticed the fd took a POLLHUP.  The
 * ctx has effectively gone out of scope before this, and the connection can
 * be cleaned up and the ctx freed.
 */

static void
cb_closing(struct lws_dbus_ctx *ctx)
{
	struct lws_dbus_ctx_wsproxy_client *dcwc =
			(struct lws_dbus_ctx_wsproxy_client *)ctx;

	lwsl_err("%s: closing\n", __func__);

	if (dcwc == dbus_ctx)
		dbus_ctx = NULL;

	destroy_dbus_client_conn(&dcwc);

	interrupted = 1;
}

static struct lws_dbus_ctx_wsproxy_client *
create_dbus_client_conn(struct lws_vhost *vh, int tsi, const char *ads)
{
	struct lws_dbus_ctx_wsproxy_client *dcwc;
	DBusError e;

	dcwc = malloc(sizeof(*dcwc));
	if (!dcwc)
		return NULL;

	memset(dcwc, 0, sizeof(*dcwc));

	dcwc->state = LDCS_NOTHING;
	dcwc->ctx.vh = vh;
	dcwc->ctx.tsi = tsi;

        dbus_error_init(&e);

        lwsl_user("%s: connecting to '%s'\n", __func__, ads);
#if 1
	/* connect to our daemon bus */

        dcwc->ctx.conn = dbus_connection_open_private(ads, &e);
	if (!dcwc->ctx.conn) {
		lwsl_err("%s: Failed to connect: %s\n",
			 __func__, e.message);
		goto fail;
	}
#else
	/* connect to the SYSTEM bus */

	dcwc->ctx.conn = dbus_bus_get(DBUS_BUS_SYSTEM, &e);
	if (!dcwc->ctx.conn) {
		lwsl_err("%s: Failed to get a session DBus connection: %s\n",
			 __func__, e.message);
		goto fail;
	}
#endif
	dbus_connection_set_exit_on_disconnect(dcwc->ctx.conn, 0);

	if (!dbus_connection_add_filter(dcwc->ctx.conn, filter,
					&dcwc->ctx, NULL)) {
		lwsl_err("%s: Failed to add filter\n", __func__);
		goto fail;
	}

	/*
	 * This is the part that binds the connection to lws watcher and
	 * timeout handling provided by lws
	 */

	if (lws_dbus_connection_setup(&dcwc->ctx, dcwc->ctx.conn, cb_closing)) {
		lwsl_err("%s: connection bind to lws failed\n", __func__);
		goto fail;
	}

	state_transition(dcwc, LDCS_CONN);

	lwsl_notice("%s: created OK\n", __func__);

	return dcwc;

fail:
	dbus_error_free(&e);

	free(dcwc);

	return NULL;
}


void sigint_handler(int sig)
{
	interrupted = 1;
}

/*
 * This gets called if we timed out waiting for the dbus server reply, or the
 * reply arrived.
 */

static void
pending_call_notify(DBusPendingCall *pending, void *data)
{
	const char *payload;
	DBusMessage *msg;

	if (!dbus_pending_call_get_completed(pending)) {
		lwsl_err("%s: timed out waiting for reply\n", __func__);

		goto bail;
	}

	msg = dbus_pending_call_steal_reply(pending);
	if (!msg)
		goto bail;

	if (!dbus_message_get_args(msg, NULL, DBUS_TYPE_STRING, &payload,
					      DBUS_TYPE_INVALID)) {
		goto bail1;
	}

	lwsl_user("%s: received '%s'\n", __func__, payload);

bail1:
	dbus_message_unref(msg);
bail:
	dbus_pending_call_unref(pending);
}

static int
remote_method_call(struct lws_dbus_ctx_wsproxy_client *dcwc)
{
	char _uri[96];
	const char *subprotocol = "lws-mirror-protocol", *uri = _uri;
	DBusMessage *msg;
	int ret = 1;

	/*
	 * make our own private mirror session... because others may run this
	 * at the same time against libwebsockets.org... as happened 2019-03-14
	 * and broke travis tests :-)
	 */

	lws_snprintf(_uri, sizeof(_uri), "wss://libwebsockets.org/?mirror=dbt-%d",
			(int)getpid());

	msg = dbus_message_new_method_call(
			/* dest */	  THIS_BUSNAME,
			/* object-path */ THIS_OBJECT,
			/* interface */   THIS_INTERFACE,
			/* method */	  "Connect");
	if (!msg)
		return 1;

	if (!dbus_message_append_args(msg, DBUS_TYPE_STRING, &uri,
					   DBUS_TYPE_STRING, &subprotocol,
					   DBUS_TYPE_INVALID))
		goto bail;

	lwsl_user("%s: requesting proxy connection %s %s\n", __func__,
			uri, subprotocol);

	if (!dbus_connection_send_with_reply(dcwc->ctx.conn, msg, &dcwc->ctx.pc,
					     DBUS_TIMEOUT_USE_DEFAULT)) {
		lwsl_err("%s: unable to send\n", __func__);

		goto bail;
	}

	dbus_pending_call_set_notify(dcwc->ctx.pc, pending_call_notify,
				     &dcwc->ctx, NULL);

	state_transition(dcwc, LDCS_CONN_WAITING_ONWARD);

	ret = 0;

bail:
	dbus_message_unref(msg);

	return ret;
}

static void
sul_timer(struct lws_sorted_usec_list *sul)
{
	char payload[64];
	const char *ws_pkt = payload;
	DBusMessage *msg;

	if (!dbus_ctx || dbus_ctx->state != LDCS_CONN_ONWARD)
		goto again;

	if (autoexit_budget > 0) {
		if (!--autoexit_budget) {
			lwsl_notice("reached autoexit budget\n");
			interrupted = 1;
			return;
		}
	}

	msg = dbus_message_new_method_call(THIS_BUSNAME, THIS_OBJECT,
					   THIS_INTERFACE, "Send");
	if (!msg)
		goto again;

	lws_snprintf(payload, sizeof(payload), "d #%06X %d %d %d %d;",
		     rand() & 0xffffff, rand() % 480, rand() % 300,
		     rand() % 480, rand() % 300);

	if (!dbus_message_append_args(msg, DBUS_TYPE_STRING, &ws_pkt,
					   DBUS_TYPE_INVALID)) {
		dbus_message_unref(msg);
		goto again;
	}

	if (!dbus_connection_send_with_reply(dbus_ctx->ctx.conn, msg,
					     &dbus_ctx->ctx.pc,
					    DBUS_TIMEOUT_USE_DEFAULT)) {
		lwsl_err("%s: unable to send\n", __func__);
		dbus_message_unref(msg);
		goto again;
	}

	dbus_message_unref(msg);
	dbus_pending_call_set_notify(dbus_ctx->ctx.pc,
				     pending_call_notify,
				     &dbus_ctx->ctx, NULL);
	count_tx++;

again:
	lws_sul_schedule(context, 0, &dbus_ctx->sul, sul_timer, 2 * LWS_US_PER_SEC);
}

int main(int argc, const char **argv)
{
	struct lws_vhost *vh;
	struct lws_context_creation_info info;
	const char *p;
	int n = 0, logs = LLL_USER | LLL_ERR | LLL_WARN | LLL_NOTICE
			/* for LLL_ verbosity above NOTICE to be built into lws,
			 * lws must have been configured and built with
			 * -DCMAKE_BUILD_TYPE=DEBUG instead of =RELEASE */
			/* | LLL_INFO */ /* | LLL_PARSER */ /* | LLL_HEADER */
			/* | LLL_EXT */ /* | LLL_CLIENT */ /* | LLL_LATENCY */
			/* | LLL_DEBUG */ /* | LLL_THREAD */;

	signal(SIGINT, sigint_handler);

	if ((p = lws_cmdline_option(argc, argv, "-d")))
		logs = atoi(p);

	if ((p = lws_cmdline_option(argc, argv, "-x")))
		autoexit_budget = atoi(p);

	lws_set_log_level(logs, NULL);
	lwsl_user("LWS minimal DBUS ws proxy testclient\n");

	memset(&info, 0, sizeof info); /* otherwise uninitialized garbage */
	info.options = LWS_SERVER_OPTION_EXPLICIT_VHOSTS;
	context = lws_create_context(&info);
	if (!context) {
		lwsl_err("lws init failed\n");
		return 1;
	}

	info.options |=
		LWS_SERVER_OPTION_HTTP_HEADERS_SECURITY_BEST_PRACTICES_ENFORCE;

	vh = lws_create_vhost(context, &info);
	if (!vh)
		goto bail;

	dbus_ctx = create_dbus_client_conn(vh, 0, THIS_LISTEN_PATH);
	if (!dbus_ctx)
		goto bail1;

	lws_sul_schedule(context, 0, &dbus_ctx->sul, sul_timer, LWS_US_PER_SEC);


	if (remote_method_call(dbus_ctx))
		goto bail2;

	/* lws event loop (default poll one) */

	while (n >= 0 && !interrupted)
		n = lws_service(context, 0);

bail2:
	destroy_dbus_client_conn(&dbus_ctx);

bail1:
	/* this is required for valgrind-cleanliness */
	dbus_shutdown();
	lws_context_destroy(context);

	lwsl_notice("Exiting cleanly, rx: %d, tx: %d\n", count_rx, count_tx);

	return 0;

bail:
	lwsl_err("%s: failed to start\n", __func__);
	lws_context_destroy(context);

	return 1;
}
