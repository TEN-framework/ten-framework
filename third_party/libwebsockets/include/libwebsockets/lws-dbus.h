 /*
 * libwebsockets - small server side websockets and web server implementation
 *
 * Copyright (C) 2010 - 2019 Andy Green <andy@warmcat.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 * must be included manually as
 *
 *  #include <libwebsockets/lws-dbus.h>
 *
 * if dbus apis needed
 */

#if !defined(__LWS_DBUS_H__)
#define __LWS_DBUS_H__

#include <dbus/dbus.h>

/* helper type to simplify implementing methods as individual functions */
typedef DBusHandlerResult (*lws_dbus_message_handler)(DBusConnection *conn,
			   DBusMessage *message, DBusMessage **reply, void *d);

struct lws_dbus_ctx;
typedef void (*lws_dbus_closing_t)(struct lws_dbus_ctx *ctx);

struct lws_dbus_ctx {
	struct lws_dll2_owner owner; /* dbusserver ctx: HEAD of accepted list */
	struct lws_dll2 next; /* dbusserver ctx: HEAD of accepted list */
	struct lws_vhost *vh; /* the vhost we logically bind to in lws */
	int tsi;	/* the lws thread service index (0 if only one service
			   thread as is the default */
	DBusConnection *conn;
	DBusServer *dbs;
	DBusWatch *w[4];
 	DBusPendingCall *pc;

 	char hup;
 	char timeouts;

 	/* cb_closing callback will be called after the connection and this
 	 * related ctx struct have effectively gone out of scope.
 	 *
 	 * The callback should close and clean up the connection and free the
 	 * ctx.
 	 */
 	lws_dbus_closing_t cb_closing;
};

/**
 * lws_dbus_connection_setup() - bind dbus connection object to lws event loop
 *
 * \param ctx: additional information about the connection
 * \param conn: the DBusConnection object to bind
 *
 * This configures a DBusConnection object to use lws for watchers and timeout
 * operations.
 */
LWS_VISIBLE LWS_EXTERN int
lws_dbus_connection_setup(struct lws_dbus_ctx *ctx, DBusConnection *conn,
			  lws_dbus_closing_t cb_closing);

/**
 * lws_dbus_server_listen() - bind dbus connection object to lws event loop
 *
 * \param ctx: additional information about the connection
 * \param ads: the DBUS address to listen on, eg, "unix:abstract=mysocket"
 * \param err: a DBusError object to take any extra error information
 * \param new_conn: a callback function to prepare new accepted connections
 *
 * This creates a DBusServer and binds it to the lws event loop, and your
 * callback to accept new connections.
 */
LWS_VISIBLE LWS_EXTERN DBusServer *
lws_dbus_server_listen(struct lws_dbus_ctx *ctx, const char *ads,
		       DBusError *err, DBusNewConnectionFunction new_conn);

#endif
