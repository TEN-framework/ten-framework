/*
 * lws-minimal-http-server-deaddrop
 *
 * Written in 2010-2020 by Andy Green <andy@warmcat.com>
 *
 * This file is made available under the Creative Commons CC0 1.0
 * Universal Public Domain Dedication.
 *
 * This demonstrates how you can leverage the lws deaddrop plugin to make a
 * secure, modern html5 file upload and sharing application.
 *
 * Because the guts are in a plugin, you can avoid all this setup by using the
 * plugin from lwsws and do the config in JSON.
 */

#include <libwebsockets.h>
#include <string.h>
#include <signal.h>
#include <time.h>

#define LWS_PLUGIN_STATIC
#include "../plugins/deaddrop/protocol_lws_deaddrop.c"

static struct lws_protocols protocols[] = {
       LWS_PLUGIN_PROTOCOL_DEADDROP,
       LWS_PROTOCOL_LIST_TERM
};


static int interrupted;

/*
 * teach the /get mount how to present various filetypes to the client...
 * lws won't serve files it doesn't know the mimetype for as a security
 * measure.
 */

static struct lws_protocol_vhost_options em3 = {
        NULL, NULL, ".zip", "application/zip"
}, em2 = {
	&em3, NULL, ".pdf", "application/pdf"
}, extra_mimetypes = {
	&em2, NULL, ".tar.gz", "application/x-gzip"
};

/* wire up /upload URLs to the plugin (protected by basic auth) */

static const struct lws_http_mount mount_upload = {
	/* .mount_next */		NULL,
	/* .mountpoint */		"/upload",	/* mountpoint URL */
	/* .origin */			"lws-deaddrop",
	/* .def */			"",
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
	/* .origin_protocol */		LWSMPRO_CALLBACK,
	/* .mountpoint_len */		7,		/* char count */
	/* .basic_auth_login_file */	"./ba-passwords",
};

/* wire up /get URLs to the upload directory (protected by basic auth) */

static const struct lws_http_mount mount_get = {
	/* .mount_next */		&mount_upload,	/* linked-list "next" */
	/* .mountpoint */		"/get",	/* mountpoint URL */
	/* .origin */			"./uploads",
	/* .def */			"",
	/* .protocol */			NULL,
	/* .cgienv */			NULL,
	/* .extra_mimetypes */		&extra_mimetypes,
	/* .interpret */		NULL,
	/* .cgi_timeout */		0,
	/* .cache_max_age */		0,
	/* .auth_mask */		0,
	/* .cache_reusable */		0,
	/* .cache_revalidate */		0,
	/* .cache_intermediaries */	0,
	/* .origin_protocol */		LWSMPRO_FILE, /* dynamic */
	/* .mountpoint_len */		4,		/* char count */
	/* .basic_auth_login_file */	"./ba-passwords",
};

/* wire up / to serve from ./mount-origin (protected by basic auth) */

static const struct lws_http_mount mount = {
	/* .mount_next */		&mount_get,	/* linked-list "next" */
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
	/* .basic_auth_login_file */	"./ba-passwords",
};

/* pass config options to the deaddrop plugin using pvos */

static struct lws_protocol_vhost_options pvo3 = {
	/* make the wss also require to pass basic auth */
	NULL, NULL, "basic-auth", "./ba-passwords"
}, pvo2 = {
	&pvo3, NULL, "max-size", "10000000"
}, pvo1 = {
        &pvo2, NULL, "upload-dir", "./uploads" /* would be an absolute path */
}, pvo = {
        NULL,                  /* "next" pvo linked-list */
        &pvo1,                 /* "child" pvo linked-list */
        "lws-deaddrop",        /* protocol name we belong to on this vhost */
        ""                     /* ignored */
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
	int n = 0, logs = LLL_USER | LLL_ERR | LLL_WARN | LLL_NOTICE;

	signal(SIGINT, sigint_handler);

	if ((p = lws_cmdline_option(argc, argv, "-d")))
		logs = atoi(p);

	lws_set_log_level(logs, NULL);
	lwsl_user("LWS minimal http server deaddrop | visit https://localhost:7681\n");

	memset(&info, 0, sizeof info); /* otherwise uninitialized garbage */
	info.port = 7681;
	info.mounts = &mount;
	info.pvo = &pvo;
	info.protocols = protocols;
	info.error_document_404 = "/404.html";
	info.options = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT |
		LWS_SERVER_OPTION_HTTP_HEADERS_SECURITY_BEST_PRACTICES_ENFORCE;
#if defined(LWS_WITH_TLS)
	info.ssl_cert_filepath = "localhost-100y.cert";
	info.ssl_private_key_filepath = "localhost-100y.key";
#endif

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
