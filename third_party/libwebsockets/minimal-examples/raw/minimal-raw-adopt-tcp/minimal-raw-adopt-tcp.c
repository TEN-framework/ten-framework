/*
 * lws-minimal-raw-adopt-tcp
 *
 * Written in 2010-2019 by Andy Green <andy@warmcat.com>
 *
 * This file is made available under the Creative Commons CC0 1.0
 * Universal Public Domain Dedication.
 *
 * This demonstrates integrating somebody else's connected tcp
 * socket into the lws event loop as a RAW wsi.  It's interesting in
 * the kind of situation where you already have a connected socket
 * in your application, and you need to hand it over to lws to deal with.
 *
 * Lws supports "adopting" these foreign sockets.
 *
 * If you simply want a connected client raw socket using lws alone, you
 * can just use lws_client_connect_via_info() with info.method = "RAW".
 *
 */

#include <libwebsockets.h>
#include <string.h>
#include <signal.h>
#if !defined(WIN32)
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#endif
#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#if !defined(WIN32)
#include <unistd.h>
#endif
#include <errno.h>

static int
callback_raw_test(struct lws *wsi, enum lws_callback_reasons reason,
			void *user, void *in, size_t len)
{

	switch (reason) {

	/* callbacks related to raw socket descriptor */

        case LWS_CALLBACK_RAW_ADOPT:
		lwsl_user("LWS_CALLBACK_RAW_ADOPT\n");
		lws_callback_on_writable(wsi);
                break;

	case LWS_CALLBACK_RAW_CLOSE:
		lwsl_user("LWS_CALLBACK_RAW_CLOSE\n");
		break;

	case LWS_CALLBACK_RAW_RX:
		lwsl_user("LWS_CALLBACK_RAW_RX (%d)\n", (int)len);
		lwsl_hexdump_level(LLL_NOTICE, in, len);
		break;

	case LWS_CALLBACK_RAW_WRITEABLE:
		if (lws_write(wsi,
			      (uint8_t *)"GET / HTTP/1.1\xd\xa\xd\xa", 18,
			      LWS_WRITE_RAW) != 18) {
			lwsl_notice("%s: raw write failed\n", __func__);
			return 1;
		}
		break;

	default:
		break;
	}

	return 0;
}

static struct lws_protocols protocols[] = {
	{ "raw-test", callback_raw_test, 0, 0, 0, NULL, 0 },
	LWS_PROTOCOL_LIST_TERM
};

static int interrupted;

void sigint_handler(int sig)
{
	interrupted = 1;
}

int main(int argc, const char **argv)
{
	struct lws_context_creation_info info;
	struct lws_context *context;
	lws_sock_file_fd_type sock;
	struct addrinfo h, *r, *rp;
	struct lws_vhost *vhost;
	const char *p;
	int n = 0, logs = LLL_USER | LLL_ERR | LLL_WARN | LLL_NOTICE
			/* for LLL_ verbosity above NOTICE to be built into lws,
			 * lws must have been configured and built with
			 * -DCMAKE_BUILD_TYPE=DEBUG instead of =RELEASE */
			/* | LLL_INFO */ /* | LLL_PARSER */ /* | LLL_HEADER */
			/* | LLL_EXT */ /* | LLL_CLIENT */ /* | LLL_LATENCY */
			/* | LLL_DEBUG */;

	signal(SIGINT, sigint_handler);

	if ((p = lws_cmdline_option(argc, argv, "-d")))
		logs = atoi(p);

	lws_set_log_level(logs, NULL);
	lwsl_user("LWS minimal raw adopt tcp\n");

	memset(&info, 0, sizeof info); /* otherwise uninitialized garbage */
	info.options = LWS_SERVER_OPTION_EXPLICIT_VHOSTS;

	context = lws_create_context(&info);
	if (!context) {
		lwsl_err("lws init failed\n");
		return 1;
	}

	info.port = CONTEXT_PORT_NO_LISTEN_SERVER;
	info.protocols = protocols;

	vhost = lws_create_vhost(context, &info);
	if (!vhost) {
		lwsl_err("lws vhost creation failed\n");
		goto bail;
	}

	/*
	 * Connect our own "foreign" socket to libwebsockets.org:80
	 *
	 * Normally you would do this with lws_client_connect_via_info() inside
	 * the lws event loop, hiding all this detail.  But this example
	 * demonstrates how to integrate an externally-connected "foreign"
	 * socket, so we create one by hand.
	 */

	memset(&h, 0, sizeof(h));
	h.ai_family = AF_UNSPEC;    /* Allow IPv4 or IPv6 */
	h.ai_socktype = SOCK_STREAM;
	h.ai_protocol = IPPROTO_TCP;

	n = getaddrinfo("libwebsockets.org", "80", &h, &r);
	if (n) {
		lwsl_err("%s: problem resolving libwebsockets.org: %s\n", __func__, gai_strerror(n));
		return 1;
	}

	for (rp = r; rp; rp = rp->ai_next) {
		sock.sockfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
		if (sock.sockfd != LWS_SOCK_INVALID)
			break;
	}
	if (!rp) {
		lwsl_err("%s: unable to create INET socket\n", __func__);
		freeaddrinfo(r);

		return 1;
	}

	lwsl_user("Starting connect...\n");
	if (connect(sock.sockfd, rp->ai_addr, sizeof(*rp->ai_addr)) < 0) {
		lwsl_err("%s: unable to connect to libwebsockets.org:80\n", __func__);
		freeaddrinfo(r);
		return 1;
	}

	freeaddrinfo(r);
	signal(SIGINT, sigint_handler);
	lwsl_user("Connected...\n");

	/* our foreign socket is connected... adopt it into lws */

	if (!lws_adopt_descriptor_vhost(vhost, LWS_ADOPT_SOCKET, sock,
				       protocols[0].name, NULL)) {
		lwsl_err("%s: foreign socket adoption failed\n", __func__);
		goto bail;
	}

	while (n >= 0 && !interrupted)
		n = lws_service(context, 0);

bail:
	lws_context_destroy(context);

	return 0;
}
