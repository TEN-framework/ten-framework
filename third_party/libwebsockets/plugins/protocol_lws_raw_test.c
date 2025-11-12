/*
 * ws protocol handler plugin for testing raw file and raw socket
 *
 * Written in 2010-2019 by Andy Green <andy@warmcat.com>
 *
 * This file is made available under the Creative Commons CC0 1.0
 * Universal Public Domain Dedication.
 *
 * The person who associated a work with this deed has dedicated
 * the work to the public domain by waiving all of his or her rights
 * to the work worldwide under copyright law, including all related
 * and neighboring rights, to the extent allowed by law. You can copy,
 * modify, distribute and perform the work, even for commercial purposes,
 * all without asking permission.
 *
 * These test plugins are intended to be adapted for use in your code, which
 * may be proprietary.  So unlike the library itself, they are licensed
 * Public Domain.
 *
 * This plugin test both raw file descriptors and raw socket descriptors.  It
 * can test both or just one depending on how you configure it.  libwebsockets-
 * test-server-v2.0 is set up to test both.
 *
 * RAW File Descriptor Testing
 * ===========================
 *
 * Enable on a vhost like this
 *
 *        "protocol-lws-raw-test": {
 *                 "status": "ok",
 *                 "fifo-path": "/tmp/lws-test-raw"
 *        },
 *
 * Then you can feed it data through the FIFO like this
 *
 *  $ sudo sh -c "echo hello > /tmp/lws-test-raw"
 *
 * This plugin simply prints the data.  But it does it through the lws event
 * loop / service poll.
 *
 *
 * RAW Socket Descriptor Testing
 * =============================
 *
 * 1) You must give the vhost the option flag
 * 	LWS_SERVER_OPTION_FALLBACK_TO_APPLY_LISTEN_ACCEPT_CONFIG
 *
 * 2) Enable on a vhost like this
 *
 *        "protocol-lws-raw-test": {
 *                 "status": "ok",
 *                 "raw": "1"
 *        },
 *
 *    The "raw" pvo marks this protocol as being used for RAW connections.
 *
 * 3) Run libwebsockets-test-server-v2.0 and connect to it by telnet, eg
 *
 *    telnet 127.0.0.1 7681
 *
 *    type something that isn't a valid HTTP method and enter, before the
 *    connection times out.  The connection will switch to RAW mode using this
 *    protocol, and pass the unused rx as a raw RX callback.
 *
 *    The test protocol echos back what was typed on telnet to telnet.
 */

#if !defined (LWS_PLUGIN_STATIC)
#if !defined(LWS_DLL)
#define LWS_DLL
#endif
#if !defined(LWS_INTERNAL)
#define LWS_INTERNAL
#endif
#include <libwebsockets.h>
#endif

#include <string.h>
#include <fcntl.h>

#include <sys/stat.h>

struct per_vhost_data__raw_test {
	struct lws_context *context;
	struct lws_vhost *vhost;
	const struct lws_protocols *protocol;
	char fifo_path[100];
	int fifo;
	char zero_length_read;
};

struct per_session_data__raw_test {
	int number;
	unsigned char buf[128];
	int len;
};

static int
callback_raw_test(struct lws *wsi, enum lws_callback_reasons reason, void *user,
		  void *in, size_t len)
{
	struct per_session_data__raw_test *pss =
			(struct per_session_data__raw_test *)user;
	struct per_vhost_data__raw_test *vhd =
			(struct per_vhost_data__raw_test *)
			lws_protocol_vh_priv_get(lws_get_vhost(wsi),
					lws_get_protocol(wsi));
	lws_sock_file_fd_type u;

	(void)pss;

	switch (reason) {
	case LWS_CALLBACK_PROTOCOL_INIT:
		vhd = lws_protocol_vh_priv_zalloc(lws_get_vhost(wsi),
				lws_get_protocol(wsi),
				sizeof(struct per_vhost_data__raw_test));
		if (!vhd)
			return 0;
		vhd->context = lws_get_context(wsi);
		vhd->protocol = lws_get_protocol(wsi);
		vhd->vhost = lws_get_vhost(wsi);
		{
			const struct lws_protocol_vhost_options *pvo =
				(const struct lws_protocol_vhost_options *)in;
			while (pvo) {
				if (!strcmp(pvo->name, "fifo-path"))
					lws_strncpy(vhd->fifo_path, pvo->value,
							sizeof(vhd->fifo_path));
				pvo = pvo->next;
			}
			if (vhd->fifo_path[0] == '\0') {
				lwsl_warn("%s: Missing pvo \"fifo-path\", "
					 "raw file fd testing disabled\n",
					 __func__);
				break;
			}
		}
		unlink(vhd->fifo_path);
		if (mkfifo(vhd->fifo_path, 0666)) {
			lwsl_err("mkfifo failed\n");
			return 1;
		}
		vhd->fifo = lws_open(vhd->fifo_path, O_NONBLOCK | O_RDONLY);
		if (vhd->fifo == -1) {
			lwsl_err("opening fifo failed\n");
			unlink(vhd->fifo_path);
			return 1;
		}
		lwsl_notice("FIFO %s created\n", vhd->fifo_path);
		u.filefd = vhd->fifo;
		if (!lws_adopt_descriptor_vhost(vhd->vhost,
						LWS_ADOPT_RAW_FILE_DESC, u,
						"protocol-lws-raw-test",
						NULL)) {
			lwsl_err("Failed to adopt fifo descriptor\n");
			close(vhd->fifo);
			unlink(vhd->fifo_path);
			return 1;
		}
		break;

	case LWS_CALLBACK_PROTOCOL_DESTROY:
		if (!vhd)
			break;
		if (vhd->fifo >= 0) {
			close(vhd->fifo);
			unlink(vhd->fifo_path);
		}
		break;


	/*
	 * Callbacks related to Raw file descriptor testing
	 */

	case LWS_CALLBACK_RAW_ADOPT_FILE:
		lwsl_notice("LWS_CALLBACK_RAW_ADOPT_FILE\n");
		break;


	case LWS_CALLBACK_RAW_RX_FILE:
		lwsl_notice("LWS_CALLBACK_RAW_RX_FILE\n");
		{
			char buf[256];
			int n;
			
			n = (int)read(vhd->fifo, buf, sizeof(buf) - 1);
			if (n < 0) {
				lwsl_err("FIFO read failed\n");
				return 1;
			}
			/*
			 * When nobody opened the other side of the FIFO, the
			 * FIFO fd acts well and only signals POLLIN when
			 * somebody opened and wrote to it.
			 *
			 * But if the other side of the FIFO closed it, we will
			 * see an endless POLLIN and 0 available to read.
			 *
			 * The only way to handle it is to reopen the FIFO our
			 * side and wait for a new peer.  This is a quirk of
			 * FIFOs not of LWS.
			 */
			if (n == 0) { /* peer closed - reopen in close processing */
				vhd->zero_length_read = 1;
				return 1;
			}
			buf[n] = '\0';
			lwsl_info("read %d\n", n);
			puts(buf);
		}
		break;

	case LWS_CALLBACK_RAW_CLOSE_FILE:
		lwsl_notice("LWS_CALLBACK_RAW_CLOSE_FILE\n");
		if (vhd->zero_length_read) {
			vhd->zero_length_read = 0;
			close(vhd->fifo);
			/* the wsi that adopted the fifo file is closing...
			 * reopen the fifo and readopt
			 */
			vhd->fifo = lws_open(vhd->fifo_path,
					     O_NONBLOCK | O_RDONLY);
			if (vhd->fifo == -1) {
				lwsl_err("opening fifo failed\n");
				return 1;
			}
			lwsl_notice("FIFO %s reopened\n", vhd->fifo_path);
			u.filefd = vhd->fifo;
			if (!lws_adopt_descriptor_vhost(vhd->vhost, 0, u,
					"protocol-lws-raw-test", NULL)) {
				lwsl_err("Failed to adopt fifo descriptor\n");
				close(vhd->fifo);
				return 1;
			}
		}
		break;

	case LWS_CALLBACK_RAW_WRITEABLE_FILE:
		lwsl_notice("LWS_CALLBACK_RAW_WRITEABLE_FILE\n");
		break;

	/*
	 * Callbacks related to Raw socket descriptor testing
	 */

	case LWS_CALLBACK_RAW_ADOPT:
		lwsl_notice("LWS_CALLBACK_RAW_ADOPT\n");
		break;

	case LWS_CALLBACK_RAW_RX:
		lwsl_notice("LWS_CALLBACK_RAW_RX %ld\n", (long)len);
		if (len > sizeof(pss->buf))
			len = sizeof(pss->buf);
		memcpy(pss->buf, in, len);
		pss->len = (int)len;
		lws_callback_on_writable(wsi);
		break;

	case LWS_CALLBACK_RAW_CLOSE:
		lwsl_notice("LWS_CALLBACK_RAW_CLOSE\n");
		break;

	case LWS_CALLBACK_RAW_WRITEABLE:
		lwsl_notice("LWS_CALLBACK_RAW_WRITEABLE\n");
		lws_write(wsi, pss->buf, (size_t)pss->len, LWS_WRITE_HTTP);
		break;

	default:
		break;
	}

	return 0;
}

#define LWS_PLUGIN_PROTOCOL_RAW_TEST \
	{ \
		"protocol-lws-raw-test", \
		callback_raw_test, \
		sizeof(struct per_session_data__raw_test), \
		1024, /* rx buf size must be >= permessage-deflate rx size */ 0, NULL, 0\
	}

#if !defined (LWS_PLUGIN_STATIC)
		
LWS_VISIBLE const struct lws_protocols lws_raw_test_protocols[] = {
	LWS_PLUGIN_PROTOCOL_RAW_TEST
};

LWS_VISIBLE const lws_plugin_protocol_t lws_raw_test = {
	.hdr = {
		"lws raw test",
		"lws_protocol_plugin",
		LWS_BUILD_HASH,
		LWS_PLUGIN_API_MAGIC
	},

	.protocols = lws_raw_test_protocols,
	.count_protocols = LWS_ARRAY_SIZE(lws_raw_test_protocols),
	.extensions = NULL,
	.count_extensions = 0,
};

#endif
