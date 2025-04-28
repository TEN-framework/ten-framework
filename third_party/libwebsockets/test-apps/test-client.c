/*
 * libwebsockets-test-client - libwebsockets test implementation
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
 * The test apps are intended to be adapted for use in your code, which
 * may be proprietary.  So unlike the library itself, they are licensed
 * Public Domain.
 */

#include "lws_config.h"

#include <stdio.h>
#include <stdlib.h>
#if defined(LWS_HAS_GETOPT_LONG) || defined(WIN32)
#include <getopt.h>
#endif
#include <string.h>
#include <signal.h>

#ifdef _WIN32
#define random rand
#include "gettimeofday.h"
#else
#include <syslog.h>
#include <sys/time.h>
#include <unistd.h>
#endif

#include <libwebsockets.h>

struct lws_poly_gen {
	uint32_t cyc[2];
};

#define block_size (3 * 4096)

static int deny_deflate, longlived, mirror_lifetime, test_post, once;
static struct lws *wsi_dumb, *wsi_mirror;
static struct lws *wsi_multi[3];
static volatile int force_exit;
static unsigned int opts, rl_multi[3];
static int flag_no_mirror_traffic, justmirror, flag_echo;
static uint32_t count_blocks = 1024, txb, rxb, rx_count, errs;
static struct lws_poly_gen tx = { { 0xabcde, 0x23456789 } },
			   rx = { { 0xabcde, 0x23456789 } }
;

#if defined(LWS_WITH_TLS) && defined(LWS_HAVE_SSL_CTX_set1_param)
char crl_path[1024] = "";
#endif

/*
 * This demo shows how to connect multiple websockets simultaneously to a
 * websocket server (there is no restriction on their having to be the same
 * server just it simplifies the demo).
 *
 *  dumb-increment-protocol:  we connect to the server and print the number
 *				we are given
 *
 *  lws-mirror-protocol: draws random circles, which are mirrored on to every
 *				client (see them being drawn in every browser
 *				session also using the test server)
 */

enum demo_protocols {

	PROTOCOL_DUMB_INCREMENT,
	PROTOCOL_LWS_MIRROR,

	/* always last */
	DEMO_PROTOCOL_COUNT
};

static uint8_t
lws_poly_rand(struct lws_poly_gen *p)
{
	p->cyc[0] = (p->cyc[0] & 1) ? (p->cyc[0] >> 1) ^ 0xb4bcd35c :
				      p->cyc[0] >> 1;
	p->cyc[0] = (p->cyc[0] & 1) ? (p->cyc[0] >> 1) ^ 0xb4bcd35c :
				      p->cyc[0] >> 1;
	p->cyc[1] = (p->cyc[1] & 1) ? (p->cyc[1] >> 1) ^ 0x7a5bc2e3 :
				      p->cyc[1] >> 1;

	return (uint8_t)(p->cyc[0] ^ p->cyc[1]);
}

static void show_http_content(const char *p, size_t l)
{
	if (lwsl_visible(LLL_INFO)) {
		while (l--)
			if (*p < 0x7f)
				putchar(*p++);
			else
				putchar('.');
	}
}


/*
 * dumb_increment protocol
 *
 * since this also happens to be protocols[0], some callbacks that are not
 * bound to a specific protocol also turn up here.
 */

static int
callback_dumb_increment(struct lws *wsi, enum lws_callback_reasons reason,
			void *user, void *in, size_t len)
{
#if defined(LWS_WITH_TLS)
	union lws_tls_cert_info_results ci;
#if defined(LWS_HAVE_CTIME_R) && !defined(LWS_WITH_NO_LOGS)
	char date[32];
#endif
#endif
	const char *which = "http";
       char which_wsi[50], buf[80 + LWS_PRE];
	int n;

	switch (reason) {

	case LWS_CALLBACK_CLIENT_ESTABLISHED:
		lwsl_info("dumb: LWS_CALLBACK_CLIENT_ESTABLISHED\n");
		break;

	case LWS_CALLBACK_CLOSED:
		lwsl_notice("dumb: LWS_CALLBACK_CLOSED\n");
		wsi_dumb = NULL;
		break;

	case LWS_CALLBACK_CLIENT_RECEIVE:
		((char *)in)[len] = '\0';
		lwsl_info("rx %d '%s'\n", (int)len, (char *)in);
		break;

	/* because we are protocols[0] ... */

	case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
		if (wsi == wsi_dumb) {
			which = "dumb";
			wsi_dumb = NULL;
		}
		if (wsi == wsi_mirror) {
			which = "mirror";
			wsi_mirror = NULL;
		}

		for (n = 0; n < (int)LWS_ARRAY_SIZE(wsi_multi); n++)
			if (wsi == wsi_multi[n]) {
				sprintf(which_wsi, "multi %d", n);
				which = which_wsi;
				wsi_multi[n] = NULL;
			}

		lwsl_err("CLIENT_CONNECTION_ERROR: %s: %s\n", which,
			 in ? (char *)in : "(null)");
		break;

	case LWS_CALLBACK_CLIENT_CONFIRM_EXTENSION_SUPPORTED:
		if ((strcmp((const char *)in, "deflate-stream") == 0) &&
		    deny_deflate) {
			lwsl_notice("denied deflate-stream extension\n");
			return 1;
		}
		if ((strcmp((const char *)in, "x-webkit-deflate-frame") == 0))
			return 1;
		if ((strcmp((const char *)in, "deflate-frame") == 0))
			return 1;
		break;

	case LWS_CALLBACK_ESTABLISHED_CLIENT_HTTP:
		lwsl_notice("lws_http_client_http_response %d\n",
				lws_http_client_http_response(wsi));
#if defined(LWS_WITH_TLS)
		if (!lws_tls_peer_cert_info(wsi, LWS_TLS_CERT_INFO_COMMON_NAME,
					    &ci, sizeof(ci.ns.name)))
			lwsl_notice(" Peer Cert CN        : %s\n", ci.ns.name);

		if (!lws_tls_peer_cert_info(wsi, LWS_TLS_CERT_INFO_ISSUER_NAME,
					    &ci, sizeof(ci.ns.name)))
			lwsl_notice(" Peer Cert issuer    : %s\n", ci.ns.name);

		if (!lws_tls_peer_cert_info(wsi, LWS_TLS_CERT_INFO_VALIDITY_FROM,
					    &ci, 0))
#if defined(LWS_HAVE_CTIME_R)
			lwsl_notice(" Peer Cert Valid from: %s",
						ctime_r(&ci.time, date));
#else
			lwsl_notice(" Peer Cert Valid from: %s",
						ctime(&ci.time));
#endif
		if (!lws_tls_peer_cert_info(wsi, LWS_TLS_CERT_INFO_VALIDITY_TO,
					    &ci, 0))
#if defined(LWS_HAVE_CTIME_R)
			lwsl_notice(" Peer Cert Valid to  : %s",
						ctime_r(&ci.time, date));
#else
			lwsl_notice(" Peer Cert Valid to  : %s",
						ctime(&ci.time));
#endif
		if (!lws_tls_peer_cert_info(wsi, LWS_TLS_CERT_INFO_USAGE,
					    &ci, 0))
			lwsl_notice(" Peer Cert usage bits: 0x%x\n", ci.usage);
#endif
		break;

	/* chunked content */
	case LWS_CALLBACK_RECEIVE_CLIENT_HTTP_READ:
		lwsl_notice("LWS_CALLBACK_RECEIVE_CLIENT_HTTP_READ: %ld\n",
			    (long)len);
		show_http_content(in, len);
		break;

	/* unchunked content */
	case LWS_CALLBACK_RECEIVE_CLIENT_HTTP:
		{
			char buffer[1024 + LWS_PRE];
			char *px = buffer + LWS_PRE;
			int lenx = sizeof(buffer) - LWS_PRE;

			/*
			 * Often you need to flow control this by something
			 * else being writable.  In that case call the api
			 * to get a callback when writable here, and do the
			 * pending client read in the writeable callback of
			 * the output.
			 *
			 * In the case of chunked content, this will call back
			 * LWS_CALLBACK_RECEIVE_CLIENT_HTTP_READ once per
			 * chunk or partial chunk in the buffer, and report
			 * zero length back here.
			 */
			if (lws_http_client_read(wsi, &px, &lenx) < 0)
				return -1;
		}
		break;

	case LWS_CALLBACK_CLIENT_WRITEABLE:
		lwsl_info("Client wsi %p writable\n", wsi);
		break;

	case LWS_CALLBACK_CLIENT_APPEND_HANDSHAKE_HEADER:
		if (test_post) {
			unsigned char **p = (unsigned char **)in, *end = (*p) + len;

			if (lws_add_http_header_by_token(wsi,
					WSI_TOKEN_HTTP_CONTENT_LENGTH,
					(unsigned char *)"29", 2, p, end))
				return -1;
			if (lws_add_http_header_by_token(wsi,
					WSI_TOKEN_HTTP_CONTENT_TYPE,
					(unsigned char *)"application/x-www-form-urlencoded",
					33, p, end))
				return -1;

			/* inform lws we have http body to send */
			lws_client_http_body_pending(wsi, 1);
			lws_callback_on_writable(wsi);
		}
		break;

	case LWS_CALLBACK_CLIENT_HTTP_WRITEABLE:
		strcpy(buf + LWS_PRE, "text=hello&send=Send+the+form");
		n = lws_write(wsi, (unsigned char *)&buf[LWS_PRE],
			      strlen(&buf[LWS_PRE]), LWS_WRITE_HTTP);
		if (n < 0)
			return -1;
		/* we only had one thing to send, so inform lws we are done
		 * if we had more to send, call lws_callback_on_writable(wsi);
		 * and just return 0 from callback.  On having sent the last
		 * part, call the below api instead.*/
		lws_client_http_body_pending(wsi, 0);
		break;

	case LWS_CALLBACK_COMPLETED_CLIENT_HTTP:
		wsi_dumb = NULL;
		force_exit = 1;
		break;

#if defined(LWS_WITH_TLS) && defined(LWS_HAVE_SSL_CTX_set1_param) && \
	!defined(LWS_WITH_MBEDTLS)
	case LWS_CALLBACK_OPENSSL_LOAD_EXTRA_CLIENT_VERIFY_CERTS:
		if (crl_path[0]) {
			/* Enable CRL checking of the server certificate */
			X509_STORE *store;
			X509_LOOKUP *lookup;
			int n;
			X509_VERIFY_PARAM *param = X509_VERIFY_PARAM_new();
			X509_VERIFY_PARAM_set_flags(param, X509_V_FLAG_CRL_CHECK);
			SSL_CTX_set1_param((SSL_CTX*)user, param);
			store = SSL_CTX_get_cert_store((SSL_CTX*)user);
			lookup = X509_STORE_add_lookup(store,
							X509_LOOKUP_file());
			n = X509_load_cert_crl_file(lookup, crl_path,
							X509_FILETYPE_PEM);
			X509_VERIFY_PARAM_free(param);
			if (n != 1) {
				char errbuf[256];
				const char *es;

				n = (int)ERR_get_error();
				es = ERR_error_string(
#if defined(LWS_WITH_BORINGSSL)
							 (uint32_t)
#else
							 (unsigned long)
#endif
							 n, errbuf);
				lwsl_err("EXTRA_CLIENT_VERIFY_CERTS: "
					 "SSL error: %s (%d)\n", es, n);
				return 1;
			}
		}
		break;
#endif

	default:
		break;
	}

	return 0;
}


/* lws-mirror_protocol */


static int
callback_lws_mirror(struct lws *wsi, enum lws_callback_reasons reason,
		    void *user, void *in, size_t len)
{
	unsigned char buf[LWS_PRE + block_size], *p;
	unsigned int rands[4];
	int l = 0;
	int n;

	switch (reason) {
	case LWS_CALLBACK_CLIENT_ESTABLISHED:

		lwsl_notice("mirror: LWS_CALLBACK_CLIENT_ESTABLISHED\n");

		if (flag_echo) {
			rxb = txb = 0;
			rx.cyc[0] = tx.cyc[0] = 0xabcde;
			rx.cyc[1] = tx.cyc[1] = 0x23456789;

			lws_callback_on_writable(wsi);

			break;
		}

		lws_get_random(lws_get_context(wsi), rands, sizeof(rands[0]));
		mirror_lifetime = (int)(16384 + (rands[0] & 65535));
		/* useful to test single connection stability */
		if (longlived)
			mirror_lifetime += 500000;

		lwsl_notice("opened mirror connection with "
			  "%d lifetime\n", mirror_lifetime);

		/*
		 * mirror_lifetime is decremented each send, when it reaches
		 * zero the connection is closed in the send callback.
		 * When the close callback comes, wsi_mirror is set to NULL
		 * so a new connection will be opened
		 *
		 * start the ball rolling,
		 * LWS_CALLBACK_CLIENT_WRITEABLE will come next service
		 */
		if (!flag_no_mirror_traffic)
			lws_callback_on_writable(wsi);
		break;

	case LWS_CALLBACK_CLIENT_CLOSED:
		lwsl_notice("mirror: LWS_CALLBACK_CLOSED mirror_lifetime=%d, "
			    "rxb %d, rx_count %d\n", mirror_lifetime, rxb,
			    rx_count);
		wsi_mirror = NULL;
		if (flag_echo || once)
			force_exit = 1;
		break;

	case LWS_CALLBACK_CLIENT_WRITEABLE:
		lwsl_user("LWS_CALLBACK_CLIENT_WRITEABLE\n");
		if (flag_no_mirror_traffic)
			return 0;

		if (flag_echo) {
			for (n = 0; n < (int)block_size; n++)
				buf[LWS_PRE + n] = lws_poly_rand(&tx);

			n = lws_write(wsi, &buf[LWS_PRE], block_size,
				      opts | LWS_WRITE_TEXT);
			if (n < 0) {
				lwsl_err("Error sending\n");
				return -1;
			}

			txb++;
			if (txb != count_blocks)
				lws_callback_on_writable(wsi);
			else {
				lwsl_notice("send completed: %d x %d\n",
					    count_blocks, block_size);
			}
			break;
		}

		for (n = 0; n < 1; n++) {
			lws_get_random(lws_get_context(wsi), rands,
				       sizeof(rands));
			l += sprintf((char *)&buf[LWS_PRE + l],
					"c #%06X %u %u %u;",
					rands[0] & 0xffffff,	/* colour */
					rands[1] & 511,		/* x */
					rands[2] & 255,		/* y */
					(rands[3] & 31) + 1);	/* radius */
		}

		n = (int)lws_write(wsi, &buf[LWS_PRE], (unsigned int)l,
			      opts | LWS_WRITE_TEXT);
		if (n < 0)
			return -1;
		if (n < l) {
			lwsl_err("Partial write LWS_CALLBACK_CLIENT_WRITEABLE\n");
			return -1;
		}
		if (!justmirror)
			mirror_lifetime--;
		if (!mirror_lifetime) {
			lwsl_notice("closing mirror session\n");
			return -1;
		}
		/* get notified as soon as we can write again */
		lws_callback_on_writable(wsi);

#if !defined(_WIN32) && !defined(WIN32)
		usleep(50);
#endif
		break;

	case LWS_CALLBACK_CLIENT_RECEIVE:
		if (flag_echo) {
			p = (unsigned char *)in;
			for (n = 0; n < (int)len; n++)
				if (*p++ != lws_poly_rand(&rx)) {
					lwsl_err("mismatch at rxb %d offset %d\n", (int)rxb + (n / block_size), n % block_size);
					errs++;
					force_exit = 1;
					return -1;
				}
			rx_count += (unsigned int)(unsigned long long)len;
			while (rx_count >= block_size) {
				rx_count -= block_size;
				rxb++;
			}
			if (rx_count == 0 && rxb == count_blocks) {
				lwsl_notice("Everything received: errs %d\n",
					    errs);
				force_exit = 1;
				return -1;
			}
		}
		break;
	default:
		break;
	}

	return 0;
}

static int
callback_test_raw_client(struct lws *wsi, enum lws_callback_reasons reason,
			 void *user, void *in, size_t len)
{
	switch (reason) {
	case LWS_CALLBACK_RAW_ADOPT:
		lwsl_notice("LWS_CALLBACK_RAW_ADOPT\n");
		break;

	case LWS_CALLBACK_RAW_RX:
		lwsl_notice("LWS_CALLBACK_RAW_RX %ld\n", (long)len);
		puts(in);
		break;

	case LWS_CALLBACK_RAW_CLOSE:
		lwsl_notice("LWS_CALLBACK_RAW_CLOSE\n");
		break;

	case LWS_CALLBACK_RAW_WRITEABLE:
		lwsl_notice("LWS_CALLBACK_RAW_WRITEABLE\n");
		break;

	default:
		break;
	}

	return 0;
}

/* list of supported protocols and callbacks */

static const struct lws_protocols protocols[] = {
	{
		"dumb-increment-protocol",
		callback_dumb_increment,
		0, 20, 0, NULL, 0
	},
	{
		"lws-mirror-protocol",
		callback_lws_mirror,
		0, 4096, 0, NULL, 0
	}, {
		"lws-test-raw-client",
		callback_test_raw_client,
		0, 128, 0, NULL, 0
	},
	LWS_PROTOCOL_LIST_TERM
};

#if defined(LWS_ROLE_WS) && !defined(LWS_WITHOUT_EXTENSIONS)
static const struct lws_extension exts[] = {
	{
		"permessage-deflate",
		lws_extension_callback_pm_deflate,
		"permessage-deflate; client_no_context_takeover"
	},
	{
		"deflate-frame",
		lws_extension_callback_pm_deflate,
		"deflate_frame"
	},
	{ NULL, NULL, NULL /* terminator */ }
};
#endif


void sighandler(int sig)
{
	force_exit = 1;
}

#if defined(LWS_HAS_GETOPT_LONG) || defined(WIN32)
static struct option options[] = {
	{ "help",	no_argument,		NULL, 'h' },
	{ "debug",      required_argument,      NULL, 'd' },
	{ "port",	required_argument,	NULL, 'p' },
	{ "ssl",	no_argument,		NULL, 's' },
	{ "strict-ssl",	no_argument,		NULL, 'S' },
	{ "version",	required_argument,	NULL, 'v' },
	{ "undeflated",	no_argument,		NULL, 'u' },
	{ "echo",	no_argument,		NULL, 'e' },
	{ "multi-test",	no_argument,		NULL, 'm' },
	{ "nomirror",	no_argument,		NULL, 'n' },
	{ "justmirror",	no_argument,		NULL, 'j' },
	{ "longlived",	no_argument,		NULL, 'l' },
	{ "post",	no_argument,		NULL, 'o' },
	{ "once",	no_argument,		NULL, 'O' },
	{ "ssl-cert",  required_argument,	NULL, 'C' },
	{ "ssl-key",  required_argument,	NULL, 'K' },
	{ "ssl-ca",  required_argument,		NULL, 'A' },
#if defined(LWS_WITH_TLS) && defined(LWS_HAVE_SSL_CTX_set1_param)
	{ "ssl-crl",  required_argument,		NULL, 'R' },
#endif
	{ NULL, 0, 0, 0 }
};
#endif

static int ratelimit_connects(unsigned int *last, unsigned int secs)
{
	struct timeval tv;

	gettimeofday(&tv, NULL);

	if ((unsigned long)tv.tv_sec - (unsigned long)(*last) < (unsigned long)secs)
		return 0;

	*last = (unsigned int)tv.tv_sec;

	return 1;
}

int main(int argc, char **argv)
{
	int n = 0, m, ret = 0, port = 7681, use_ssl = 0, ietf_version = -1;
	unsigned int rl_dumb = 0, rl_mirror = 0, do_ws = 1, do_multi = 0;
	struct lws_context_creation_info info;
	struct lws_client_connect_info i;
	struct lws_context *context;
	const char *prot, *p;
	char path[300];
	char cert_path[1024] = "";
	char key_path[1024] = "";
	char ca_path[1024] = "";
	unsigned long last = lws_now_secs();

	memset(&info, 0, sizeof info);

	lwsl_notice("libwebsockets test client - license MIT\n");
	lwsl_notice("(C) Copyright 2010-2018 Andy Green <andy@warmcat.com>\n");

	if (argc < 2)
		goto usage;

	while (n >= 0) {
#if defined(LWS_HAS_GETOPT_LONG) || defined(WIN32)
       n = getopt_long(argc, argv, "Sjnuv:hsp:d:lC:K:A:moeO", options, NULL);
#else
       n = getopt(argc, argv, "Sjnuv:hsp:d:lC:K:A:moeO");
#endif
		if (n < 0)
			continue;
		switch (n) {
		case 'd':
			lws_set_log_level(atoi(optarg), NULL);
			break;
		case 's': /* lax SSL, allow selfsigned, skip checking hostname */
			use_ssl = LCCSCF_USE_SSL |
				  LCCSCF_ALLOW_SELFSIGNED |
				  LCCSCF_SKIP_SERVER_CERT_HOSTNAME_CHECK;
			break;
		case 'S': /* Strict SSL, no selfsigned, check server hostname */
			use_ssl = LCCSCF_USE_SSL;
			break;
		case 'p':
			port = atoi(optarg);
			break;
		case 'e':
			flag_echo = 1;
			break;
		case 'j':
			justmirror = 1;
			break;
		case 'l':
			longlived = 1;
			break;
		case 'v':
			ietf_version = atoi(optarg);
			break;
		case 'u':
			deny_deflate = 1;
			break;
		case 'm':
			do_multi = 1;
			break;
		case 'o':
			test_post = 1;
			break;
		case 'O':
			once = 1;
			break;
		case 'n':
			flag_no_mirror_traffic = 1;
			lwsl_notice("Disabled sending mirror data (for pingpong testing)\n");
			break;
		case 'C':
			lws_strncpy(cert_path, optarg, sizeof(cert_path));
			break;
		case 'K':
			lws_strncpy(key_path, optarg, sizeof(key_path));
			break;
		case 'A':
			lws_strncpy(ca_path, optarg, sizeof(ca_path));
			break;

#if defined(LWS_WITH_TLS) && defined(LWS_HAVE_SSL_CTX_set1_param)
		case 'R':
			lws_strncpy(crl_path, optarg, sizeof(crl_path));
			break;
#endif
		case 'h':
			goto usage;
		}
	}

	if (optind >= argc)
		goto usage;

	signal(SIGINT, sighandler);

	memset(&i, 0, sizeof(i));

	i.port = port;
	if (lws_parse_uri(argv[optind], &prot, &i.address, &i.port, &p))
		goto usage;

	/* add back the leading / on path */
	if (p[0] != '/') {
		path[0] = '/';
		lws_strncpy(path + 1, p, sizeof(path) - 1);
		i.path = path;
	} else
		i.path = p;

	if (!strcmp(prot, "http") || !strcmp(prot, "ws"))
		use_ssl = 0;
	if (!strcmp(prot, "https") || !strcmp(prot, "wss"))
		if (!use_ssl)
			use_ssl = LCCSCF_USE_SSL;

	lwsl_debug("'%s' %p '%s' %p\n", i.address, i.address, i.path, i.path);

	/*
	 * create the websockets context.  This tracks open connections and
	 * knows how to route any traffic and which protocol version to use,
	 * and if each connection is client or server side.
	 *
	 * For this client-only demo, we tell it to not listen on any port.
	 */

	info.port = CONTEXT_PORT_NO_LISTEN;
	info.protocols = protocols;
	info.gid = (gid_t)-1;
	info.uid = (uid_t)-1;
#if defined(LWS_ROLE_WS) && !defined(LWS_WITHOUT_EXTENSIONS)
	info.extensions = exts;
#endif

	/*
	 * since we know this lws context is only ever going to be used with
	 * a few client wsis / fds / sockets at a time, let lws know it doesn't
	 * have to use the default allocations for fd tables up to ulimit -n.
	 * It will just allocate for 2 internal and 4 that we might use.
	 */
	info.fd_limit_per_thread = 2 + 4;

#if defined(LWS_WITH_TLS)
	info.options |= LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;
#endif

	info.options |= LWS_SERVER_OPTION_H2_JUST_FIX_WINDOW_UPDATE_OVERFLOW;
#if defined(LWS_WITH_TLS)
	if (use_ssl) {
		/*
		 * If the server wants us to present a valid SSL client certificate
		 * then we can set it up here.
		 */

		if (cert_path[0])
			info.client_ssl_cert_filepath = cert_path;
		if (key_path[0])
			info.client_ssl_private_key_filepath = key_path;

		/*
		 * A CA cert and CRL can be used to validate the cert send by the server
		 */
		if (ca_path[0])
			info.client_ssl_ca_filepath = ca_path;

#if defined(LWS_WITH_TLS) && defined(LWS_HAVE_SSL_CTX_set1_param)
		else if (crl_path[0])
			lwsl_notice("WARNING, providing a CRL requires a CA cert!\n");
#endif
	}

	if (use_ssl & LCCSCF_USE_SSL) {
		lwsl_notice(" Using SSL\n");
#if defined(LWS_WITH_MBEDTLS)
		lwsl_notice("   (NOTE: mbedtls needs to be given the remote\n");
		lwsl_notice("    CA cert to trust (with -A) to validate it)\n");
#endif
	}
	else
		lwsl_notice(" SSL disabled\n");
	if (use_ssl & LCCSCF_ALLOW_SELFSIGNED)
		lwsl_notice(" Selfsigned certs allowed\n");
	else
		lwsl_notice(" Cert must validate correctly (use -s to allow selfsigned)\n");
	if (use_ssl & LCCSCF_SKIP_SERVER_CERT_HOSTNAME_CHECK)
		lwsl_notice(" Skipping peer cert hostname check\n");
	else
		lwsl_notice(" Requiring peer cert hostname matches\n");
#endif
	context = lws_create_context(&info);
	if (context == NULL) {
		fprintf(stderr, "Creating libwebsocket context failed\n");
		return 1;
	}

	i.context = context;
	i.ssl_connection = use_ssl;
	i.host = i.address;
	i.origin = i.address;
	i.ietf_version_or_minus_one = ietf_version;

	if (!strcmp(prot, "http") || !strcmp(prot, "https")) {
		lwsl_notice("using %s mode (non-ws)\n", prot);
		if (test_post) {
			i.method = "POST";
			lwsl_notice("POST mode\n");
		}
		else
			i.method = "GET";
		do_ws = 0;
	} else
		if (!strcmp(prot, "raw")) {
			i.method = "RAW";
			i.protocol = "lws-test-raw-client";
			lwsl_notice("using RAW mode connection\n");
			do_ws = 0;
		} else
			lwsl_notice("using %s mode (ws)\n", prot);

	/*
	 * sit there servicing the websocket context to handle incoming
	 * packets, and drawing random circles on the mirror protocol websocket
	 *
	 * nothing happens until the client websocket connection is
	 * asynchronously established... calling lws_client_connect() only
	 * instantiates the connection logically, lws_service() progresses it
	 * asynchronously.
	 */

	m = 0;
	while (!force_exit) {

		if (do_multi) {
			for (n = 0; n < (int)LWS_ARRAY_SIZE(wsi_multi); n++) {
				if (!wsi_multi[n] && ratelimit_connects(&rl_multi[n], 2u)) {
					lwsl_notice("dumb %d: connecting\n", n);
					i.protocol = protocols[PROTOCOL_DUMB_INCREMENT].name;
					i.pwsi = &wsi_multi[n];
					lws_client_connect_via_info(&i);
				}
			}
		} else {

			if (do_ws) {
				if (!flag_echo && !justmirror && !wsi_dumb && ratelimit_connects(&rl_dumb, 2u)) {
					lwsl_notice("dumb: connecting\n");
					i.protocol = protocols[PROTOCOL_DUMB_INCREMENT].name;
					i.pwsi = &wsi_dumb;
					lws_client_connect_via_info(&i);
				}

				if (!wsi_mirror && ratelimit_connects(&rl_mirror, 2u)) {
					lwsl_notice("mirror: connecting\n");
					i.protocol = protocols[PROTOCOL_LWS_MIRROR].name;
					i.pwsi = &wsi_mirror;
					wsi_mirror = lws_client_connect_via_info(&i);
				}
			} else
				if (!wsi_dumb && ratelimit_connects(&rl_dumb, 2u)) {
					lwsl_notice("http: connecting\n");
					i.pwsi = &wsi_dumb;
					lws_client_connect_via_info(&i);
				}
		}

		lws_service(context, 500);

		if (do_multi) {
			m++;
			if (m == 10) {
				m = 0;
				lwsl_notice("doing lws_callback_on_writable_all_protocol\n");
				lws_callback_on_writable_all_protocol(context,
					   &protocols[PROTOCOL_DUMB_INCREMENT]);
			}
		}

		if (flag_echo && lws_now_secs() != last) {
			lwsl_notice("rxb %d, rx_count %d\n", rxb, rx_count);
			last = lws_now_secs();
		}
	}

	lwsl_err("Exiting\n");
	lws_context_destroy(context);

	return ret;

usage:
	fprintf(stderr, "Usage: libwebsockets-test-client "
				"<server address> [--port=<p>] "
				"[--ssl] [-k] [-v <ver>] "
				"[-d <log bitfield>] [-l]\n");
	return 1;
}
