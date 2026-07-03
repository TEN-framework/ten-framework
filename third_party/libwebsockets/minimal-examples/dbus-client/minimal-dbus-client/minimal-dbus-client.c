/*
 * lws-minimal-dbus-client
 *
 * Written in 2010-2019 by Andy Green <andy@warmcat.com>
 *
 * This file is made available under the Creative Commons CC0 1.0
 * Universal Public Domain Dedication.
 *
 * This demonstrates a minimal session dbus server that uses the lws event loop,
 * making it possible to integrate it with other lws features.
 */

#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>

#include <libwebsockets.h>
#include <libwebsockets/lws-dbus.h>

static struct lws_dbus_ctx *dbus_ctx;
static struct lws_context *context;
static int interrupted;

#define THIS_INTERFACE	 "org.libwebsockets.test"
#define THIS_OBJECT	 "/org/libwebsockets/test"
#define THIS_BUSNAME	 "org.libwebsockets.test"

#define THIS_LISTEN_PATH "unix:abstract=org.libwebsockets.test"


static DBusHandlerResult
client_message_handler(DBusConnection *conn, DBusMessage *message, void *data)
{
	const char *str;

	lwsl_info("%s: Got D-Bus request: %s.%s on %s\n", __func__,
		  dbus_message_get_interface(message),
		  dbus_message_get_member(message),
		  dbus_message_get_path(message));

	if (!dbus_message_get_args(message, NULL,
				   DBUS_TYPE_STRING, &str,
				   DBUS_TYPE_INVALID))
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

	lwsl_notice("%s: '%s'\n", __func__, str);

	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

static void
destroy_dbus_client_conn(struct lws_dbus_ctx *ctx)
{
	if (!ctx || !ctx->conn)
		return;

	lwsl_notice("%s\n", __func__);

	dbus_connection_remove_filter(ctx->conn, client_message_handler, ctx);
	dbus_connection_close(ctx->conn);
	dbus_connection_unref(ctx->conn);

	free(ctx);
}

/*
 * This callback is coming when lws has noticed the fd took a POLLHUP.  The
 * ctx has effectively gone out of scope before this, and the connection can
 * be cleaned up and the ctx freed.
 */

static void
cb_closing(struct lws_dbus_ctx *ctx)
{
	lwsl_err("%s: closing\n", __func__);

	if (ctx == dbus_ctx)
		dbus_ctx = NULL;

	destroy_dbus_client_conn(ctx);
}

static struct lws_dbus_ctx *
create_dbus_client_conn(struct lws_vhost *vh, int tsi, const char *ads)
{
	struct lws_dbus_ctx *ctx;
	DBusError err;

	ctx = malloc(sizeof(*ctx));
	if (!ctx)
		return NULL;

	memset(ctx, 0, sizeof(*ctx));

	ctx->vh = vh;
	ctx->tsi = tsi;

        dbus_error_init(&err);

	/* connect to the daemon bus */
	ctx->conn = dbus_connection_open_private(ads, &err);
	if (!ctx->conn) {
		lwsl_err("%s: Failed to connect: %s\n",
			 __func__, err.message);
		goto fail;
	}

	dbus_connection_set_exit_on_disconnect(ctx->conn, 0);

	if (!dbus_connection_add_filter(ctx->conn, client_message_handler,
					ctx, NULL)) {
		lwsl_err("%s: Failed to add filter\n", __func__);
		goto fail;
	}

	/*
	 * This is the part that binds the connection to lws watcher and
	 * timeout handling provided by lws
	 */

	if (lws_dbus_connection_setup(ctx, ctx->conn, cb_closing)) {
		lwsl_err("%s: connection bind to lws failed\n", __func__);
		goto fail;
	}

	lwsl_notice("%s: created OK\n", __func__);

	return ctx;

fail:
	dbus_error_free(&err);

	free(ctx);

	return NULL;
}


void sigint_handler(int sig)
{
	interrupted = 1;
}

/*
 * This gets called if we timed out waiting for the server reply, or the
 * reply arrived.
 */

static void
pending_call_notify(DBusPendingCall *pending, void *data)
{
	// struct lws_dbus_ctx *ctx = (struct lws_dbus_ctx *)data;
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
remote_method_call(struct lws_dbus_ctx *ctx)
{
	DBusMessage *msg;
	const char *payload = "Hello!";
	int ret = 1;

	msg = dbus_message_new_method_call(
			/* dest */	  THIS_BUSNAME,
			/* object-path */ THIS_OBJECT,
			/* interface */   THIS_INTERFACE,
			/* method */	  "Echo");
	if (!msg)
		return 1;

	if (!dbus_message_append_args(msg, DBUS_TYPE_STRING, &payload,
				      DBUS_TYPE_INVALID))
		goto bail;

	if (!dbus_connection_send_with_reply(ctx->conn, msg,
					     &ctx->pc,
					     DBUS_TIMEOUT_USE_DEFAULT)) {
		lwsl_err("%s: unable to send\n", __func__);

		goto bail;
	}

	dbus_pending_call_set_notify(ctx->pc, pending_call_notify, ctx, NULL);

	ret = 0;

bail:
	dbus_message_unref(msg);

	return ret;
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

	lws_set_log_level(logs, NULL);
	lwsl_user("LWS minimal DBUS client\n");

	memset(&info, 0, sizeof info); /* otherwise uninitialized garbage */
	info.options = LWS_SERVER_OPTION_EXPLICIT_VHOSTS;
	context = lws_create_context(&info);
	if (!context) {
		lwsl_err("lws init failed\n");
		return 1;
	}

	vh = lws_create_vhost(context, &info);
	if (!vh)
		goto bail;

	dbus_ctx = create_dbus_client_conn(vh, 0, THIS_LISTEN_PATH);
	if (!dbus_ctx)
		goto bail1;

	if (remote_method_call(dbus_ctx))
		goto bail2;

	/* lws event loop (default poll one) */

	while (n >= 0 && !interrupted)
		n = lws_service(context, 0);

bail2:
	destroy_dbus_client_conn(dbus_ctx);

bail1:
	/* this is required for valgrind-cleanliness */
	dbus_shutdown();
	lws_context_destroy(context);

	lwsl_notice("Exiting cleanly\n");

	return 0;

bail:
	lwsl_err("%s: failed to start\n", __func__);
	lws_context_destroy(context);

	return 1;
}
