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
 * This role for wrapping dbus fds in a wsi + role is unusual in that the
 * wsi it creates and binds to the role do not have control over the related fd
 * lifecycle.  In fact dbus doesn't inform us directly about the lifecycle of
 * the fds it wants to be managed by the lws event loop.
 *
 * What it does tell us is when it wants to wait on POLLOUT and / or POLLIN,
 * and since it should stop any watchers before close, we take the approach to
 * create a lightweight "shadow" wsi for any fd from dbus that has a POLLIN or
 * POLLOUT wait active.  When the dbus fd asks to have no wait active, we
 * destroy the wsi, since this is indistinguishable from dbus close path
 * behaviour.  If it actually stays alive and later asks to wait again, well no
 * worries we create a new shadow wsi until it looks like it is closing again.
 */

#include <private-lib-core.h>

#include <libwebsockets/lws-dbus.h>

/*
 * retreives existing or creates new shadow wsi for fd owned by dbus stuff.
 *
 * Requires context + vhost lock
 */

static struct lws *
__lws_shadow_wsi(struct lws_dbus_ctx *ctx, DBusWatch *w, int fd, int create_ok)
{
	struct lws *wsi;

	if (fd < 0 || fd >= (int)ctx->vh->context->fd_limit_per_thread) {
		lwsl_err("%s: fd %d vs fds_count %d\n", __func__, fd,
				(int)ctx->vh->context->fd_limit_per_thread);
		assert(0);

		return NULL;
	}

	wsi = wsi_from_fd(ctx->vh->context, fd);
	if (wsi) {
		assert(wsi->opaque_parent_data == ctx);

		return wsi;
	}

	if (!create_ok)
		return NULL;

	lws_context_assert_lock_held(wsi->a.context);
	lws_vhost_assert_lock_held(wsi->a.vhost);

	/* requires context lock */
	wsi = __lws_wsi_create_with_role(ctx->vh->context, ctx->tsi, NULL,
						ctx->vh->lc.log_cx);
	if (wsi == NULL) {
		lwsl_err("Out of mem\n");
		return NULL;
	}

	lwsl_info("%s: creating shadow wsi\n", __func__);

	wsi->desc.sockfd = fd;
	lws_role_transition(wsi, 0, LRS_ESTABLISHED, &role_ops_dbus);
	wsi->a.protocol = ctx->vh->protocols;
	wsi->shadow = 1;
	wsi->opaque_parent_data = ctx;
	ctx->w[0] = w;

	__lws_lc_tag(ctx->vh->context, &ctx->vh->context->lcg[LWSLCG_WSI],
		     &wsi->lc, "dbus|%s", ctx->vh->name);

	lws_vhost_bind_wsi(ctx->vh, wsi);
	if (__insert_wsi_socket_into_fds(ctx->vh->context, wsi)) {
		lwsl_err("inserting wsi socket into fds failed\n");
		__lws_vhost_unbind_wsi(wsi); /* cx + vh lock */
		lws_free(wsi);
		return NULL;
	}

	return wsi;
}

/*
 * Requires cx + vhost lock
 */

static int
__lws_shadow_wsi_destroy(struct lws_dbus_ctx *ctx, struct lws *wsi)
{
	lwsl_info("%s: destroying shadow wsi\n", __func__);

	lws_context_assert_lock_held(wsi->a.context);
	lws_vhost_assert_lock_held(wsi->a.vhost);

	if (__remove_wsi_socket_from_fds(wsi)) {
		lwsl_err("%s: unable to remove %d from fds\n", __func__,
				wsi->desc.sockfd);

		return 1;
	}

	__lws_vhost_unbind_wsi(wsi);

	lws_free(wsi);

	return 0;
}


static void
handle_dispatch_status(DBusConnection *c, DBusDispatchStatus s, void *data)
{
	lwsl_info("%s: new dbus dispatch status: %d\n", __func__, s);
}

/*
 * These are complicated by the fact libdbus can have two separate DBusWatch
 * objects for the same fd, to control watching POLLIN and POLLOUT individually.
 *
 * However we will actually watch using poll(), where the unit is the fd, and
 * it has a unified events field with just POLLIN / POLLOUT flags.
 *
 * So we have to be prepared for one or two watchers coming in any order.
 */

static dbus_bool_t
lws_dbus_add_watch(DBusWatch *w, void *data)
{
	struct lws_dbus_ctx *ctx = (struct lws_dbus_ctx *)data;
	struct lws_context_per_thread *pt = &ctx->vh->context->pt[ctx->tsi];
	unsigned int flags = 0, lws_flags = 0;
	struct lws *wsi;
	int n;

	lws_context_lock(pt->context, __func__);
	lws_pt_lock(pt, __func__);

	wsi = __lws_shadow_wsi(ctx, w, dbus_watch_get_unix_fd(w), 1);
	if (!wsi) {
		lws_pt_unlock(pt);
		lws_context_unlock(pt->context);
		lwsl_err("%s: unable to get wsi\n", __func__);

		return FALSE;
	}

	for (n = 0; n < (int)LWS_ARRAY_SIZE(ctx->w); n++)
		if (w == ctx->w[n])
			break;

	if (n == (int)LWS_ARRAY_SIZE(ctx->w))
		for (n = 0; n < (int)LWS_ARRAY_SIZE(ctx->w); n++)
			if (!ctx->w[n]) {
				ctx->w[n] = w;
				break;
			}

	for (n = 0; n < (int)LWS_ARRAY_SIZE(ctx->w); n++)
		if (ctx->w[n] && dbus_watch_get_enabled(ctx->w[n]))
			flags |= dbus_watch_get_flags(ctx->w[n]);

	if (flags & DBUS_WATCH_READABLE)
		lws_flags |= LWS_POLLIN;
	if (flags & DBUS_WATCH_WRITABLE)
		lws_flags |= LWS_POLLOUT;

	lwsl_info("%s: %s: %p, fd %d, data %p, fl %d\n", __func__,
		  lws_wsi_tag(wsi), w, dbus_watch_get_unix_fd(w),
		  data, lws_flags);

	if (lws_flags)
		__lws_change_pollfd(wsi, 0, (int)lws_flags);

	lws_pt_unlock(pt);
	lws_context_unlock(pt->context);

	return TRUE;
}

/* cx + vh lock */
static int
__check_destroy_shadow_wsi(struct lws_dbus_ctx *ctx, struct lws *wsi)
{
	int n;

	if (!wsi)
		return 0;

	for (n = 0; n < (int)LWS_ARRAY_SIZE(ctx->w); n++)
		if (ctx->w[n])
			return 0;

	__lws_shadow_wsi_destroy(ctx, wsi);

	if (!ctx->conn || !ctx->hup || ctx->timeouts)
		return 0;

	if (dbus_connection_get_dispatch_status(ctx->conn) ==
						     DBUS_DISPATCH_DATA_REMAINS)
		return 0;

	if (ctx->cb_closing)
		ctx->cb_closing(ctx);

	return 1;
}

static void
lws_dbus_remove_watch(DBusWatch *w, void *data)
{
	struct lws_dbus_ctx *ctx = (struct lws_dbus_ctx *)data;
	struct lws_context_per_thread *pt = &ctx->vh->context->pt[ctx->tsi];
	unsigned int flags = 0, lws_flags = 0;
	struct lws *wsi;
	int n;

	lws_context_lock(pt->context, __func__);
	lws_pt_lock(pt, __func__);

	wsi = __lws_shadow_wsi(ctx, w, dbus_watch_get_unix_fd(w), 0);
	if (!wsi)
		goto bail;

	for (n = 0; n < (int)LWS_ARRAY_SIZE(ctx->w); n++)
		if (w == ctx->w[n]) {
			ctx->w[n] = NULL;
			break;
		}

	for (n = 0; n < (int)LWS_ARRAY_SIZE(ctx->w); n++)
		if (ctx->w[n])
			flags |= dbus_watch_get_flags(ctx->w[n]);

	if ((~flags) & DBUS_WATCH_READABLE)
		lws_flags |= LWS_POLLIN;
	if ((~flags) & DBUS_WATCH_WRITABLE)
		lws_flags |= LWS_POLLOUT;

	lwsl_info("%s: %p, fd %d, data %p, clearing lws flags %d\n",
		  __func__, w, dbus_watch_get_unix_fd(w),
		  data, lws_flags);

	__lws_change_pollfd(wsi, (int)lws_flags, 0);

bail:
	lws_pt_unlock(pt);
	lws_context_unlock(pt->context);
}

static void
lws_dbus_toggle_watch(DBusWatch *w, void *data)
{
	if (dbus_watch_get_enabled(w))
		lws_dbus_add_watch(w, data);
	else
		lws_dbus_remove_watch(w, data);
}

static void
lws_dbus_sul_cb(lws_sorted_usec_list_t *sul)
{
	struct lws_context_per_thread *pt = lws_container_of(sul,
				struct lws_context_per_thread, dbus.sul);

	lws_start_foreach_dll_safe(struct lws_dll2 *, rdt, nx,
			 lws_dll2_get_head(&pt->dbus.timer_list_owner)) {
		struct lws_role_dbus_timer *r = lws_container_of(rdt,
					struct lws_role_dbus_timer, timer_list);

		if (time(NULL) > r->fire) {
			lwsl_notice("%s: firing timer\n", __func__);
			dbus_timeout_handle(r->data);
			lws_dll2_remove(rdt);
			lws_free(rdt);
		}
	} lws_end_foreach_dll_safe(rdt, nx);

	if (pt->dbus.timer_list_owner.count)
		lws_sul_schedule(pt->context, pt->tid, &pt->dbus.sul,
				 lws_dbus_sul_cb, 3 * LWS_US_PER_SEC);
}

static dbus_bool_t
lws_dbus_add_timeout(DBusTimeout *t, void *data)
{
	struct lws_dbus_ctx *ctx = (struct lws_dbus_ctx *)data;
	struct lws_context_per_thread *pt = &ctx->vh->context->pt[ctx->tsi];
	int ms = dbus_timeout_get_interval(t);
	struct lws_role_dbus_timer *dbt;
	time_t ti = time(NULL);

	if (!dbus_timeout_get_enabled(t))
		return TRUE;

	if (ms < 1000)
		ms = 1000;

	dbt = lws_malloc(sizeof(*dbt), "dbus timer");
	if (!dbt)
		return FALSE;

	lwsl_info("%s: adding timeout %dms\n", __func__,
			dbus_timeout_get_interval(t));

	dbt->data = t;
	dbt->fire = ti + (ms < 1000);
	dbt->timer_list.prev = NULL;
	dbt->timer_list.next = NULL;
	dbt->timer_list.owner = NULL;
	lws_dll2_add_head(&dbt->timer_list, &pt->dbus.timer_list_owner);

	if (!pt->dbus.sul.list.owner)
		lws_sul_schedule(pt->context, pt->tid, &pt->dbus.sul,
				 lws_dbus_sul_cb, 3 * LWS_US_PER_SEC);

	ctx->timeouts++;

	return TRUE;
}

static void
lws_dbus_remove_timeout(DBusTimeout *t, void *data)
{
	struct lws_dbus_ctx *ctx = (struct lws_dbus_ctx *)data;
	struct lws_context_per_thread *pt = &ctx->vh->context->pt[ctx->tsi];

	lwsl_info("%s: t %p, data %p\n", __func__, t, data);

	lws_start_foreach_dll_safe(struct lws_dll2 *, rdt, nx,
				lws_dll2_get_head(&pt->dbus.timer_list_owner)) {
		struct lws_role_dbus_timer *r = lws_container_of(rdt,
					struct lws_role_dbus_timer, timer_list);
		if (t == r->data) {
			lws_dll2_remove(rdt);
			lws_free(rdt);
			ctx->timeouts--;
			break;
		}
	} lws_end_foreach_dll_safe(rdt, nx);

	if (!pt->dbus.timer_list_owner.count)
		lws_sul_cancel(&pt->dbus.sul);
}

static void
lws_dbus_toggle_timeout(DBusTimeout *t, void *data)
{
	if (dbus_timeout_get_enabled(t))
		lws_dbus_add_timeout(t, data);
	else
		lws_dbus_remove_timeout(t, data);
}

/*
 * This sets up a connection along the same lines as
 * dbus_connection_setup_with_g_main(), but for using the lws event loop.
 */

int
lws_dbus_connection_setup(struct lws_dbus_ctx *ctx, DBusConnection *conn,
			  lws_dbus_closing_t cb_closing)
{
	int n;

	ctx->conn = conn;
	ctx->cb_closing = cb_closing;
	ctx->hup = 0;
	ctx->timeouts = 0;
	for (n = 0; n < (int)LWS_ARRAY_SIZE(ctx->w); n++)
		ctx->w[n] = NULL;

	if (!dbus_connection_set_watch_functions(conn, lws_dbus_add_watch,
						 lws_dbus_remove_watch,
						 lws_dbus_toggle_watch,
						 ctx, NULL)) {
		lwsl_err("%s: dbus_connection_set_watch_functions fail\n",
			 __func__);
		return 1;
	}

	if (!dbus_connection_set_timeout_functions(conn,
						   lws_dbus_add_timeout,
						   lws_dbus_remove_timeout,
						   lws_dbus_toggle_timeout,
						   ctx, NULL)) {
		lwsl_err("%s: dbus_connection_set_timeout_functions fail\n",
			 __func__);
		return 1;
	}

	dbus_connection_set_dispatch_status_function(conn,
						     handle_dispatch_status,
						     ctx, NULL);

	return 0;
}

/*
 * This wraps dbus_server_listen(), additionally taking care of the event loop
 * -related setups.
 */

DBusServer *
lws_dbus_server_listen(struct lws_dbus_ctx *ctx, const char *ads, DBusError *e,
		       DBusNewConnectionFunction new_conn)
{
	ctx->cb_closing = NULL;
	ctx->hup = 0;
	ctx->timeouts = 0;

	ctx->dbs = dbus_server_listen(ads, e);
	if (!ctx->dbs)
		return NULL;

	dbus_server_set_new_connection_function(ctx->dbs, new_conn, ctx, NULL);

	if (!dbus_server_set_watch_functions(ctx->dbs, lws_dbus_add_watch,
					     lws_dbus_remove_watch,
					     lws_dbus_toggle_watch,
					     ctx, NULL)) {
		lwsl_err("%s: dbus_connection_set_watch_functions fail\n",
			 __func__);
		goto bail;
	}

	if (!dbus_server_set_timeout_functions(ctx->dbs, lws_dbus_add_timeout,
					       lws_dbus_remove_timeout,
					       lws_dbus_toggle_timeout,
					       ctx, NULL)) {
		lwsl_err("%s: dbus_connection_set_timeout_functions fail\n",
			 __func__);
		goto bail;
	}

	return ctx->dbs;

bail:
	dbus_server_disconnect(ctx->dbs);
	dbus_server_unref(ctx->dbs);

	return NULL;
}


/*
 * There shouldn't be a race here with watcher removal and poll wait, because
 * everything including the dbus activity is serialized in one event loop.
 *
 * If it removes the watcher and we remove the wsi and fd entry before this,
 * actually we can no longer map the fd to this invalidated wsi pointer to call
 * this.
 */

static int
rops_handle_POLLIN_dbus(struct lws_context_per_thread *pt, struct lws *wsi,
			struct lws_pollfd *pollfd)
{
	struct lws_dbus_ctx *ctx =
			(struct lws_dbus_ctx *)wsi->opaque_parent_data;
	unsigned int flags = 0;
	int n;

	if (pollfd->revents & LWS_POLLIN)
		flags |= DBUS_WATCH_READABLE;
	if (pollfd->revents & LWS_POLLOUT)
		flags |= DBUS_WATCH_WRITABLE;

	if (pollfd->revents & (LWS_POLLHUP))
		ctx->hup = 1;

	/*
	 * POLLIN + POLLOUT gets us called here on the corresponding shadow
	 * wsi.  wsi->opaque_parent_data is the watcher handle bound to the wsi
	 */

	for (n = 0; n < (int)LWS_ARRAY_SIZE(ctx->w); n++)
		if (ctx->w[n] && !dbus_watch_handle(ctx->w[n], flags))
			lwsl_err("%s: dbus_watch_handle failed\n", __func__);

	if (ctx->conn) {
		lwsl_info("%s: conn: flags %d\n", __func__, flags);

		while (dbus_connection_get_dispatch_status(ctx->conn) ==
						DBUS_DISPATCH_DATA_REMAINS)
			dbus_connection_dispatch(ctx->conn);

		handle_dispatch_status(NULL, DBUS_DISPATCH_DATA_REMAINS, NULL);

		__check_destroy_shadow_wsi(ctx, wsi);
	} else
		if (ctx->dbs)
			/* ??? */
			lwsl_debug("%s: dbs: %d\n", __func__, flags);

	return LWS_HPI_RET_HANDLED;
}

static int
rops_pt_init_destroy_dbus(struct lws_context *context,
		    const struct lws_context_creation_info *info,
		    struct lws_context_per_thread *pt, int destroy)
{
	if (destroy)
		lws_sul_cancel(&pt->dbus.sul);

	return 0;
}

static const lws_rops_t rops_table_dbus[] = {
	/*  1 */ { .pt_init_destroy	= rops_pt_init_destroy_dbus },
	/*  2 */ { .handle_POLLIN	= rops_handle_POLLIN_dbus },
};

const struct lws_role_ops role_ops_dbus = {
	/* role name */			"dbus",
	/* alpn id */			NULL,

	/* rops_table */		rops_table_dbus,
	/* rops_idx */			{
	  /* LWS_ROPS_check_upgrades */
	  /* LWS_ROPS_pt_init_destroy */		0x01,
	  /* LWS_ROPS_init_vhost */
	  /* LWS_ROPS_destroy_vhost */			0x00,
	  /* LWS_ROPS_service_flag_pending */
	  /* LWS_ROPS_handle_POLLIN */			0x02,
	  /* LWS_ROPS_handle_POLLOUT */
	  /* LWS_ROPS_perform_user_POLLOUT */		0x00,
	  /* LWS_ROPS_callback_on_writable */
	  /* LWS_ROPS_tx_credit */			0x00,
	  /* LWS_ROPS_write_role_protocol */
	  /* LWS_ROPS_encapsulation_parent */		0x00,
	  /* LWS_ROPS_alpn_negotiated */
	  /* LWS_ROPS_close_via_role_protocol */	0x00,
	  /* LWS_ROPS_close_role */
	  /* LWS_ROPS_close_kill_connection */		0x00,
	  /* LWS_ROPS_destroy_role */
	  /* LWS_ROPS_adoption_bind */			0x00,
	  /* LWS_ROPS_client_bind */
	  /* LWS_ROPS_issue_keepalive */		0x00,
					},

	/* adoption_cb clnt, srv */	{ 0, 0 },
	/* rx_cb clnt, srv */		{ 0, 0 },
	/* writeable cb clnt, srv */	{ 0, 0 },
	/* close cb clnt, srv */	{ 0, 0 },
	/* protocol_bind_cb c,s */	{ 0, 0 },
	/* protocol_unbind_cb c,s */	{ 0, 0 },
	/* file_handle */		0,
};
