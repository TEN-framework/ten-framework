/*
 * lws-minimal-dbus-server
 *
 * Written in 2010-2019 by Andy Green <andy@warmcat.com>
 *
 * This file is made available under the Creative Commons CC0 1.0
 * Universal Public Domain Dedication.
 *
 * This demonstrates a minimal session dbus server that uses the lws event loop,
 * making it possible to integrate it with other lws features.
 *
 * The dbus server parts are based on "Sample code illustrating basic use of
 * D-BUS" (presumed Public Domain) here:
 *
 * https://github.com/fbuihuu/samples-dbus/blob/master/dbus-server.c
 */

#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>

#include <libwebsockets.h>
#include <libwebsockets/lws-dbus.h>

static struct lws_context *context;
static const char *version = "0.1";
static int interrupted;
static struct lws_dbus_ctx dbus_ctx, ctx_listener;
static char session;

#define THIS_INTERFACE	 "org.libwebsockets.test"
#define THIS_OBJECT	 "/org/libwebsockets/test"
#define THIS_BUSNAME	 "org.libwebsockets.test"

#define THIS_LISTEN_PATH "unix:abstract=org.libwebsockets.test"

static const char *
server_introspection_xml =
	DBUS_INTROSPECT_1_0_XML_DOCTYPE_DECL_NODE
	"<node>\n"
	"  <interface name='" DBUS_INTERFACE_INTROSPECTABLE "'>\n"
	"    <method name='Introspect'>\n"
	"      <arg name='data' type='s' direction='out' />\n"
	"    </method>\n"
	"  </interface>\n"

	"  <interface name='" DBUS_INTERFACE_PROPERTIES "'>\n"
	"    <method name='Get'>\n"
	"      <arg name='interface' type='s' direction='in' />\n"
	"      <arg name='property'  type='s' direction='in' />\n"
	"      <arg name='value'     type='s' direction='out' />\n"
	"    </method>\n"
	"    <method name='GetAll'>\n"
	"      <arg name='interface'  type='s'     direction='in'/>\n"
	"      <arg name='properties' type='a{sv}' direction='out'/>\n"
	"    </method>\n"
	"  </interface>\n"

	"  <interface name='"THIS_INTERFACE"'>\n"
	"    <property name='Version' type='s' access='read' />\n"
	"    <method name='Ping' >\n"
	"      <arg type='s' direction='out' />\n"
	"    </method>\n"
	"    <method name='Echo'>\n"
	"      <arg name='string' direction='in' type='s'/>\n"
	"      <arg type='s' direction='out' />\n"
	"    </method>\n"
	"    <method name='EmitSignal'>\n"
	"    </method>\n"
	"    <method name='Quit'>\n"
	"    </method>\n"
	"    <signal name='OnEmitSignal'>\n"
	"    </signal>"
	"  </interface>\n"

	"</node>\n";

static DBusHandlerResult
dmh_introspect(DBusConnection *c, DBusMessage *m, DBusMessage **reply, void *d)
{
	dbus_message_append_args(*reply, DBUS_TYPE_STRING,
				 &server_introspection_xml, DBUS_TYPE_INVALID);

	return DBUS_HANDLER_RESULT_HANDLED;
}

static DBusHandlerResult
dmh_get(DBusConnection *c, DBusMessage *m, DBusMessage **reply, void *d)
{
	const char *interface, *property;
	DBusError err;

	dbus_error_init(&err);

	if (!dbus_message_get_args(m, &err, DBUS_TYPE_STRING, &interface,
					    DBUS_TYPE_STRING, &property,
					    DBUS_TYPE_INVALID)) {
		dbus_message_unref(*reply);
		*reply = dbus_message_new_error(m, err.name, err.message);
		dbus_error_free(&err);

		return DBUS_HANDLER_RESULT_HANDLED;
	}

	if (strcmp(property, "Version")) /* Unknown property */
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

	dbus_message_append_args(*reply, DBUS_TYPE_STRING, &version,
				 DBUS_TYPE_INVALID);

	return DBUS_HANDLER_RESULT_HANDLED;
}

static DBusHandlerResult
dmh_getall(DBusConnection *c, DBusMessage *m, DBusMessage **reply, void *d)
{
	DBusMessageIter arr, di, iter, va;
	const char *property = "Version";

	dbus_message_iter_init_append(*reply, &iter);
	dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY, "{sv}", &arr);

	/* Append all properties name/value pairs */
	dbus_message_iter_open_container(&arr, DBUS_TYPE_DICT_ENTRY, NULL, &di);
	dbus_message_iter_append_basic(&di, DBUS_TYPE_STRING, &property);
	dbus_message_iter_open_container(&di, DBUS_TYPE_VARIANT, "s", &va);
	dbus_message_iter_append_basic(&va, DBUS_TYPE_STRING, &version);
	dbus_message_iter_close_container(&di, &va);
	dbus_message_iter_close_container(&arr, &di);

	dbus_message_iter_close_container(&iter, &arr);

	return DBUS_HANDLER_RESULT_HANDLED;
}

static DBusHandlerResult
dmh_ping(DBusConnection *c, DBusMessage *m, DBusMessage **reply, void *d)
{
	const char *pong = "Pong";

	dbus_message_append_args(*reply, DBUS_TYPE_STRING, &pong,
					 DBUS_TYPE_INVALID);

	return DBUS_HANDLER_RESULT_HANDLED;
}

static DBusHandlerResult
dmh_echo(DBusConnection *c, DBusMessage *m, DBusMessage **reply, void *d)
{
	const char *msg;
	DBusError err;

	dbus_error_init(&err);

	if (!dbus_message_get_args(m, &err, DBUS_TYPE_STRING,
				   &msg, DBUS_TYPE_INVALID)) {
		dbus_message_unref(*reply);
		*reply = dbus_message_new_error(m, err.name, err.message);
		dbus_error_free(&err);

		return DBUS_HANDLER_RESULT_HANDLED;
	}

	dbus_message_append_args(*reply, DBUS_TYPE_STRING, &msg,
					 DBUS_TYPE_INVALID);

	return DBUS_HANDLER_RESULT_HANDLED;
}

static DBusHandlerResult
dmh_emit_signal(DBusConnection *c, DBusMessage *m, DBusMessage **reply, void *d)
{
	DBusMessage *r = dbus_message_new_signal(THIS_OBJECT, THIS_INTERFACE,
					         "OnEmitSignal");

	if (!r)
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

	if (!dbus_connection_send(c, r, NULL))
		return DBUS_HANDLER_RESULT_NEED_MEMORY;

	/* and send the original empty reply after */

	return DBUS_HANDLER_RESULT_HANDLED;
}

static DBusHandlerResult
dmh_emit_quit(DBusConnection *c, DBusMessage *m, DBusMessage **reply, void *d)
{
	interrupted = 1;

	return DBUS_HANDLER_RESULT_HANDLED;
}

struct lws_dbus_methods {
	const char *inter;
	const char *call;
	lws_dbus_message_handler handler;
} meths[] = {
	{ DBUS_INTERFACE_INTROSPECTABLE, "Introspect",	dmh_introspect	},
	{ DBUS_INTERFACE_PROPERTIES,	 "Get",		dmh_get		},
	{ DBUS_INTERFACE_PROPERTIES,	 "GetAll",	dmh_getall	},
	{ THIS_INTERFACE,		 "Ping",	dmh_ping	},
	{ THIS_INTERFACE,		 "Echo",	dmh_echo	},
	{ THIS_INTERFACE,		 "EmitSignal",	dmh_emit_signal },
	{ THIS_INTERFACE,		 "Quit",	dmh_emit_quit	},
};

static DBusHandlerResult
server_message_handler(DBusConnection *conn, DBusMessage *message, void *data)
{
	struct lws_dbus_methods *mp = meths;
	DBusHandlerResult result;
        DBusMessage *reply = NULL;
	size_t n;

	lwsl_info("%s: Got D-Bus request: %s.%s on %s\n", __func__,
		  dbus_message_get_interface(message),
		  dbus_message_get_member(message),
		  dbus_message_get_path(message));

	for (n = 0; n < LWS_ARRAY_SIZE(meths); n++) {
		if (dbus_message_is_method_call(message, mp->inter, mp->call)) {
			reply = dbus_message_new_method_return(message);
			if (!reply)
				return DBUS_HANDLER_RESULT_NEED_MEMORY;

			result = mp->handler(conn, message, &reply, data);

			if (result == DBUS_HANDLER_RESULT_HANDLED &&
			    !dbus_connection_send(conn, reply, NULL))
				result = DBUS_HANDLER_RESULT_NEED_MEMORY;

			dbus_message_unref(reply);

			return result;
		}

		mp++;
	}

	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

static const DBusObjectPathVTable server_vtable = {
	.message_function = server_message_handler
};

static void
destroy_dbus_server_conn(struct lws_dbus_ctx *ctx)
{
	if (!ctx->conn)
		return;

	lwsl_notice("%s\n", __func__);

	dbus_connection_unregister_object_path(ctx->conn, THIS_OBJECT);
	lws_dll2_remove(&ctx->next);
	dbus_connection_unref(ctx->conn);
}

static void
cb_closing(struct lws_dbus_ctx *ctx)
{
	lwsl_err("%s: closing\n", __func__);
	destroy_dbus_server_conn(ctx);

	free(ctx);
}


static void
new_conn(DBusServer *server, DBusConnection *conn, void *data)
{
	struct lws_dbus_ctx *conn_ctx, *ctx = (struct lws_dbus_ctx *)data;

	lwsl_notice("%s: vh %s\n", __func__, lws_get_vhost_name(ctx->vh));

	conn_ctx = malloc(sizeof(*conn_ctx));
	if (!conn_ctx)
		return;

	memset(conn_ctx, 0, sizeof(*conn_ctx));

	conn_ctx->tsi = ctx->tsi;
	conn_ctx->vh = ctx->vh;
	conn_ctx->conn = conn;

	if (lws_dbus_connection_setup(conn_ctx, conn, cb_closing)) {
		lwsl_err("%s: connection bind to lws failed\n", __func__);
		goto bail;
	}

	if (!dbus_connection_register_object_path(conn, THIS_OBJECT,
						  &server_vtable, conn_ctx)) {
		lwsl_err("%s: Failed to register object path\n", __func__);
		goto bail;
	}

	lws_dll2_add_head(&conn_ctx->next, &ctx->owner);

	/* we take on responsibility for explicit close / unref with this... */
	dbus_connection_ref(conn);

	return;

bail:
	free(conn_ctx);
}

static int
create_dbus_listener(const char *ads)
{
	DBusError e;

        dbus_error_init(&e);

	if (!lws_dbus_server_listen(&ctx_listener, ads, &e, new_conn)) {
		lwsl_err("%s: failed\n", __func__);
		dbus_error_free(&e);

		return 1;
	}

	return 0;
}

static int
create_dbus_server_conn(struct lws_dbus_ctx *ctx, DBusBusType type)
{
	DBusError err;
	int rv;

        dbus_error_init(&err);

	/* connect to the daemon bus */
	ctx->conn = dbus_bus_get(type, &err);
	if (!ctx->conn) {
		lwsl_err("%s: Failed to get a session DBus connection: %s\n",
			 __func__, err.message);
		goto fail;
	}

	/*
	 * by default dbus will call exit() when this connection closes...
	 * we have to shut down other things cleanly, so disable that
	 */
	dbus_connection_set_exit_on_disconnect(ctx->conn, 0);

	rv = dbus_bus_request_name(ctx->conn, THIS_BUSNAME,
				   DBUS_NAME_FLAG_REPLACE_EXISTING, &err);
	if (rv != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER) {
		lwsl_err("%s: Failed to request name on bus: %s\n",
			 __func__, err.message);
		goto fail;
	}

	if (!dbus_connection_register_object_path(ctx->conn, THIS_OBJECT,
						  &server_vtable, NULL)) {
		lwsl_err("%s: Failed to register object path for TestObject\n",
			 __func__);
		dbus_bus_release_name(ctx->conn, THIS_BUSNAME, &err);
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

	return 0;

fail:
	dbus_error_free(&err);

	return 1;
}

/*
 * Cleanly release the connection
 */

static void
destroy_dbus_server_listener(struct lws_dbus_ctx *ctx)
{
	dbus_server_disconnect(ctx->dbs);

	lws_start_foreach_dll_safe(struct lws_dll2 *, rdt, nx,
				   ctx->owner.head) {
		struct lws_dbus_ctx *r =
			lws_container_of(rdt, struct lws_dbus_ctx, next);

		dbus_connection_close(r->conn);
		dbus_connection_unref(r->conn);
		free(r);
	} lws_end_foreach_dll_safe(rdt, nx);

	dbus_server_unref(ctx->dbs);
}

/*
 * DBUS can send messages outside the usual client-initiated RPC concept.
 *
 * You can receive them using a message filter.
 */

static void
spam_connected_clients(struct lws_dbus_ctx *ctx)
{

	/* send connected clients an unsolicited message */

	lws_start_foreach_dll_safe(struct lws_dll2 *, rdt, nx,
				   ctx->owner.head) {
		struct lws_dbus_ctx *r =
			lws_container_of(rdt, struct lws_dbus_ctx, next);


		DBusMessage *msg;
		const char *payload = "Unsolicited message";

		msg = dbus_message_new(DBUS_NUM_MESSAGE_TYPES + 1);
		if (!msg) {
			lwsl_err("%s: new message failed\n", __func__);
		}

		dbus_message_append_args(msg, DBUS_TYPE_STRING, &payload,
						 DBUS_TYPE_INVALID);
		if (!dbus_connection_send(r->conn, msg, NULL)) {
			lwsl_err("%s: unable to send\n", __func__);
		}

		lwsl_notice("%s\n", __func__);

		dbus_message_unref(msg);

	} lws_end_foreach_dll_safe(rdt, nx);

}


void sigint_handler(int sig)
{
	interrupted = 1;
}

int main(int argc, const char **argv)
{
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
	lwsl_user("LWS minimal DBUS server\n");

	memset(&info, 0, sizeof info); /* otherwise uninitialized garbage */
	info.options = LWS_SERVER_OPTION_EXPLICIT_VHOSTS;
	context = lws_create_context(&info);
	if (!context) {
		lwsl_err("lws init failed\n");
		return 1;
	}

	info.options |=
		LWS_SERVER_OPTION_HTTP_HEADERS_SECURITY_BEST_PRACTICES_ENFORCE;

	dbus_ctx.tsi = 0;
	ctx_listener.tsi = 0;
	ctx_listener.vh = dbus_ctx.vh = lws_create_vhost(context, &info);
	if (!dbus_ctx.vh)
		goto bail;

	session = !!lws_cmdline_option(argc, argv, "--session");

	if (session) {
		/* create the dbus connection, loosely bound to our lws vhost */

		if (create_dbus_server_conn(&dbus_ctx, DBUS_BUS_SESSION))
			goto bail;
	} else {
		if (create_dbus_listener(THIS_LISTEN_PATH)) {
			lwsl_err("%s: create_dbus_listener failed\n", __func__);
			goto bail;
		}
	}

	/* lws event loop (default poll one) */

	while (n >= 0 && !interrupted) {
		if (!session)
			spam_connected_clients(&ctx_listener);
		n = lws_service(context, 0);
	}

	if (session)
		destroy_dbus_server_conn(&dbus_ctx);
	else
		destroy_dbus_server_listener(&ctx_listener);

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
