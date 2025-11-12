/*
 * lws-minimal-raw-file
 *
 * Written in 2010-2019 by Andy Green <andy@warmcat.com>
 *
 * This file is made available under the Creative Commons CC0 1.0
 * Universal Public Domain Dedication.
 *
 * This demonstrates dealing with a serial port
 */

#include <libwebsockets.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <termios.h>
#include <sys/ioctl.h>

#if defined(__linux__)
#include <asm/ioctls.h>
#include <linux/serial.h>
#endif

struct raw_vhd {
	lws_sorted_usec_list_t sul;
	struct lws *wsi;
	int filefd;
};

static char filepath[256];

static void
sul_cb(lws_sorted_usec_list_t *sul)
{
	struct raw_vhd *v = lws_container_of(sul, struct raw_vhd, sul);

	lws_callback_on_writable(v->wsi);

	lws_sul_schedule(lws_get_context(v->wsi), 0, &v->sul, sul_cb,
			 2 * LWS_USEC_PER_SEC);
}

static int
callback_raw_test(struct lws *wsi, enum lws_callback_reasons reason,
			void *user, void *in, size_t len)
{
	struct raw_vhd *vhd = (struct raw_vhd *)lws_protocol_vh_priv_get(
				     lws_get_vhost(wsi), lws_get_protocol(wsi));
#if defined(__linux__)
	struct serial_struct s_s;
#endif
	lws_sock_file_fd_type u;
	struct termios tio;
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

		tcflush(vhd->filefd, TCIOFLUSH);

#if defined(__linux__)
		if (ioctl(vhd->filefd, TIOCGSERIAL, &s_s) == 0) {
			s_s.closing_wait = ASYNC_CLOSING_WAIT_NONE;
			ioctl(vhd->filefd, TIOCSSERIAL, &s_s);
		}
#endif

		/* enforce suitable tty state */

		memset(&tio, 0, sizeof tio);
		if (tcgetattr(vhd->filefd, &tio)) {
			close(vhd->filefd);
			vhd->filefd = -1;
			return -1;
		}

		cfsetispeed(&tio, B115200);
		cfsetospeed(&tio, B115200);

		tio.c_lflag &= (tcflag_t)~(ISIG | ICANON | IEXTEN | ECHO |
#if defined(__linux__)
				XCASE |
#endif
				 ECHOE | ECHOK | ECHONL | ECHOCTL | ECHOKE);
		tio.c_iflag &= (tcflag_t)~(INLCR | IGNBRK | IGNPAR | IGNCR | ICRNL |
				 IMAXBEL | IXON | IXOFF | IXANY
#if defined(__linux__)
				 | IUCLC
#endif
				| 0xff);
		tio.c_oflag = 0;

		tio.c_cc[VMIN]  = 1;
		tio.c_cc[VTIME] = 0;
		tio.c_cc[VEOF] = 1;
		tio.c_cflag = tio.c_cflag & (unsigned long) ~(
#if defined(__linux__)
				CBAUD |
#endif
				CSIZE | CSTOPB | PARENB
#if !defined(__QNX__)
				| CRTSCTS
#endif
		);
		tio.c_cflag |= 0x1412 | CS8 | CREAD | CLOCAL;

		tcsetattr(vhd->filefd, TCSANOW, &tio);

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
		vhd->wsi = wsi;
		lws_sul_schedule(lws_get_context(wsi), 0, &vhd->sul, sul_cb, 1);
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
		lws_sul_cancel(&vhd->sul);
		break;

	case LWS_CALLBACK_RAW_WRITEABLE_FILE:
		lwsl_notice("LWS_CALLBACK_RAW_WRITEABLE_FILE\n");
		if (lws_write(wsi, (uint8_t *)"hello-this-is-written-every-couple-of-seconds\r\n", 47, LWS_WRITE_RAW) != 47)
			return -1;
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
	lwsl_user("LWS minimal raw serial\n");
	if (argc < 2) {
		lwsl_user("Usage: %s <serial device>  "
			  " eg, /dev/ttyUSB0\n", argv[0]);

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
