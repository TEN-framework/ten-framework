/*
 * lws-minimal-http-server-eventlib-custom
 *
 * Written in 2010-2021 by Andy Green <andy@warmcat.com>
 *
 * This file is made available under the Creative Commons CC0 1.0
 * Universal Public Domain Dedication.
 *
 * This demonstrates a minimal http server using lws, on top of a custom "event
 * library" that uses an existing application POLL loop.
 *
 * To keep it simple, it serves stuff from the subdirectory  "./mount-origin" of
 * the dir it was started in.  Change mount.origin to serve from elsewhere.
 */

#include <libwebsockets.h>
#include <string.h>
#include <signal.h>

static int interrupted;
static struct lws_context *context;

#define MAX_CUSTOM_POLLFDS	64

/* this represents the existing application poll loop context we want lws
 * to cooperate with */

typedef struct custom_poll_ctx {
	struct lws_pollfd	pollfds[MAX_CUSTOM_POLLFDS];
	int			count_pollfds;
} custom_poll_ctx_t;

/* for this example we just have the one, but it is passed into lws as a
 * foreign loop pointer, and all callbacks have access to it via that, so it
 * is not needed to be defined at file scope. */
static custom_poll_ctx_t a_cpcx;

/*
 * These are the custom event loop operators that just make the custom event
 * loop able to work by itself.  These would already exist in some form in an
 * existing application.
 */

static struct lws_pollfd *
custom_poll_find_fd(custom_poll_ctx_t *cpcx, lws_sockfd_type fd)
{
	int n;

	for (n = 0; n < cpcx->count_pollfds; n++)
		if (cpcx->pollfds[n].fd == fd)
			return &cpcx->pollfds[n];

	return NULL;
}

static int
custom_poll_add_fd(custom_poll_ctx_t *cpcx, lws_sockfd_type fd, int events)
{
	struct lws_pollfd *pfd;

	lwsl_info("%s: ADD fd %d, ev %d\n", __func__, fd, events);

	pfd = custom_poll_find_fd(cpcx, fd);
	if (pfd) {
		lwsl_err("%s: ADD fd %d already in ext table\n", __func__, fd);
		return 1;
	}

	if (cpcx->count_pollfds == LWS_ARRAY_SIZE(cpcx->pollfds)) {
		lwsl_err("%s: no room left\n", __func__);
		return 1;
	}

	pfd = &cpcx->pollfds[cpcx->count_pollfds++];
	pfd->fd = fd;
	pfd->events = (short)events;
	pfd->revents = 0;

	return 0;
}

static int
custom_poll_del_fd(custom_poll_ctx_t *cpcx, lws_sockfd_type fd)
{
	struct lws_pollfd *pfd;

	lwsl_info("%s: DEL fd %d\n", __func__, fd);

	pfd = custom_poll_find_fd(cpcx, fd);
	if (!pfd) {
		lwsl_err("%s: DEL fd %d missing in ext table\n", __func__, fd);
		return 1;
	}

	if (cpcx->count_pollfds > 1)
		*pfd = cpcx->pollfds[cpcx->count_pollfds - 1];

	cpcx->count_pollfds--;

	return 0;
}

static int
custom_poll_change_fd(custom_poll_ctx_t *cpcx, lws_sockfd_type fd,
		     int events_add, int events_remove)
{
	struct lws_pollfd *pfd;

	lwsl_info("%s: CHG fd %d, ev_add %d, ev_rem %d\n", __func__, fd,
			events_add, events_remove);

	pfd = custom_poll_find_fd(cpcx, fd);
	if (!pfd)
		return 1;

	pfd->events = (short)((pfd->events & (~events_remove)) | events_add);

	return 0;
}

int
custom_poll_run(custom_poll_ctx_t *cpcx)
{
	int n;

	while (!interrupted) {

		/*
		 * Notice that the existing loop must consult with lws about
		 * the maximum wait timeout to use.  Lws will reduce the
		 * timeout to the earliest scheduled event time if any earlier
		 * than the provided timeout.
		 */

		n = lws_service_adjust_timeout(context, 5000, 0);

		lwsl_debug("%s: entering poll wait %dms\n", __func__, n);

		n = poll(cpcx->pollfds, (nfds_t)cpcx->count_pollfds, n);

		lwsl_debug("%s: exiting poll ret %d\n", __func__, n);

		if (n <= 0)
			continue;

		for (n = 0; n < cpcx->count_pollfds; n++) {
			lws_sockfd_type fd = cpcx->pollfds[n].fd;
			int m;

			if (!cpcx->pollfds[n].revents)
				continue;

			m = lws_service_fd(context, &cpcx->pollfds[n]);

			/* if something closed, retry this slot since may have been
			 * swapped with end fd */
			if (m && cpcx->pollfds[n].fd != fd)
				n--;

			if (m < 0)
				/* lws feels something bad happened, but
				 * the outer application may not care */
				continue;
			if (!m) {
				/* check if it is an fd owned by the
				 * application */
			}
		}
	}

	return 0;
}


/*
 * These is the custom "event library" interface layer between lws event lib
 * support and the custom loop implementation above.  We only need to support
 * a few key apis.
 *
 * We are user code, so all the internal lws objects are opaque.  But there are
 * enough public helpers to get everything done.
 */

/* one of these is appended to each pt for our use */
struct pt_eventlibs_custom {
	custom_poll_ctx_t		*io_loop;
};

/*
 * During lws context creation, we get called with the foreign loop pointer
 * that was passed in the creation info struct.  Stash it in our private part
 * of the pt, so we can reference it in the other callbacks subsequently.
 */

static int
init_pt_custom(struct lws_context *cx, void *_loop, int tsi)
{
	struct pt_eventlibs_custom *priv = (struct pt_eventlibs_custom *)
					     lws_evlib_tsi_to_evlib_pt(cx, tsi);

	/* store the loop we are bound to in our private part of the pt */

	priv->io_loop = (custom_poll_ctx_t *)_loop;

	return 0;
}

static int
sock_accept_custom(struct lws *wsi)
{
	struct pt_eventlibs_custom *priv = (struct pt_eventlibs_custom *)
						lws_evlib_wsi_to_evlib_pt(wsi);

	return custom_poll_add_fd(priv->io_loop, lws_get_socket_fd(wsi), POLLIN);
}

static void
io_custom(struct lws *wsi, unsigned int flags)
{
	struct pt_eventlibs_custom *priv = (struct pt_eventlibs_custom *)
						lws_evlib_wsi_to_evlib_pt(wsi);
	int e_add = 0, e_remove = 0;

	if (flags & LWS_EV_START) {
		if (flags & LWS_EV_WRITE)
			e_add |= POLLOUT;

		if (flags & LWS_EV_READ)
			e_add |= POLLIN;
	} else {
		if (flags & LWS_EV_WRITE)
			e_remove |= POLLOUT;

		if (flags & LWS_EV_READ)
			e_remove |= POLLIN;
	}

	custom_poll_change_fd(priv->io_loop, lws_get_socket_fd(wsi),
			      e_add, e_remove);
}

static int
wsi_logical_close_custom(struct lws *wsi)
{
	struct pt_eventlibs_custom *priv = (struct pt_eventlibs_custom *)
						lws_evlib_wsi_to_evlib_pt(wsi);
	return custom_poll_del_fd(priv->io_loop, lws_get_socket_fd(wsi));
}

static const struct lws_event_loop_ops event_loop_ops_custom = {
	.name				= "custom",

	.init_pt			= init_pt_custom,
	.init_vhost_listen_wsi		= sock_accept_custom,
	.sock_accept			= sock_accept_custom,
	.io				= io_custom,
	.wsi_logical_close		= wsi_logical_close_custom,

	.evlib_size_pt			= sizeof(struct pt_eventlibs_custom)
};

static const lws_plugin_evlib_t evlib_custom = {
	.hdr = {
		"custom event loop",
		"lws_evlib_plugin",
		LWS_BUILD_HASH,
		LWS_PLUGIN_API_MAGIC
	},

	.ops	= &event_loop_ops_custom
};

/*
 * The rest is just the normal minimal example for lws, with a couple of extra
 * lines wiring up the custom event library handlers above.
 */

static const struct lws_http_mount mount = {
	/* .mount_next */		NULL,		/* linked-list "next" */
	/* .mountpoint */		"/",		/* mountpoint URL */
	/* .origin */			"./mount-origin", /* serve from dir */
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

/*
 * This demonstrates a client connection operating on the same loop
 * It's optional...
 */

static int
callback_http(struct lws *wsi, enum lws_callback_reasons reason,
	      void *user, void *in, size_t len)
{
	switch (reason) {

	case LWS_CALLBACK_ESTABLISHED_CLIENT_HTTP:
		lwsl_user("LWS_CALLBACK_ESTABLISHED_CLIENT_HTTP: resp %u\n",
				lws_http_client_http_response(wsi));
		break;

	/* because we are protocols[0] ... */
	case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
		lwsl_err("CLIENT_CONNECTION_ERROR: %s\n",
			 in ? (char *)in : "(null)");
		break;

	/* chunks of chunked content, with header removed */
	case LWS_CALLBACK_RECEIVE_CLIENT_HTTP_READ:
		lwsl_user("RECEIVE_CLIENT_HTTP_READ: read %d\n", (int)len);
		lwsl_hexdump_info(in, len);
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
		lwsl_user("LWS_CALLBACK_COMPLETED_CLIENT_HTTP %s\n",
			  lws_wsi_tag(wsi));
		break;

	case LWS_CALLBACK_CLOSED_CLIENT_HTTP:
		lwsl_info("%s: closed: %s\n", __func__, lws_wsi_tag(wsi));
		break;

	default:
		break;
	}

	return lws_callback_http_dummy(wsi, reason, user, in, len);
}

static const struct lws_protocols protocols[] = {
	{ "httptest", callback_http, 0, 0, 0, NULL, 0},
	LWS_PROTOCOL_LIST_TERM
};

static int
do_client_conn(void)
{
	struct lws_client_connect_info i;

	memset(&i, 0, sizeof i); /* otherwise uninitialized garbage */

	i.context		= context;

	i.ssl_connection	= LCCSCF_USE_SSL;
	i.port			= 443;
	i.address		= "warmcat.com";

	i.ssl_connection	|= LCCSCF_H2_QUIRK_OVERFLOWS_TXCR |
				   LCCSCF_H2_QUIRK_NGHTTP2_END_STREAM;
	i.path			= "/";
	i.host			= i.address;
	i.origin		= i.address;
	i.method		= "GET";
	i.protocol	= protocols[0].name;
#if defined(LWS_WITH_SYS_FAULT_INJECTION)
	i.fi_wsi_name		= "user";
#endif

	if (!lws_client_connect_via_info(&i)) {
		lwsl_err("Client creation failed\n");

		return 1;
	}

	lwsl_notice("Client creation OK\n");

	return 0;
}

/*
 * End of client part
 *
 * Initialization part -->
 */

void sigint_handler(int sig)
{
	interrupted = 1;
}

int main(int argc, const char **argv)
{
	struct lws_context_creation_info info;
	const char *p;
	int logs = LLL_USER | LLL_ERR | LLL_WARN | LLL_NOTICE;
	void *foreign_loops[1];

	signal(SIGINT, sigint_handler);

	if ((p = lws_cmdline_option(argc, argv, "-d")))
		logs = atoi(p);

	/*
	 * init the existing custom event loop here if anything to do, don't
	 * run it yet. In our example, no init required.
	 */

	lws_set_log_level(logs, NULL);
	lwsl_user("LWS minimal http server | visit http://localhost:7681\n");

	memset(&info, 0, sizeof info); /* otherwise uninitialized garbage */
	info.port = 7681;
	info.mounts = &mount;
	info.error_document_404 = "/404.html";
	info.options = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT |
		LWS_SERVER_OPTION_HTTP_HEADERS_SECURITY_BEST_PRACTICES_ENFORCE;

	info.event_lib_custom = &evlib_custom; /* bind lws to our custom event
						* lib implementation above */
	foreign_loops[0] = &a_cpcx; /* pass in the custom poll object as the
				     * foreign loop object we will bind to */
	info.foreign_loops = foreign_loops;

	/* optional to demonstrate client connection */
	info.protocols = protocols;

	context = lws_create_context(&info);
	if (!context) {
		lwsl_err("lws init failed\n");
		return 1;
	}

	/* optional to demonstrate client connection */
	do_client_conn();

	/*
	 * We're going to run the custom loop now, instead of the lws loop.
	 * We have told lws to cooperate with this loop to get stuff done.
	 *
	 * We only come back from this when interrupted gets set by SIGINT
	 */

	custom_poll_run(&a_cpcx);

	/* clean up lws part */

	lws_context_destroy(context);

	return 0;
}
