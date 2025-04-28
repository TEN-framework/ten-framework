/*
 * lws-minimal-secure-streams-avs
 *
 * This file is made available under the Creative Commons CC0 1.0
 * Universal Public Domain Dedication.
 */

#include <libwebsockets.h>
#include <string.h>
#include <signal.h>

extern int
avs_example_start(struct lws_context *context);

int interrupted, bad = 1;
static lws_state_notify_link_t nl;

static const char *canned_root_token_payload =
	"grant_type=refresh_token"
	"&refresh_token=Atzr|IwEBIJedGXjDqsU_vMxykqOMg"
	"SHfYe3CPcedueWEMWSDMaDnEmiW8RlR1Kns7Cb4B-TOSnqp7ifVsY4BMY2B8tpHfO39XP"
	"zfu9HapGjTR458IyHX44FE71pWJkGZ79uVBpljP4sazJuk8XS3Oe_yLnm_DIO6fU1nU3Y"
	"0flYmsOiOAQE_gRk_pdlmEtHnpMA-9rLw3mkY5L89Ty9kUygBsiFaYatouROhbsTn8-jW"
	"k1zZLUDpT6ICtBXSnrCIg0pUbZevPFhTwdXd6eX-u4rq0W-XaDvPWFO7au-iPb4Zk5eZE"
	"iX6sissYrtNmuEXc2uHu7MnQO1hHCaTdIO2CANVumf-PHSD8xseamyh04sLV5JgFzY45S"
	"KvKMajiUZuLkMokOx86rjC2Hdkx5DO7G-dbG1ufBDG-N79pFMSs7Ck5pc283IdLoJkCQc"
	"AGvTX8o8I29QqkcGou-9TKhOJmpX8As94T61ok0UqqEKPJ7RhfQHHYdCtsdwxgvfVr9qI"
	"xL_hDCcTho8opCVX-6QhJHl6SQFlTw13"
	"&client_id="
		"amzn1.application-oa2-client.4823334c434b4190a2b5a42c07938a2d";

static int
app_system_state_nf(lws_state_manager_t *mgr, lws_state_notify_link_t *link,
		    int current, int target)
{
	struct lws_context *context = lws_system_context_from_system_mgr(mgr);
	lws_system_blob_t *ab = lws_system_get_blob(context,
				LWS_SYSBLOB_TYPE_AUTH, 1 /* AUTH_IDX_ROOT */);
	size_t size;

	/*
	 * For the things we care about, let's notice if we are trying to get
	 * past them when we haven't solved them yet, and make the system
	 * state wait while we trigger the dependent action.
	 */
	switch (target) {
	case LWS_SYSTATE_REGISTERED:
		size = lws_system_blob_get_size(ab);
		if (size)
			break;

		/* let's register our canned root token so auth can use it */
		lws_system_blob_direct_set(ab,
				(const uint8_t *)canned_root_token_payload,
				strlen(canned_root_token_payload));
		break;
	case LWS_SYSTATE_OPERATIONAL:
		if (current == LWS_SYSTATE_OPERATIONAL)
			avs_example_start(context);
		break;
	case LWS_SYSTATE_POLICY_INVALID:
		/*
		 * This is a NOP since we used direct set... but in a real
		 * system this could easily change to be done on the heap, then
		 * this would be important
		 */
		lws_system_blob_destroy(lws_system_get_blob(context,
					LWS_SYSBLOB_TYPE_AUTH,
					1 /* AUTH_IDX_ROOT */));
		break;
	}

	return 0;
}

static void
sigint_handler(int sig)
{
	interrupted = 1;
}

static lws_state_notify_link_t * const app_notifier_list[] = {
	&nl, NULL
};

int main(int argc, const char **argv)
{
	struct lws_context_creation_info info;
	struct lws_context *context;
	int n = 0;

	signal(SIGINT, sigint_handler);
	memset(&info, 0, sizeof info); /* otherwise uninitialized garbage */
	lws_cmdline_option_handle_builtin(argc, argv, &info);

	lwsl_user("LWS secure streams - AVS test client [-d<verb>]\n");

	info.options = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;
	info.fd_limit_per_thread = 1 + 6 + 1;
	info.protocols = lws_sspc_protocols;
	info.port = CONTEXT_PORT_NO_LISTEN;

	/* integrate us with lws system state management when context created */
	nl.name = "app";
	nl.notify_cb = app_system_state_nf;
	info.register_notifier_list = app_notifier_list;

	context = lws_create_context(&info);
	if (!context) {
		lwsl_err("lws init failed\n");
		return 1;
	}

	/* the event loop */

	while (n >= 0 && !interrupted)
		n = lws_service(context, 0);

	lws_context_destroy(context);
	lwsl_user("Completed: %s\n", bad ? "failed" : "OK");

	return bad;
}
