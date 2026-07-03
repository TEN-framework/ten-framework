/*
 * lws-minimal-raw-client
 *
 * Written in 2010-2022 by Andy Green <andy@warmcat.com>
 *
 * This file is made available under the Creative Commons CC0 1.0
 * Universal Public Domain Dedication.
 *
 * This demonstrates connecting a "raw" client connection
 */

#include <libwebsockets.h>
#include <string.h>
#include <signal.h>
#if !defined(WIN32)
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#endif
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#if !defined(WIN32)
#include <unistd.h>
#endif
#include <errno.h>

#include <assert.h>

static struct lws *raw_wsi, *stdin_wsi;
static uint8_t buf[LWS_PRE + 4096];
static int waiting, interrupted;
static struct lws_context *context;
static int us_wait_after_input_close = LWS_USEC_PER_SEC / 10;

static const char *server = "libwebsockets.org", *port = "443";

static int
callback_raw_test(struct lws *wsi, enum lws_callback_reasons reason,
		  void *user, void *in, size_t len)
{
	const char *cp = (const char *)in;

	switch (reason) {

	/* callbacks related to file descriptor */

        case LWS_CALLBACK_RAW_ADOPT_FILE:
        	lwsl_user("LWS_CALLBACK_RAW_ADOPT_FILE\n");
                break;

	case LWS_CALLBACK_RAW_CLOSE_FILE:
		lwsl_user("LWS_CALLBACK_RAW_CLOSE_FILE\n");
		/* stdin close, wait 1s then close the raw skt */
		stdin_wsi = NULL; /* invalid now we close */
		if (raw_wsi)
			lws_set_timer_usecs(raw_wsi, us_wait_after_input_close);
		else {
			interrupted = 1;
			lws_cancel_service(context);
		}
		break;

	case LWS_CALLBACK_RAW_RX_FILE:
		lwsl_user("LWS_CALLBACK_RAW_RX_FILE\n");
		waiting = (int)read(0, buf, sizeof(buf));
		lwsl_notice("raw file read %d\n", waiting);
		if (waiting < 0)
			return -1;

		if (raw_wsi)
			lws_callback_on_writable(raw_wsi);
		lws_rx_flow_control(wsi, 0);
		break;


	/* callbacks related to raw socket descriptor */

        case LWS_CALLBACK_RAW_ADOPT:
		lwsl_user("LWS_CALLBACK_RAW_ADOPT\n");
		lws_callback_on_writable(wsi);
                break;

        case LWS_CALLBACK_RAW_CONNECTED:
        	lwsl_user("LWS_CALLBACK_RAW_CONNECTED\n");
        	break;

	case LWS_CALLBACK_RAW_CLOSE:
		lwsl_user("LWS_CALLBACK_RAW_CLOSE\n");
		/*
		 * If the socket to the remote server closed, we must close
		 * and drop any remaining stdin
		 */
		interrupted = 1;
		lws_cancel_service(context);
		/* our pointer to this wsi is invalid now we close */
		raw_wsi = NULL;
		break;

	case LWS_CALLBACK_RAW_RX:
		lwsl_user("LWS_CALLBACK_RAW_RX (%d)\n", (int)len);
		while (len--)
			putchar(*cp++);
		fflush(stdout);
		break;

	case LWS_CALLBACK_RAW_WRITEABLE:
		lwsl_user("LWS_CALLBACK_RAW_WRITEABLE\n");
		// lwsl_hexdump_info(buf, waiting);
		if (!waiting)
			break;
		if (stdin_wsi)
			lws_rx_flow_control(stdin_wsi, 1);
		if (lws_write(wsi, buf, (unsigned int)waiting, LWS_WRITE_RAW) != waiting) {
			lwsl_notice("%s: raw skt write failed\n", __func__);

			return -1;
		}
		break;

	case LWS_CALLBACK_TIMER:
		lwsl_user("LWS_CALLBACK_TIMER\n");
		interrupted = 1;
		lws_cancel_service(context);
		return -1;

	default:
		break;
	}

	return 0;
}

static struct lws_protocols protocols[] = {
	{ "raw-test", callback_raw_test, 0, 0, 0, NULL, 0 },
	LWS_PROTOCOL_LIST_TERM
};

static int
system_notify_cb(lws_state_manager_t *mgr, lws_state_notify_link_t *link,
		   int current, int target)
{
	struct lws_client_connect_info i;

	if (current != LWS_SYSTATE_OPERATIONAL ||
	    target != LWS_SYSTATE_OPERATIONAL)
		return 0;

	memset(&i, 0, sizeof i);
	i.context		= context;
	i.method		= "RAW";
	i.ssl_connection	= LCCSCF_USE_SSL;
	i.alpn			= "http/1.1";
	i.address		= server;
	i.host			= server;
	i.port			= atoi(port);
	i.local_protocol_name	= "raw-test";

	waiting = lws_snprintf((char *)buf, sizeof(buf), "GET / HTTP/1.1\xaHost: libwebsockets.org\xa\xa");

        if (!lws_client_connect_via_info(&i)) {
                lwsl_err("Client creation failed\n");
                interrupted = 1;
        }

	return 0;
}

void sigint_handler(int sig)
{
	interrupted = 1;
}

int main(int argc, const char **argv)
{
	struct lws_context_creation_info info;
	const char *p;
	int n = 0, logs = LLL_USER | LLL_ERR | LLL_WARN | LLL_NOTICE;
	lws_state_notify_link_t notifier = { { NULL, NULL, NULL },
					     system_notify_cb, "app" };
	lws_state_notify_link_t *na[] = { &notifier, NULL };

	signal(SIGINT, sigint_handler);

	if ((p = lws_cmdline_option(argc, argv, "-d")))
		logs = atoi(p);

	lws_set_log_level(logs, NULL);
	lwsl_user("LWS minimal raw client\n");

	memset(&info, 0, sizeof info);

	info.options = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;
	info.port = CONTEXT_PORT_NO_LISTEN_SERVER;
	info.protocols = protocols;
	info.register_notifier_list	= na;

	context = lws_create_context(&info);
	if (!context) {
		lwsl_err("lws init failed\n");
		return 1;
	}

	while (n >= 0 && !interrupted)
		n = lws_service(context, 0);

	lwsl_user("%s: destroying context\n", __func__);

	lws_context_destroy(context);

	return 0;
}
