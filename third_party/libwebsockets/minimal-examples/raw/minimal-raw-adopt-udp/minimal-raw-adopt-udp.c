/*
 * lws-minimal-raw-adopt-udp
 *
 * Written in 2010-2019 by Andy Green <andy@warmcat.com>
 *
 * This file is made available under the Creative Commons CC0 1.0
 * Universal Public Domain Dedication.
 *
 * This demonstrates integrating a connected udp
 * socket into the lws event loop as a RAW wsi.  It's interesting in
 * the kind of situation where you already have a connected socket
 * in your application, and you need to hand it over to lws to deal with.
 *
 * Lws supports "adopting" these foreign sockets, and also has a helper API
 * to create, bind, and adopt them inside lws.
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

static uint8_t sendbuf[4096];
static size_t sendlen;
struct lws_udp udp;

static int
callback_raw_test(struct lws *wsi, enum lws_callback_reasons reason,
			void *user, void *in, size_t len)
{
	ssize_t n;
	lws_sockfd_type fd;

	switch (reason) {

	/* callbacks related to raw socket descriptor */

        case LWS_CALLBACK_RAW_ADOPT:
		lwsl_user("LWS_CALLBACK_RAW_ADOPT\n");
                break;

	case LWS_CALLBACK_RAW_CLOSE:
		lwsl_user("LWS_CALLBACK_RAW_CLOSE\n");
		break;

	case LWS_CALLBACK_RAW_RX:
		lwsl_user("LWS_CALLBACK_RAW_RX (%d)\n", (int)len);
		lwsl_hexdump_level(LLL_NOTICE, in, len);
		/*
		 * Take a copy of the buffer and the source socket address...
		 */
		udp = *(lws_get_udp(wsi));
		sendlen = len;
		if (sendlen > sizeof(sendbuf))
			sendlen = sizeof(sendbuf);
		memcpy(sendbuf, in, sendlen);
		/*
		 * ... and we send it next time around the event loop.  This
		 * can be extended to having a ringbuffer of different send
		 * buffers and targets queued.
		 *
		 * Note that UDP is ALWAYS writable as far as poll() knows
		 * because there is no mechanism like the tcp window to
		 * understand that packets are not being acknowledged.  But
		 * this allows the event loop to share out the work.
		 */
		lws_callback_on_writable(wsi);
		break;

	case LWS_CALLBACK_RAW_WRITEABLE:

		if (!sendlen)
			break;

		fd = lws_get_socket_fd(wsi);
#if defined(WIN32)
		if ((int)fd < 0)
			break;
#else
		if (fd < 0) /* keep Coverity happy: actually it cannot be < 0 */
			break;
#endif

		/*
		 * We can write directly on the UDP socket, specifying
		 * the peer the write is directed to.
		 *
		 * However the kernel may only accept parts of large sendto()s,
		 * leaving you to try to resend the remainder later.  However
		 * depending on how your protocol on top of UDP works, that
		 * may involve sticking new headers before the remainder.
		 *
		 * For clarity partial sends just drop the remainder here.
		 */
		n = sendto(fd,
#if defined(WIN32)
				(const char *)
#endif
			sendbuf,
#if defined(WIN32)
			(int)
#endif
			sendlen, 0, sa46_sockaddr(&udp.sa46),
			sa46_socklen(&udp.sa46));
		if (n < (ssize_t)len)
			lwsl_notice("%s: send returned %d\n", __func__, (int)n);
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
	lwsl_user("LWS minimal raw adopt udp | nc -u 127.0.0.1 7681\n");

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
	 * Create our own "foreign" UDP socket bound to 7681/udp
	 */
	if (!lws_create_adopt_udp(vhost, NULL, 7681, LWS_CAUDP_BIND,
				  protocols[0].name, NULL, NULL, NULL, NULL,
				  "user")) {
		lwsl_err("%s: foreign socket adoption failed\n", __func__);
		goto bail;
	}

	while (n >= 0 && !interrupted)
		n = lws_service(context, 0);

bail:
	lws_context_destroy(context);

	return 0;
}
