/*
 * lws-minimal-raw-netcat
 *
 * Written in 2010-2019 by Andy Green <andy@warmcat.com>
 *
 * This file is made available under the Creative Commons CC0 1.0
 * Universal Public Domain Dedication.
 *
 * This demonstrates sending stdin to a remote socket and printing
 * what is returned to stdout.
 *
 * All the logging is on stderr, so you can tune it out with 2>log
 * or whatever.
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

static struct lws *raw_wsi, *stdin_wsi;
static uint8_t buf[LWS_PRE + 4096];
static int waiting, interrupted;
static struct lws_context *context;
static int us_wait_after_input_close = LWS_USEC_PER_SEC / 10;

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

void sigint_handler(int sig)
{
	interrupted = 1;
}

int main(int argc, const char **argv)
{
	const char *server = "libwebsockets.org", *port = "80";
	struct lws_context_creation_info info;
	lws_sock_file_fd_type sock;
	struct addrinfo h, *r, *rp;
	struct lws_vhost *vhost;
	const char *p;
	int n = 0, logs = LLL_USER | LLL_ERR | LLL_WARN | LLL_NOTICE;

	signal(SIGINT, sigint_handler);

	if ((p = lws_cmdline_option(argc, argv, "-d")))
		logs = atoi(p);

	lws_set_log_level(logs, NULL);
	lwsl_user("LWS minimal raw netcat [--server ip] [--port port] [-w ms]\n");

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

	if ((p = lws_cmdline_option(argc, argv, "--port")))
		port = p;

	if ((p = lws_cmdline_option(argc, argv, "--server")))
		server = p;

	if ((p = lws_cmdline_option(argc, argv, "-w")))
		us_wait_after_input_close = 1000 * atoi(p);

	n = getaddrinfo(server, port, &h, &r);
	if (n) {
		lwsl_err("%s: problem resolving %s: %s\n", __func__, 
			 server, gai_strerror(n));
		return 1;
	}

	for (rp = r; rp; rp = rp->ai_next) {
		sock.sockfd = socket(rp->ai_family, rp->ai_socktype,
				     rp->ai_protocol);
		if (sock.sockfd != LWS_SOCK_INVALID)
			break;
	}
	if (!rp) {
		lwsl_err("%s: unable to create INET socket\n", __func__);
		freeaddrinfo(r);

		return 1;
	}

	lwsl_user("Starting connect to %s:%s...\n", server, port);
	if (connect(sock.sockfd, rp->ai_addr, sizeof(*rp->ai_addr)) < 0) {
		lwsl_err("%s: unable to connect\n", __func__);
		freeaddrinfo(r);
		return 1;
	}

	freeaddrinfo(r);
	signal(SIGINT, sigint_handler);
	lwsl_user("Connected...\n");

	/* our foreign socket is connected... adopt it into lws */

	raw_wsi = lws_adopt_descriptor_vhost(vhost, LWS_ADOPT_SOCKET, sock,
					     protocols[0].name, NULL);
	if (!raw_wsi) {
		lwsl_err("%s: foreign socket adoption failed\n", __func__);
		goto bail;
	}

	sock.filefd = 0;
	stdin_wsi = lws_adopt_descriptor_vhost(vhost, LWS_ADOPT_RAW_FILE_DESC,
					       sock, protocols[0].name, NULL);
	if (!stdin_wsi) {
		lwsl_err("%s: stdin adoption failed\n", __func__);
		goto bail;
	}

	while (n >= 0 && !interrupted)
		n = lws_service(context, 0);

bail:

	lwsl_user("%s: destroying context\n", __func__);

	lws_context_destroy(context);

	return 0;
}
