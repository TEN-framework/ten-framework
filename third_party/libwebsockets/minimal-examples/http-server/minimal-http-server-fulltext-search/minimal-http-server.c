/*
 * lws-minimal-http-server-fts
 *
 * Written in 2010-2019 by Andy Green <andy@warmcat.com>
 *
 * This file is made available under the Creative Commons CC0 1.0
 * Universal Public Domain Dedication.
 *
 * This demonstrates how to use lws full-text search
 */

#include <libwebsockets.h>
#include <string.h>
#include <signal.h>

#define LWS_PLUGIN_STATIC
#include <protocol_fulltext_demo.c>

const char *index_filepath = "./lws-fts.index";
static int interrupted;

static struct lws_protocols protocols[] = {
	LWS_PLUGIN_PROTOCOL_FULLTEXT_DEMO,
	LWS_PROTOCOL_LIST_TERM
};

static struct lws_protocol_vhost_options pvo_idx = {
	NULL,
	NULL,
	"indexpath",		/* pvo name */
	NULL	/* filled in at runtime */
};

static const struct lws_protocol_vhost_options pvo = {
	NULL,		/* "next" pvo linked-list */
	&pvo_idx,	/* "child" pvo linked-list */
	"lws-test-fts",	/* protocol name we belong to on this vhost */
	""		/* ignored */
};

/* override the default mount for /fts in the URL space */

static const struct lws_http_mount mount_fts = {
	/* .mount_next */		NULL,		/* linked-list "next" */
	/* .mountpoint */		"/fts",		/* mountpoint URL */
	/* .origin */			NULL,	/* protocol */
	/* .def */			NULL,
	/* .protocol */			"lws-test-fts",
	/* .cgienv */			NULL,
	/* .extra_mimetypes */		NULL,
	/* .interpret */		NULL,
	/* .cgi_timeout */		0,
	/* .cache_max_age */		0,
	/* .auth_mask */		0,
	/* .cache_reusable */		0,
	/* .cache_revalidate */		0,
	/* .cache_intermediaries */	0,
	/* .origin_protocol */		LWSMPRO_CALLBACK, /* dynamic */
	/* .mountpoint_len */		4,		/* char count */
	/* .basic_auth_login_file */	NULL,
};

static const struct lws_http_mount mount = {
	/* .mount_next */		&mount_fts,	/* linked-list "next" */
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

void sigint_handler(int sig)
{
	interrupted = 1;
}

int main(int argc, const char **argv)
{
	int n = 0, logs = LLL_USER | LLL_ERR | LLL_WARN | LLL_NOTICE;
	struct lws_context_creation_info info;
	struct lws_context *context;
	const char *p;

	signal(SIGINT, sigint_handler);

	if ((p = lws_cmdline_option(argc, argv, "-d")))
		logs = atoi(p);

	lws_set_log_level(logs, NULL);
	lwsl_user("LWS minimal http server fulltext search | "
		  "visit http://localhost:7681\n");

	memset(&info, 0, sizeof info);
	info.port = 7681;
	info.mounts = &mount;
	info.protocols = protocols;
	info.pvo = &pvo;
	info.options =
		LWS_SERVER_OPTION_HTTP_HEADERS_SECURITY_BEST_PRACTICES_ENFORCE;

	pvo_idx.value = index_filepath;

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
