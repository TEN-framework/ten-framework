/*
 * ws protocol handler plugin for dbus ws proxy
 *
 * Written in 2010-2019 by Andy Green <andy@warmcat.com>
 *
 * This file is made available under the Creative Commons CC0 1.0
 * Universal Public Domain Dedication.
 *
 * This proxies outgoing ws client connections on DBUS.  So a DBUS client can
 * reach out and get remote WS payloads in both directions.
 *
 * DEVELOPER NOTE
 *
 * Two worlds, dbus and ws, collide in this file.
 *
 * There main thing keeping it sane is both worlds are running in the same
 * thread and on the same event loop.  Although things may happen completely
 * asynchronously in both worlds, the logical reaction to those events are
 * serialized in a single event loop doing one thing at a time.
 *
 * So while you are servicing an event in the ws world, you can be certain the
 * logical state of any related dbus thing cannot change underneath you, until
 * you return back to the event loop, and vice versa.  So other-world objects
 * can't be freed, other-world handles can't close etc while you are servicing
 * in your world.
 *
 * Since all bets are off what happens next, and in which world, after you
 * return back to the event loop though, an additional rule is needed: worlds
 * must not allocate in objects owned by the other world.  They must generate
 * their own objects in their world and use those for allocations and state.
 *
 * For example in the dbus-world there is a struct lws_dbus_ctx_wsproxy with
 * various state, but he is subject to deletion by events in dbus-world.  If
 * the ws-world stored things there, they are subject to going out of scope
 * at the whim of the dbus connection without the ws world hearing about it and
 * cleanly deallocaing them.  So the ws world must keep his own pss that remains
 * in scope until the ws link closes for allocations from ws-world.
 *
 * In this application there's a point of contact between the worlds, a ring
 * buffer allocated in ws world when the ws connection is established, and
 * deallocated when the ws connection is closed.  The DBUS world needs to put
 * things in this ringbuffer.  But the way lws_ring works, when the message
 * allocated in DBUS world is queued on the ringbuffer, the ringbuffer itself
 * takes responsibility for deallocation.  So there is no problem.
 */

#if !defined (LWS_PLUGIN_STATIC)
#define LWS_DLL
#define LWS_INTERNAL
#include <libwebsockets.h>
#include <libwebsockets/lws-dbus.h>
#endif

#include <string.h>
#include <assert.h>
#include <signal.h>

/*
 * dbus accepted connections create these larger context structs that start
 * with the lws dbus context
 */

struct vhd_dbus_proxy;

struct msg {
	void *payload; /* is malloc'd */
	size_t len;
	char binary;
	char first;
	char final;
};

struct pss_dbus_proxy {
	struct lws_ring *ring_out;
	uint32_t ring_out_tail;
};

struct lws_dbus_ctx_wsproxy {
	struct lws_dbus_ctx ctx;

	struct lws *cwsi;
	struct vhd_dbus_proxy *vhd;
	struct pss_dbus_proxy *pss;
};

struct vhd_dbus_proxy {
	struct lws_context *context;
	struct lws_vhost *vhost;

	/*
	 * Because the listener ctx is composed in the vhd, we can always get a
	 * pointer to the outer vhd from a pointer to ctx_listener inside.
	 */
	struct lws_dbus_ctx ctx_listener;
	struct lws_dbus_ctx_wsproxy dctx;

	const char *dbus_listen_ads;
};

#define THIS_INTERFACE	"org.libwebsockets.wsclientproxy"
#define THIS_OBJECT	"/org/libwebsockets/wsclientproxy"
#define THIS_BUSNAME	"org.libwebsockets.wsclientproxy"
static const char *version = "0.1";

static const char *server_introspection_xml =
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
	"    <method name='Connect' >\n"
	"      <arg name='url' type='s' direction='in' />\n"
	"      <arg name='subprotocol' type='s' direction='in' />\n"
	"    </method>\n"
	"    <method name='Send'>\n"
	"      <arg name='payload' type='s' direction='in' />\n"
	"    </method>\n"
	"    <signal name='Receive'>\n"
	"    </signal>"
	"    <signal name='Status'>\n"
	"    </signal>"
	"  </interface>\n"

	"</node>\n";

static void
destroy_message(void *_msg)
{
	struct msg *msg = _msg;

	free(msg->payload);
	msg->payload = NULL;
	msg->len = 0;
}

/*
 * DBUS WORLD
 */

static DBusHandlerResult
dmh_introspect(DBusConnection *c, DBusMessage *m, DBusMessage **reply, void *d)
{
	dbus_message_append_args(*reply,
				 DBUS_TYPE_STRING, &server_introspection_xml,
				 DBUS_TYPE_INVALID);

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
dmh_connect(DBusConnection *c, DBusMessage *m, DBusMessage **reply, void *d)
{
	struct lws_dbus_ctx_wsproxy *wspctx = (struct lws_dbus_ctx_wsproxy *)d;
	const char *prot = "", *ads = "", *path = "", *baduri = "Bad Uri",
		   *connecting = "Connecting", *failed = "Failed", **pp;
	struct lws_client_connect_info i;
	char host[128], uri_copy[512];
	const char *uri, *subprotocol;
	DBusError err;
	int port = 0;

	dbus_error_init(&err);

	if (!dbus_message_get_args(m, &err, DBUS_TYPE_STRING, &uri,
			 	 	    DBUS_TYPE_STRING, &subprotocol,
					    DBUS_TYPE_INVALID)) {
		dbus_message_unref(*reply);
		*reply = dbus_message_new_error(m, err.name, err.message);
		dbus_error_free(&err);

		return DBUS_HANDLER_RESULT_HANDLED;
	}

	strncpy(uri_copy, uri, sizeof(uri_copy) - 1);
	uri_copy[sizeof(uri_copy) - 1] = '\0';

	if (lws_parse_uri(uri_copy, &prot, &ads, &port, &path)) {
		pp = &baduri;
		goto send_reply;
	}

	lws_snprintf(host, sizeof(host), "%s:%u", ads, port);

	memset(&i, 0, sizeof(i));

	assert(wspctx);
	assert(wspctx->vhd);

	i.context = wspctx->vhd->context;
	i.port = port;
	i.address = ads;
	i.path = path;
	i.host = host;
	i.origin = host;
	i.ssl_connection = !strcmp(prot, "https") || !strcmp(prot, "wss");
	i.vhost = wspctx->ctx.vh;
	i.protocol = subprotocol;
	i.local_protocol_name = "lws-minimal-dbus-wsproxy";
	i.pwsi = &wspctx->cwsi;

	lwsl_user("%s: connecting to %s://%s:%d%s\n", __func__, prot,
			i.address, i.port, i.path);

	if (!lws_client_connect_via_info(&i)) {
		lwsl_notice("%s: client connect failed\n", __func__);
		pp = &failed;
		goto send_reply;
	}

	lws_set_opaque_parent_data(wspctx->cwsi, wspctx);
	lwsl_notice("%s: client connecting...\n", __func__);
	pp = &connecting;

send_reply:
	dbus_message_append_args(*reply, DBUS_TYPE_STRING, pp,
					 DBUS_TYPE_INVALID);

	return DBUS_HANDLER_RESULT_HANDLED;
}

static int
issue_dbus_signal(struct lws *wsi, const char *signame, const char *string)
{
	struct lws_dbus_ctx_wsproxy *wspctx =
			lws_get_opaque_parent_data(wsi);
	DBusMessage *m;

	if (!wspctx)
		return 1;

	m = dbus_message_new_signal(THIS_OBJECT, THIS_INTERFACE, signame);
	if (!m) {
		lwsl_err("%s: new signal failed\n", __func__);
		return 1;
	}

	dbus_message_append_args(m, DBUS_TYPE_STRING, &string,
				    DBUS_TYPE_INVALID);

	if (!dbus_connection_send(wspctx->ctx.conn, m, NULL))
		lwsl_err("%s: unable to send\n", __func__);

	dbus_message_unref(m);

	return 0;
}

static DBusHandlerResult
dmh_send(DBusConnection *c, DBusMessage *m, DBusMessage **reply, void *d)
{
	struct lws_dbus_ctx_wsproxy *wspctx = (struct lws_dbus_ctx_wsproxy *)d;
	const char *payload;
	struct msg amsg;
	DBusError err;

	dbus_error_init(&err);

	if (!wspctx->cwsi || !wspctx->pss) {
		dbus_message_unref(*reply);
		*reply = dbus_message_new_error(m, "Send Fail", "No ws conn");

		return DBUS_HANDLER_RESULT_HANDLED;
	}

	if (!dbus_message_get_args(m, &err, DBUS_TYPE_STRING, &payload,
					    DBUS_TYPE_INVALID)) {
		dbus_message_unref(*reply);
		*reply = dbus_message_new_error(m, err.name, err.message);
		dbus_error_free(&err);

		return DBUS_HANDLER_RESULT_HANDLED;
	}

	/*
	 * we allocate on the ringbuffer in ws world, but responsibility for
	 * freeing it is understood by lws_ring.
	 */

	amsg.len = strlen(payload);
	/* notice we over-allocate by LWS_PRE */
	amsg.payload = malloc(LWS_PRE + amsg.len);
	if (!amsg.payload) {
		lwsl_user("OOM: dropping\n");
		dbus_message_unref(*reply);
		*reply = dbus_message_new_error(m, "Send Fail", "OOM");

		return DBUS_HANDLER_RESULT_HANDLED;
	}
	amsg.binary = 0;
	amsg.first = 1;
	amsg.final = 1;

	memcpy((char *)amsg.payload + LWS_PRE, payload, amsg.len);
	if (!lws_ring_insert(wspctx->pss->ring_out, &amsg, 1)) {
		destroy_message(&amsg);
		lwsl_user("Ring Full!\n");
		dbus_message_unref(*reply);
		*reply = dbus_message_new_error(m, "Send Fail", "Ring full");

		return DBUS_HANDLER_RESULT_HANDLED;
	}
	if (wspctx->cwsi)
		lws_callback_on_writable(wspctx->cwsi);

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
	{ THIS_INTERFACE,		 "Connect",	dmh_connect	},
	{ THIS_INTERFACE,		 "Send",	dmh_send	},
};

static DBusHandlerResult
server_message_handler(DBusConnection *conn, DBusMessage *message, void *data)
{
	struct lws_dbus_methods *mp = meths;
        DBusMessage *reply = NULL;
	DBusHandlerResult result;
	size_t n;

	assert(data);

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

static const DBusObjectPathVTable vtable = {
	.message_function = server_message_handler
};

static void
destroy_dbus_server_conn(struct lws_dbus_ctx_wsproxy *wsctx)
{
	if (!wsctx->ctx.conn)
		return;

	lwsl_notice("%s\n", __func__);

	dbus_connection_unregister_object_path(wsctx->ctx.conn, THIS_OBJECT);
	lws_dll2_remove(&wsctx->ctx.next);
	dbus_connection_unref(wsctx->ctx.conn);
}

/*
 * This is the client dbus side going away.  We need to stop the associated
 * client ws part and make sure it can't dereference us now we are gone.
 */

static void
cb_closing(struct lws_dbus_ctx *ctx)
{
	struct lws_dbus_ctx_wsproxy *wspctx =
			(struct lws_dbus_ctx_wsproxy *)ctx;
	lwsl_err("%s: closing\n", __func__);

	/*
	 * We have to take care that the associated proxy wsi knows our
	 * dbus ctx is going out of scope after we return from here.
	 *
	 * We do it by setting its pointer to our dbus ctx to NULL.
	 */

	if (wspctx->cwsi) {
		lws_set_opaque_parent_data(wspctx->cwsi, NULL);
		lws_set_timeout(wspctx->cwsi,
				PENDING_TIMEOUT_KILLED_BY_PROXY_CLIENT_CLOSE,
				LWS_TO_KILL_ASYNC);
	}

	destroy_dbus_server_conn(wspctx);

	free(wspctx);
}

static void
new_conn(DBusServer *server, DBusConnection *conn, void *d)
{
	struct lws_dbus_ctx_wsproxy *conn_wspctx, /* the new conn context */
				    /* the listener context */
				    *wspctx = (struct lws_dbus_ctx_wsproxy *)d;
	struct vhd_dbus_proxy *vhd = lws_container_of(d,
					struct vhd_dbus_proxy, ctx_listener);

	assert(vhd->vhost == wspctx->ctx.vh);

	lwsl_notice("%s\n", __func__);

	conn_wspctx = malloc(sizeof(*conn_wspctx));
	if (!conn_wspctx)
		return;

	memset(conn_wspctx, 0, sizeof(*conn_wspctx));

	conn_wspctx->ctx.tsi = wspctx->ctx.tsi;
	conn_wspctx->ctx.vh = wspctx->ctx.vh;
	conn_wspctx->ctx.conn = conn;
	conn_wspctx->vhd = vhd; /* let accepted connections also know the vhd */

	assert(conn_wspctx->vhd);

	if (lws_dbus_connection_setup(&conn_wspctx->ctx, conn, cb_closing)) {
		lwsl_err("%s: connection bind to lws failed\n", __func__);
		goto bail;
	}

	if (!dbus_connection_register_object_path(conn, THIS_OBJECT, &vtable,
						  conn_wspctx)) {
		lwsl_err("%s: Failed to register object path\n", __func__);
		goto bail;
	}

	lws_dll2_add_head(&conn_wspctx->ctx.next, &wspctx->ctx.owner);

	/* we take on responsibility for explicit close / unref with this... */
	dbus_connection_ref(conn);

	return;

bail:
	free(conn_wspctx);
}

static int
create_dbus_listener(struct vhd_dbus_proxy *vhd, int tsi)
{
	DBusError e;

        dbus_error_init(&e);
#if 0
        vhd->dctx.ctx.tsi = tsi;
        vhd->dctx.ctx.vh = vhd->vhost;
        vhd->dctx.ctx.next.prev = NULL;
        vhd->dctx.ctx.next.next = NULL;
        vhd->dctx.vhd = vhd;
        vhd->dctx.cwsi = NULL;

	/* connect to the SYSTEM bus */

	vhd->dctx.ctx.conn = dbus_bus_get(DBUS_BUS_SYSTEM, &e);
	if (!vhd->dctx.ctx.conn) {
		lwsl_notice("%s: Failed to get a session DBus connection: '%s'"
			    ", continuing with daemon listener only\n",
			 __func__, e.message);
		dbus_error_free(&e);
		dbus_error_init(&e);
		goto daemon;
	}

	/*
	 * by default dbus will call exit() when this connection closes...
	 * we have to shut down other things cleanly, so disable that
	 */
	dbus_connection_set_exit_on_disconnect(vhd->dctx.ctx.conn, 0);

	if (dbus_bus_request_name(vhd->dctx.ctx.conn, THIS_BUSNAME,
				  DBUS_NAME_FLAG_REPLACE_EXISTING, &e) !=
					DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER) {
		lwsl_notice("%s: Failed to request name on bus: '%s',"
			 " continuing with daemon listener only\n",
			 __func__, e.message);
		dbus_connection_unref(vhd->dctx.ctx.conn);
		vhd->dctx.ctx.conn = NULL;
		dbus_error_free(&e);
		dbus_error_init(&e);
		goto daemon;
	}

	if (!dbus_connection_register_object_path(vhd->dctx.ctx.conn,
						  THIS_OBJECT, &vtable,
						  &vhd->dctx)) {
		lwsl_err("%s: Failed to register object path\n", __func__);
		goto fail;
	}

	/*
	 * This is the part that binds the connection to lws watcher and
	 * timeout handling provided by lws
	 */

	if (lws_dbus_connection_setup(&vhd->dctx.ctx, vhd->dctx.ctx.conn,
				      cb_closing)) {
		lwsl_err("%s: connection bind to lws failed\n", __func__);
		goto fail;
	}

daemon:
#endif
        vhd->ctx_listener.vh = vhd->vhost;
        vhd->ctx_listener.tsi = tsi;

	if (!lws_dbus_server_listen(&vhd->ctx_listener, vhd->dbus_listen_ads,
				    &e, new_conn)) {
		lwsl_err("%s: failed\n", __func__);
		dbus_error_free(&e);

		return 1;
	}

	lwsl_notice("%s: created DBUS listener on %s\n", __func__,
			vhd->dbus_listen_ads);

	return 0;
#if 0
fail:
	dbus_error_free(&e);

	return 1;
#endif
}

static void
destroy_dbus_server_listener(struct vhd_dbus_proxy *vhd)
{
	dbus_server_disconnect(vhd->ctx_listener.dbs);

	lws_start_foreach_dll_safe(struct lws_dll2 *, rdt, nx,
			vhd->ctx_listener.owner.head) {
		struct lws_dbus_ctx *r = lws_container_of(rdt,
						struct lws_dbus_ctx, next);

		dbus_connection_close(r->conn);
		dbus_connection_unref(r->conn);
		free(r);
	} lws_end_foreach_dll_safe(rdt, nx);

	if (vhd->dctx.ctx.conn)
		dbus_connection_unref(vhd->dctx.ctx.conn);
	dbus_server_unref(vhd->ctx_listener.dbs);
}

/*
 * WS WORLD
 */

static int
callback_minimal_dbus_wsproxy(struct lws *wsi, enum lws_callback_reasons reason,
			      void *user, void *in, size_t len)
{
	struct pss_dbus_proxy *pss = (struct pss_dbus_proxy *)user;
	struct vhd_dbus_proxy *vhd = (struct vhd_dbus_proxy *)
			lws_protocol_vh_priv_get(lws_get_vhost(wsi),
						 lws_get_protocol(wsi));
	struct lws_dbus_ctx_wsproxy *wspctx;
	const struct msg *pmsg;
	int flags, m;

	switch (reason) {

	case LWS_CALLBACK_PROTOCOL_INIT:
		vhd = lws_protocol_vh_priv_zalloc(lws_get_vhost(wsi),
					lws_get_protocol(wsi), sizeof(*vhd));
		if (!vhd)
			return -1;

		vhd->context = lws_get_context(wsi);
		vhd->vhost = lws_get_vhost(wsi);

		if (lws_pvo_get_str(in, "ads", &vhd->dbus_listen_ads)) {
			lwsl_err("%s: pvo 'ads' must be set\n", __func__);
			return -1;
		}

		if (create_dbus_listener(vhd, 0)) {
			lwsl_err("%s: create_dbus_listener failed\n", __func__);
			return -1;
		}
		break;

	case LWS_CALLBACK_PROTOCOL_DESTROY:
		destroy_dbus_server_listener(vhd);
		/* this is required for valgrind-cleanliness */
		dbus_shutdown();
		break;

	case LWS_CALLBACK_CLIENT_ESTABLISHED:
		lwsl_user("LWS_CALLBACK_CLIENT_ESTABLISHED\n");

		/*
		 * create the send ringbuffer now the ws connection is
		 * established.
		 */

		wspctx = lws_get_opaque_parent_data(wsi);
		if (!wspctx)
			break;

		wspctx->pss = pss;
		pss->ring_out_tail = 0;
		pss->ring_out = lws_ring_create(sizeof(struct msg), 8,
						   destroy_message);
		if (!pss->ring_out) {
			lwsl_err("OOM\n");
			return -1;
		}

		issue_dbus_signal(wsi, "Status",
				  "ws client connection established");
		break;

	case LWS_CALLBACK_CLIENT_WRITEABLE:
		lwsl_user("LWS_CALLBACK_CLIENT_WRITEABLE:\n");

		pmsg = lws_ring_get_element(pss->ring_out, &pss->ring_out_tail);
		if (!pmsg) {
			lwsl_user(" (nothing in ring)\n");
			break;
		}

		flags = lws_write_ws_flags(
			    pmsg->binary ? LWS_WRITE_BINARY : LWS_WRITE_TEXT,
			    pmsg->first, pmsg->final);

		/* notice we allowed for LWS_PRE in the payload already */
		m = lws_write(wsi, ((unsigned char *)pmsg->payload) + LWS_PRE,
			      pmsg->len, flags);
		if (m < (int)pmsg->len) {
			lwsl_err("ERROR %d writing to ws socket\n", m);
			return -1;
		}

		lwsl_user(" wrote %d: flags: 0x%x first: %d final %d\n",
				m, flags, pmsg->first, pmsg->final);

		lws_ring_consume_single_tail(pss->ring_out,
					     &pss->ring_out_tail, 1);

		/* more to do for us? */
		if (lws_ring_get_element(pss->ring_out, &pss->ring_out_tail))
			/* come back as soon as we can write more */
			lws_callback_on_writable(wsi);

		break;

	case LWS_CALLBACK_CLIENT_RECEIVE:

		lwsl_user("LWS_CALLBACK_CLIENT_RECEIVE: %4d "
			  "(rpp %5d, first %d, last %d, bin %d)\n",
			  (int)len, (int)lws_remaining_packet_payload(wsi),
			  lws_is_first_fragment(wsi),
			  lws_is_final_fragment(wsi),
			  lws_frame_is_binary(wsi));

		{
			char strbuf[256];
			size_t l = len;

			if (l > sizeof(strbuf) - 1u)
				l = sizeof(strbuf) - 1u;

			memcpy(strbuf, in, l);
			strbuf[l] = '\0';

			issue_dbus_signal(wsi, "Receive", strbuf);
		}
		break;

	case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
		lwsl_err("CLIENT_CONNECTION_ERROR: %s\n",
			 in ? (char *)in : "(null)");
		issue_dbus_signal(wsi, "Status", "ws client connection error");
		break;

	case LWS_CALLBACK_CLIENT_CLOSED:
		lwsl_err("LWS_CALLBACK_CLIENT_CLOSED ()\n");
		issue_dbus_signal(wsi, "Status", "ws client connection closed");

		/* destroy any ringbuffer and pending messages */

		lws_ring_destroy(pss->ring_out);

		wspctx = lws_get_opaque_parent_data(wsi);
		if (!wspctx)
			break;

		/*
		 * the wspctx cannot refer to its child wsi any longer, it is
		 * about to go out of scope.
		 */

		wspctx->cwsi = NULL;
		wspctx->pss = NULL;
		break;

	default:
		break;
	}

	return 0;
}

#define LWS_PLUGIN_PROTOCOL_MINIMAL_DBUS_WSPROXY \
	{ \
		"lws-minimal-dbus-wsproxy", \
		callback_minimal_dbus_wsproxy, \
		sizeof(struct pss_dbus_proxy), \
		1024, \
		0, NULL, 0 \
	}
