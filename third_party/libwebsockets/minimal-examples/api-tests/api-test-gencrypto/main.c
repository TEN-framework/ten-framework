/*
 * lws-api-test-gencrypto
 *
 * Written in 2010-2018 by Andy Green <andy@warmcat.com>
 *
 * This file is made available under the Creative Commons CC0 1.0
 * Universal Public Domain Dedication.
 */

#include <libwebsockets.h>

int
test_genaes(struct lws_context *context);
int
test_genec(struct lws_context *context);

int main(int argc, const char **argv)
{
	struct lws_context_creation_info info;
	struct lws_context *context;
	const char *p;
	int result = 0, logs = LLL_USER | LLL_ERR | LLL_WARN | LLL_NOTICE;

	if ((p = lws_cmdline_option(argc, argv, "-d")))
		logs = atoi(p);

	lws_set_log_level(logs, NULL);
	lwsl_user("LWS gencrypto apis tests\n");

	memset(&info, 0, sizeof info); /* otherwise uninitialized garbage */
#if defined(LWS_WITH_NETWORK)
	info.port = CONTEXT_PORT_NO_LISTEN;
#endif
	info.options = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;

	context = lws_create_context(&info);
	if (!context) {
		lwsl_err("lws init failed\n");
		return 1;
	}

	result |= test_genaes(context);
	result |= test_genec(context);

	lwsl_user("Completed: %s\n", result ? "FAIL" : "PASS");

	lws_context_destroy(context);

	return result;
}
