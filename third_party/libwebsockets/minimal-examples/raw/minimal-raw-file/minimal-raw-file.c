/*
 * lws-minimal-raw-file
 *
 * Written in 2010-2019 by Andy Green <andy@warmcat.com>
 *
 * This file is made available under the Creative Commons CC0 1.0
 * Universal Public Domain Dedication.
 *
 * This demonstrates adopting a file descriptor into the lws event
 * loop.
 */

#include <libwebsockets.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

struct raw_vhd {
//	lws_sock_file_fd_type u;
	int filefd;
};

static char filepath[256];

static int
callback_raw_test(struct lws *wsi, enum lws_callback_reasons reason,
			void *user, void *in, size_t len)
{
	struct raw_vhd *vhd = (struct raw_vhd *)lws_protocol_vh_priv_get(
				     lws_get_vhost(wsi), lws_get_protocol(wsi));
	lws_sock_file_fd_type u;
	uint8_t buf[1024];
	int n;

	switch (reason) {
	case LWS_CALLBACK_PROTOCOL_INIT:
		vhd = lws_protocol_vh_priv_zalloc(lws_get_vhost(wsi),
				lws_get_protocol(wsi), sizeof(struct raw_vhd));
		vhd->filefd = lws_open(filepath, O_RDWR);
		if (vhd->filefd == -1) {
			lwsl_err("Unable to open %s\n", filepath);

			return 1;
		}
		u.filefd = (lws_filefd_type)(long long)vhd->filefd;
		if (!lws_adopt_descriptor_vhost(lws_get_vhost(wsi),
						LWS_ADOPT_RAW_FILE_DESC, u,
						"raw-test", NULL)) {
			lwsl_err("Failed to adopt fifo descriptor\n");
			close(vhd->filefd);
			vhd->filefd = -1;

			return 1;
		}
		break;

	case LWS_CALLBACK_PROTOCOL_DESTROY:
		if (vhd && vhd->filefd != -1)
			close(vhd->filefd);
		break;

	/* callbacks related to raw file descriptor */

	case LWS_CALLBACK_RAW_ADOPT_FILE:
		lwsl_notice("LWS_CALLBACK_RAW_ADOPT_FILE\n");
		break;

	case LWS_CALLBACK_RAW_RX_FILE:
		lwsl_notice("LWS_CALLBACK_RAW_RX_FILE\n");
		n = (int)read(vhd->filefd, buf, sizeof(buf));
		if (n < 0) {
			lwsl_err("Reading from %s failed\n", filepath);

			return 1;
		}
		lwsl_hexdump_level(LLL_NOTICE, buf, (unsigned int)n);
		break;

	case LWS_CALLBACK_RAW_CLOSE_FILE:
		lwsl_notice("LWS_CALLBACK_RAW_CLOSE_FILE\n");
		break;

	case LWS_CALLBACK_RAW_WRITEABLE_FILE:
		lwsl_notice("LWS_CALLBACK_RAW_WRITEABLE_FILE\n");
		/*
		 * you can call lws_callback_on_writable() on a raw file wsi as
		 * usual, and then write directly into the raw filefd here.
		 */
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
	lwsl_user("LWS minimal raw file\n");
	if (argc < 2) {
		lwsl_user("Usage: %s <file to monitor>  "
			  " eg, /dev/ttyUSB0 or /dev/input/event0 or "
			  "/proc/self/fd/0\n", argv[0]);

		return 1;
	}

	signal(SIGINT, sigint_handler);

	memset(&info, 0, sizeof info); /* otherwise uninitialized garbage */
	info.port = CONTEXT_PORT_NO_LISTEN_SERVER; /* no listen socket for demo */
	info.protocols = protocols;

	lws_strncpy(filepath, argv[1], sizeof(filepath));

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
