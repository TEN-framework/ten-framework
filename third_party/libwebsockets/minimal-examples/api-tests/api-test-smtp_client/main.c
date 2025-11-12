/*
 * lws-unit-tests-smtp-client
 *
 * Written in 2010-2019 by Andy Green <andy@warmcat.com>
 *
 * This file is made available under the Creative Commons CC0 1.0
 * Universal Public Domain Dedication.
 *
 * This performs unit tests for the SMTP client abstract protocol
 */

#include <libwebsockets.h>

#include <signal.h>

static int interrupted, results[10], count_tests, count_passes;

static int
email_sent_or_failed(struct lws_smtp_email *email, void *buf, size_t len)
{
	free(email);

	return 0;
}

/*
 * The test helper calls this on the instance it created to prepare it for
 * the test.  In our case, we need to queue up a test email to send on the
 * smtp client abstract protocol.
 */

static int
smtp_test_instance_init(lws_abs_t *instance)
{
	lws_smtp_email_t *email = (lws_smtp_email_t *)
					malloc(sizeof(*email) + 2048);

	if (!email)
		return 1;

	/* attach an email to it */

	memset(email, 0, sizeof(*email));
	email->data = NULL /* email specific user data */;
	email->email_from = "noreply@warmcat.com";
	email->email_to = "andy@warmcat.com";
	email->payload = (void *)&email[1];

	lws_snprintf((char *)email->payload, 2048,
			"From: noreply@example.com\n"
			"To: %s\n"
			"Subject: Test email for lws smtp-client\n"
			"\n"
			"Hello this was an api test for lws smtp-client\n"
			"\r\n.\r\n", "andy@warmcat.com");
	email->done = email_sent_or_failed;

	if (lws_smtpc_add_email(instance, email)) {
		lwsl_err("%s: failed to add email\n", __func__);
		return 1;
	}

	return 0;
}

/*
 * from https://en.wikipedia.org/wiki/Simple_Mail_Transfer_Protocol
 *
 *		test vector sent to protocol
 *				test vector received from protocol
 */

static lws_unit_test_packet_t test_send1[] = {
	{
		"220 smtp.example.com ESMTP Postfix",
		smtp_test_instance_init, 34, LWS_AUT_EXPECT_RX
	}, {
				"HELO lws-test-client\x0a",
		NULL, 21, LWS_AUT_EXPECT_TX
	}, {
		"250 smtp.example.com, I am glad to meet you",
		NULL, 43, LWS_AUT_EXPECT_RX
	}, {
				"MAIL FROM: <noreply@warmcat.com>\x0a",
		NULL, 33, LWS_AUT_EXPECT_TX
	}, {
		"250 Ok",
		NULL, 6, LWS_AUT_EXPECT_RX
	}, {
				"RCPT TO: <andy@warmcat.com>\x0a",
		NULL, 28, LWS_AUT_EXPECT_TX
	}, {
		"250 Ok",
		NULL, 6, LWS_AUT_EXPECT_RX
	}, {
				"DATA\x0a",
		NULL, 5, LWS_AUT_EXPECT_TX
	}, {
		"354 End data with <CR><LF>.<CR><LF>\x0a",
		NULL, 35, LWS_AUT_EXPECT_RX
	}, {
				"From: noreply@example.com\n"
				"To: andy@warmcat.com\n"
				"Subject: Test email for lws smtp-client\n"
				"\n"
				"Hello this was an api test for lws smtp-client\n"
				"\r\n.\r\n",
		NULL, 27 + 21 + 39 + 1 + 47 + 5, LWS_AUT_EXPECT_TX
	}, {
		"250 Ok: queued as 12345\x0a",
		NULL, 23, LWS_AUT_EXPECT_RX
	}, {
				"quit\x0a",
		NULL, 5, LWS_AUT_EXPECT_TX
	}, {
		"221 Bye\x0a",
		NULL, 7, LWS_AUT_EXPECT_RX |
		   LWS_AUT_EXPECT_LOCAL_CLOSE |
		   LWS_AUT_EXPECT_DO_REMOTE_CLOSE |
		   LWS_AUT_EXPECT_TEST_END
	}, { 	/* sentinel */

	}
};


static lws_unit_test_packet_t test_send2[] = {
	{
		"220 smtp.example.com ESMTP Postfix",
		smtp_test_instance_init, 34, LWS_AUT_EXPECT_RX
	}, {
				"HELO lws-test-client\x0a",
		NULL, 21, LWS_AUT_EXPECT_TX
	}, {
		"250 smtp.example.com, I am glad to meet you",
		NULL, 43, LWS_AUT_EXPECT_RX
	}, {
				"MAIL FROM: <noreply@warmcat.com>\x0a",
		NULL, 33, LWS_AUT_EXPECT_TX
	}, {
		"500 Service Unavailable",
		NULL, 23, LWS_AUT_EXPECT_RX |
		   LWS_AUT_EXPECT_DO_REMOTE_CLOSE |
		   LWS_AUT_EXPECT_TEST_END
	}, { 	/* sentinel */

	}
};

static lws_unit_test_t tests[] = {
	{ "sending", test_send1, 3 },
	{ "rejected", test_send2, 3 },
	{ }	/* sentinel */
};

static void
sigint_handler(int sig)
{
	interrupted = 1;
}

/*
 * set the HELO our SMTP client will use
 */

static const lws_token_map_t smtp_ap_tokens[] = {
 {
	.u = { .value = "lws-test-client" },
	.name_index = LTMI_PSMTP_V_HELO,
 }, {	/* sentinel */
 }
};

void
tests_completion_cb(const void *cb_user)
{
	interrupted = 1;
}

int main(int argc, const char **argv)
{
	int n = 1, logs = LLL_USER | LLL_ERR | LLL_WARN | LLL_NOTICE;
	struct lws_context_creation_info info;
	lws_test_sequencer_args_t args;
	struct lws_context *context;
	lws_abs_t *abs = NULL;
	struct lws_vhost *vh;
	const char *p;

	/* the normal lws init */

	signal(SIGINT, sigint_handler);

	if ((p = lws_cmdline_option(argc, argv, "-d")))
		logs = atoi(p);

	lws_set_log_level(logs, NULL);
	lwsl_user("LWS API selftest: SMTP client unit tests\n");

	memset(&info, 0, sizeof info); /* otherwise uninitialized garbage */
	info.port = CONTEXT_PORT_NO_LISTEN;
	info.options = LWS_SERVER_OPTION_EXPLICIT_VHOSTS;

	context = lws_create_context(&info);
	if (!context) {
		lwsl_err("lws init failed\n");
		return 1;
	}

	vh = lws_create_vhost(context, &info);
	if (!vh) {
		lwsl_err("Failed to create first vhost\n");
		goto bail1;
	}

	/* create the abs used to create connections */

	abs = lws_abstract_alloc(vh, NULL, "smtp.unit_test",
				 &smtp_ap_tokens[0], NULL);
	if (!abs)
		goto bail1;

	/* configure the test sequencer */

	args.abs = abs;
	args.tests = tests;
	args.results = results;
	args.results_max = LWS_ARRAY_SIZE(results);
	args.count_tests = &count_tests;
	args.count_passes = &count_passes;
	args.cb = tests_completion_cb;
	args.cb_user = NULL;

	if (lws_abs_unit_test_sequencer(&args)) {
		lwsl_err("%s: failed to create test sequencer\n", __func__);
		goto bail1;
	}

	/* the usual lws event loop */

	while (n >= 0 && !interrupted)
		n = lws_service(context, 0);

	/* describe the overall test results */

	lwsl_user("%s: %d tests %d fail\n", __func__, count_tests,
			count_tests - count_passes);
	for (n = 0; n < count_tests; n++)
		lwsl_user("  test %d: %s\n", n,
			  lws_unit_test_result_name(results[n]));

bail1:
	lwsl_user("Completed: %s\n",
		  !count_tests || count_passes != count_tests ? "FAIL" : "PASS");

	lws_context_destroy(context);

	lws_abstract_free(&abs);

	return !count_tests || count_passes != count_tests;
}
