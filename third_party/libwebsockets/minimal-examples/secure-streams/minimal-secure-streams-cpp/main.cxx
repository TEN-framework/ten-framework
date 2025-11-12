/*
 * lws-minimal-secure-streams-cpp
 *
 * Written in 2020 by Andy Green <andy@warmcat.com>
 *
 * This file is made available under the Creative Commons CC0 1.0
 * Universal Public Domain Dedication.
 *
 * This demonstrates a minimal http client using secure streams C++ api to
 * fetch files over https to the local filesystem
 */

#include <libwebsockets.hxx>
#include <string.h>
#include <signal.h>

static int interrupted, bad = 1, concurrent = 1, completed;

static int
lss_completion(lss *lss, lws_ss_constate_t state, void *arg)
{
	lssFile *lf = (lssFile *)lss;

	if (state == LWSSSCS_QOS_ACK_REMOTE) {
		lwsl_notice("%s: %s: len %llu, done OK %dms\n", __func__,
			    lf->path.c_str(), (unsigned long long)lf->rxlen,
			    (int)((lws_now_usecs() - lf->us_start) / 1000));
	} else
		lwsl_notice("%s: %s: failed\n", __func__, lf->path.c_str());

	if (++completed == concurrent) {
		interrupted = 1;
		bad = 0;
	}

	return 0;
}

static void
sigint_handler(int sig)
{
	interrupted = 1;
}

int main(int argc, const char **argv)
{
	struct lws_context_creation_info info;
	struct lws_context *context;
	const char *p;

	signal(SIGINT, sigint_handler);

	memset(&info, 0, sizeof info);
	lws_cmdline_option_handle_builtin(argc, argv, &info);

	if ((p = lws_cmdline_option(argc, argv, "-c")))
		concurrent = atoi(p);

	if (concurrent > 12)
		concurrent = 12;

	lwsl_user("LWS secure streams cpp test client "
			"[-d<verb>] [-c<concurrent>]\n");

	info.fd_limit_per_thread = 1 + 12 + 1;
	info.port = CONTEXT_PORT_NO_LISTEN;
	info.options = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;

	/* create the context */

	context = lws_create_context(&info);
	if (!context) {
		lwsl_err("lws init failed\n");
		return 1;
	}

	try {

		for (int n = 0; n < concurrent; n++) {
			std::string url, filepath;

			url = "https://warmcat.com/test-";
			url += ('a' + n);
			url += ".bin";

			filepath = "/tmp/test-";
			filepath += ('a' + n);
			filepath += ".bin";

			new lssFile(context, url, filepath, lss_completion, 0);
		}
	} catch (std::exception &e) {
		lwsl_err("%s: failed to create ss: %s\n", __func__, e.what());
		interrupted = 1;
	}

	/* the event loop */

	while (!interrupted && lws_service(context, 0) >= 0)
		;

	lws_context_destroy(context);

	lwsl_user("Completed: %s\n", bad ? "failed" : "OK");

	return bad;
}
