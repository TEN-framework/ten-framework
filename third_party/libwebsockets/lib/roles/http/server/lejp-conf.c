/*
 * libwebsockets - small server side websockets and web server implementation
 *
 * Copyright (C) 2010 - 2019 Andy Green <andy@warmcat.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include "private-lib-core.h"

#ifndef _WIN32
/* this is needed for Travis CI */
#include <dirent.h>
#endif

#define ESC_INSTALL_DATADIR "_lws_ddir_"

static const char * const paths_global[] = {
	"global.uid",
	"global.gid",
	"global.username",
	"global.groupname",
	"global.count-threads",
	"global.init-ssl",
	"global.server-string",
	"global.plugin-dir",
	"global.ws-pingpong-secs", /* deprecated */
	"global.timeout-secs",
	"global.reject-service-keywords[].*",
	"global.reject-service-keywords[]",
	"global.default-alpn",
	"global.ip-limit-ah",
	"global.ip-limit-wsi",
	"global.rlimit-nofile",
};

enum lejp_global_paths {
	LEJPGP_UID,
	LEJPGP_GID,
	LEJPGP_USERNAME,
	LEJPGP_GROUPNAME,
	LEJPGP_COUNT_THREADS,
	LWJPGP_INIT_SSL,
	LEJPGP_SERVER_STRING,
	LEJPGP_PLUGIN_DIR,
	LWJPGP_PINGPONG_SECS,
	LWJPGP_TIMEOUT_SECS,
	LWJPGP_REJECT_SERVICE_KEYWORDS_NAME,
	LWJPGP_REJECT_SERVICE_KEYWORDS,
	LWJPGP_DEFAULT_ALPN,
	LWJPGP_IP_LIMIT_AH,
	LWJPGP_IP_LIMIT_WSI,
	LWJPGP_FD_LIMIT_PT,
};

static const char * const paths_vhosts[] = {
	"vhosts[]",
	"vhosts[].name",
	"vhosts[].port",
	"vhosts[].interface",
	"vhosts[].unix-socket",
	"vhosts[].unix-socket-perms",
	"vhosts[].sts",
	"vhosts[].host-ssl-key",
	"vhosts[].host-ssl-cert",
	"vhosts[].host-ssl-ca",
	"vhosts[].access-log",
	"vhosts[].mounts[].mountpoint",
	"vhosts[].mounts[].origin",
	"vhosts[].mounts[].protocol",
	"vhosts[].mounts[].default",
	"vhosts[].mounts[].auth-mask",
	"vhosts[].mounts[].cgi-timeout",
	"vhosts[].mounts[].cgi-env[].*",
	"vhosts[].mounts[].cgi-env[]",
	"vhosts[].mounts[].cache-max-age",
	"vhosts[].mounts[].cache-reuse",
	"vhosts[].mounts[].cache-revalidate",
	"vhosts[].mounts[].basic-auth",
	"vhosts[].mounts[].cache-intermediaries",
	"vhosts[].mounts[].extra-mimetypes.*",
	"vhosts[].mounts[].extra-mimetypes",
	"vhosts[].mounts[].interpret.*",
	"vhosts[].mounts[].interpret",
	"vhosts[].mounts[]",
	"vhosts[].ws-protocols[].*.*",
	"vhosts[].ws-protocols[].*",
	"vhosts[].ws-protocols[]",
	"vhosts[].keepalive_timeout",
	"vhosts[].enable-client-ssl",
	"vhosts[].ciphers",
	"vhosts[].ecdh-curve",
	"vhosts[].noipv6",
	"vhosts[].ipv6only",
	"vhosts[].ssl-option-set",
	"vhosts[].ssl-option-clear",
	"vhosts[].mounts[].pmo[].*",
	"vhosts[].mounts[].pmo[]",
	"vhosts[].headers[].*",
	"vhosts[].headers[]",
	"vhosts[].client-ssl-key",
	"vhosts[].client-ssl-cert",
	"vhosts[].client-ssl-ca",
	"vhosts[].client-ssl-ciphers",
	"vhosts[].onlyraw",
	"vhosts[].client-cert-required",
	"vhosts[].ignore-missing-cert",
	"vhosts[].error-document-404",
	"vhosts[].alpn",
	"vhosts[].fo-listen-queue",
	"vhosts[].ssl-client-option-set",
	"vhosts[].ssl-client-option-clear",
	"vhosts[].tls13-ciphers",
	"vhosts[].client-tls13-ciphers",
	"vhosts[].strict-host-check",

	"vhosts[].listen-accept-role",
	"vhosts[].listen-accept-protocol",
	"vhosts[].apply-listen-accept", /* deprecates "onlyraw" */
	"vhosts[].fallback-listen-accept",
	"vhosts[].allow-non-tls",
	"vhosts[].redirect-http",
	"vhosts[].allow-http-on-https",

	"vhosts[].disable-no-protocol-ws-upgrades",
	"vhosts[].h2-half-closed-long-poll",
};

enum lejp_vhost_paths {
	LEJPVP,
	LEJPVP_NAME,
	LEJPVP_PORT,
	LEJPVP_INTERFACE,
	LEJPVP_UNIXSKT,
	LEJPVP_UNIXSKT_PERMS,
	LEJPVP_STS,
	LEJPVP_HOST_SSL_KEY,
	LEJPVP_HOST_SSL_CERT,
	LEJPVP_HOST_SSL_CA,
	LEJPVP_ACCESS_LOG,
	LEJPVP_MOUNTPOINT,
	LEJPVP_ORIGIN,
	LEJPVP_MOUNT_PROTOCOL,
	LEJPVP_DEFAULT,
	LEJPVP_DEFAULT_AUTH_MASK,
	LEJPVP_CGI_TIMEOUT,
	LEJPVP_CGI_ENV,
	LEJPVP_CGI_ENV_base,
	LEJPVP_MOUNT_CACHE_MAX_AGE,
	LEJPVP_MOUNT_CACHE_REUSE,
	LEJPVP_MOUNT_CACHE_REVALIDATE,
	LEJPVP_MOUNT_BASIC_AUTH,
	LEJPVP_MOUNT_CACHE_INTERMEDIARIES,
	LEJPVP_MOUNT_EXTRA_MIMETYPES,
	LEJPVP_MOUNT_EXTRA_MIMETYPES_base,
	LEJPVP_MOUNT_INTERPRET,
	LEJPVP_MOUNT_INTERPRET_base,

	LEJPVP_MOUNTS,

	LEJPVP_PROTOCOL_NAME_OPT,
	LEJPVP_PROTOCOL_NAME,
	LEJPVP_PROTOCOL,
	LEJPVP_KEEPALIVE_TIMEOUT,
	LEJPVP_ENABLE_CLIENT_SSL,
	LEJPVP_CIPHERS,
	LEJPVP_ECDH_CURVE,
	LEJPVP_NOIPV6,
	LEJPVP_IPV6ONLY,
	LEJPVP_SSL_OPTION_SET,
	LEJPVP_SSL_OPTION_CLEAR,
	LEJPVP_PMO,
	LEJPVP_PM_baseO,
	LEJPVP_HEADERS_NAME,
	LEJPVP_HEADERS,
	LEJPVP_CLIENT_SSL_KEY,
	LEJPVP_CLIENT_SSL_CERT,
	LEJPVP_CLIENT_SSL_CA,
	LEJPVP_CLIENT_CIPHERS,
	LEJPVP_FLAG_ONLYRAW,
	LEJPVP_FLAG_CLIENT_CERT_REQUIRED,
	LEJPVP_IGNORE_MISSING_CERT,
	LEJPVP_ERROR_DOCUMENT_404,
	LEJPVP_ALPN,
	LWJPVP_FO_LISTEN_QUEUE,
	LEJPVP_SSL_CLIENT_OPTION_SET,
	LEJPVP_SSL_CLIENT_OPTION_CLEAR,
	LEJPVP_TLS13_CIPHERS,
	LEJPVP_CLIENT_TLS13_CIPHERS,
	LEJPVP_FLAG_STRICT_HOST_CHECK,

	LEJPVP_LISTEN_ACCEPT_ROLE,
	LEJPVP_LISTEN_ACCEPT_PROTOCOL,
	LEJPVP_FLAG_APPLY_LISTEN_ACCEPT,
	LEJPVP_FLAG_FALLBACK_LISTEN_ACCEPT,
	LEJPVP_FLAG_ALLOW_NON_TLS,
	LEJPVP_FLAG_REDIRECT_HTTP,
	LEJPVP_FLAG_ALLOW_HTTP_ON_HTTPS,

	LEJPVP_FLAG_DISABLE_NO_PROTOCOL_WS_UPGRADES,
	LEJPVP_FLAG_H2_HALF_CLOSED_LONG_POLL,
};

#define MAX_PLUGIN_DIRS 10

struct jpargs {
	struct lws_context_creation_info *info;
	struct lws_context *context;
	const struct lws_protocols *protocols;
	const struct lws_protocols **pprotocols;
	const struct lws_extension *extensions;
	char *p, *end, valid;
	struct lws_http_mount *head, *last;

	struct lws_protocol_vhost_options *pvo;
	struct lws_protocol_vhost_options *pvo_em;
	struct lws_protocol_vhost_options *pvo_int;
	struct lws_http_mount m;
	const char **plugin_dirs;
	int count_plugin_dirs;

	unsigned int reject_ws_with_no_protocol:1;
	unsigned int enable_client_ssl:1;
	unsigned int fresh_mount:1;
	unsigned int any_vhosts:1;
	unsigned int chunk:1;
};

static void *
lwsws_align(struct jpargs *a)
{
	if ((lws_intptr_t)(a->p) & 15)
		a->p += 16 - ((lws_intptr_t)(a->p) & 15);

	a->chunk = 0;

	return a->p;
}

static int
arg_to_bool(const char *s)
{
	static const char * const on[] = { "on", "yes", "true" };
	int n = atoi(s);

	if (n)
		return 1;

	for (n = 0; n < (int)LWS_ARRAY_SIZE(on); n++)
		if (!strcasecmp(s, on[n]))
			return 1;

	return 0;
}

static void
set_reset_flag(uint64_t *p, const char *state, uint64_t flag)
{
	if (arg_to_bool(state))
		*p |= flag;
	else
		*p &= ~(flag);
}

static signed char
lejp_globals_cb(struct lejp_ctx *ctx, char reason)
{
	struct jpargs *a = (struct jpargs *)ctx->user;
	struct lws_protocol_vhost_options *rej;
	int n;

	/* we only match on the prepared path strings */
	if (!(reason & LEJP_FLAG_CB_IS_VALUE) || !ctx->path_match)
		return 0;

	/* this catches, eg, vhosts[].headers[].xxx */
	if (reason == LEJPCB_VAL_STR_END &&
	    ctx->path_match == LWJPGP_REJECT_SERVICE_KEYWORDS_NAME + 1) {
		rej = lwsws_align(a);
		a->p += sizeof(*rej);

		n = lejp_get_wildcard(ctx, 0, a->p, lws_ptr_diff(a->end, a->p));
		rej->next = a->info->reject_service_keywords;
		a->info->reject_service_keywords = rej;
		rej->name = a->p;
		 lwsl_notice("  adding rej %s=%s\n", a->p, ctx->buf);
		a->p += n - 1;
		*(a->p++) = '\0';
		rej->value = a->p;
		rej->options = NULL;
		goto dostring;
	}

	switch (ctx->path_match - 1) {
	case LEJPGP_UID:
		a->info->uid = (unsigned int)atoi(ctx->buf);
		return 0;
	case LEJPGP_GID:
		a->info->gid = (unsigned int)atoi(ctx->buf);
		return 0;
	case LEJPGP_USERNAME:
		a->info->username = a->p;
		break;
	case LEJPGP_GROUPNAME:
		a->info->groupname = a->p;
		break;
	case LEJPGP_COUNT_THREADS:
		a->info->count_threads = (unsigned int)atoi(ctx->buf);
		return 0;
	case LWJPGP_INIT_SSL:
		if (arg_to_bool(ctx->buf))
			a->info->options |= LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;
		return 0;
	case LEJPGP_SERVER_STRING:
#if defined(LWS_WITH_SERVER)
		a->info->server_string = a->p;
#endif
		break;
	case LEJPGP_PLUGIN_DIR:
		if (a->count_plugin_dirs == MAX_PLUGIN_DIRS - 1) {
			lwsl_err("Too many plugin dirs\n");
			return -1;
		}
		a->plugin_dirs[a->count_plugin_dirs++] = a->p;
		break;

	case LWJPGP_PINGPONG_SECS: /* deprecated */
		return 0;

	case LWJPGP_TIMEOUT_SECS:
		a->info->timeout_secs = (unsigned int)atoi(ctx->buf);
		return 0;

#if defined(LWS_WITH_TLS)
	case LWJPGP_DEFAULT_ALPN:
		a->info->alpn = a->p;
		break;
#endif

#if defined(LWS_WITH_PEER_LIMITS)
	case LWJPGP_IP_LIMIT_AH:
		a->info->ip_limit_ah = (uint16_t)atoi(ctx->buf);
		return 0;

	case LWJPGP_IP_LIMIT_WSI:
		a->info->ip_limit_wsi = (uint16_t)atoi(ctx->buf);
		return 0;
#endif

	case LWJPGP_FD_LIMIT_PT:
		a->info->rlimit_nofile = atoi(ctx->buf);
		return 0;

	default:
		return 0;
	}

dostring:
	a->p += lws_snprintf(a->p, lws_ptr_diff_size_t(a->end, a->p), "%s", ctx->buf);
	*(a->p)++ = '\0';

	return 0;
}

static signed char
lejp_vhosts_cb(struct lejp_ctx *ctx, char reason)
{
	struct jpargs *a = (struct jpargs *)ctx->user;
	struct lws_protocol_vhost_options *pvo, *mp_cgienv, *headers;
	struct lws_http_mount *m;
	char *p, *p1;
	int n;

#if 0
	lwsl_notice(" %d: %s (%d)\n", reason, ctx->path, ctx->path_match);
	for (n = 0; n < ctx->wildcount; n++)
		lwsl_notice("    %d\n", ctx->wild[n]);
#endif

	if (reason == LEJPCB_OBJECT_START && ctx->path_match == LEJPVP + 1) {
		uint32_t i[4];
#if defined(LWS_WITH_SERVER)
		const char *ss;
#endif

		/* set the defaults for this vhost */
		a->reject_ws_with_no_protocol = 0;
		a->valid = 1;
		a->head = NULL;
		a->last = NULL;

		i[0] = a->info->count_threads;
		i[1] = a->info->options & (
			LWS_SERVER_OPTION_SKIP_SERVER_CANONICAL_NAME |
			LWS_SERVER_OPTION_LIBUV |
			LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT |
			LWS_SERVER_OPTION_EXPLICIT_VHOSTS |
			LWS_SERVER_OPTION_UV_NO_SIGSEGV_SIGFPE_SPIN |
			LWS_SERVER_OPTION_LIBEVENT |
			LWS_SERVER_OPTION_LIBEV
				);
#if defined(LWS_WITH_SERVER)
		ss = a->info->server_string;
#endif
		i[3] = a->info->timeout_secs;

		memset(a->info, 0, sizeof(*a->info));

		a->info->count_threads = i[0];
		a->info->options = i[1];
#if defined(LWS_WITH_SERVER)
		a->info->server_string = ss;
#endif
		a->info->timeout_secs = i[3];

		a->info->protocols = a->protocols;
		a->info->pprotocols = a->pprotocols;
#if defined(LWS_ROLE_WS)
		a->info->extensions = a->extensions;
#endif
#if defined(LWS_WITH_TLS)
#if defined(LWS_WITH_CLIENT)
		a->info->client_ssl_cipher_list = "ECDHE-ECDSA-AES256-GCM-SHA384:"
			"ECDHE-RSA-AES256-GCM-SHA384:"
			"DHE-RSA-AES256-GCM-SHA384:"
			"ECDHE-RSA-AES256-SHA384:"
			"HIGH:!aNULL:!eNULL:!EXPORT:"
			"!DES:!MD5:!PSK:!RC4:!HMAC_SHA1:"
			"!SHA1:!DHE-RSA-AES128-GCM-SHA256:"
			"!DHE-RSA-AES128-SHA256:"
			"!AES128-GCM-SHA256:"
			"!AES128-SHA256:"
			"!DHE-RSA-AES256-SHA256:"
			"!AES256-GCM-SHA384:"
			"!AES256-SHA256";
#endif
#if defined(LWS_WITH_SERVER)
		a->info->ssl_cipher_list = "ECDHE-ECDSA-AES256-GCM-SHA384:"
				       "ECDHE-RSA-AES256-GCM-SHA384:"
				       "DHE-RSA-AES256-GCM-SHA384:"
				       "ECDHE-RSA-AES256-SHA384:"
				       "HIGH:!aNULL:!eNULL:!EXPORT:"
				       "!DES:!MD5:!PSK:!RC4:!HMAC_SHA1:"
				       "!SHA1:!DHE-RSA-AES128-GCM-SHA256:"
				       "!DHE-RSA-AES128-SHA256:"
				       "!AES128-GCM-SHA256:"
				       "!AES128-SHA256:"
				       "!DHE-RSA-AES256-SHA256:"
				       "!AES256-GCM-SHA384:"
				       "!AES256-SHA256";
#endif
#endif
		a->info->keepalive_timeout = 5;
	}

	if (reason == LEJPCB_OBJECT_START &&
	    ctx->path_match == LEJPVP_MOUNTS + 1) {
		a->fresh_mount = 1;
		memset(&a->m, 0, sizeof(a->m));
	}

	/* this catches, eg, vhosts[].ws-protocols[].xxx-protocol */
	if (reason == LEJPCB_OBJECT_START &&
	    ctx->path_match == LEJPVP_PROTOCOL_NAME + 1) {
		a->pvo = lwsws_align(a);
		a->p += sizeof(*a->pvo);

		n = lejp_get_wildcard(ctx, 0, a->p, lws_ptr_diff(a->end, a->p));
		/* ie, enable this protocol, no options yet */
		a->pvo->next = a->info->pvo;
		a->info->pvo = a->pvo;
		a->pvo->name = a->p;
		lwsl_info("  adding protocol %s\n", a->p);
		a->p += n;
		a->pvo->value = a->p;
		a->pvo->options = NULL;
		goto dostring;
	}

	/* this catches, eg, vhosts[].headers[].xxx */
	if ((reason == LEJPCB_VAL_STR_END || reason == LEJPCB_VAL_STR_CHUNK) &&
	    ctx->path_match == LEJPVP_HEADERS_NAME + 1) {

		if (!a->chunk) {
			headers = lwsws_align(a);
			a->p += sizeof(*headers);

			n = lejp_get_wildcard(ctx, 0, a->p,
					lws_ptr_diff(a->end, a->p));
			/* ie, add this header */
			headers->next = a->info->headers;
			a->info->headers = headers;
			headers->name = a->p;

			lwsl_notice("  adding header %s=%s\n", a->p, ctx->buf);
			a->p += n - 1;
			*(a->p++) = ':';
			if (a->p < a->end)
				*(a->p++) = '\0';
			else
				*(a->p - 1) = '\0';
			headers->value = a->p;
			headers->options = NULL;
		}
		a->chunk = reason == LEJPCB_VAL_STR_CHUNK;
		goto dostring;
	}

	if (reason == LEJPCB_OBJECT_END &&
	    (ctx->path_match == LEJPVP + 1 || !ctx->path[0]) &&
	    a->valid) {

		struct lws_vhost *vhost;

		//lwsl_notice("%s\n", ctx->path);
		if (!a->info->port &&
		    !(a->info->options & LWS_SERVER_OPTION_UNIX_SOCK)) {
			lwsl_err("Port required (eg, 443)\n");
			return 1;
		}
		a->valid = 0;
		a->info->mounts = a->head;

		vhost = lws_create_vhost(a->context, a->info);
		if (!vhost) {
			lwsl_err("Failed to create vhost %s\n",
				 a->info->vhost_name);
			return 1;
		}
		a->any_vhosts = 1;

		if (a->reject_ws_with_no_protocol) {
			a->reject_ws_with_no_protocol = 0;

			vhost->default_protocol_index = 255;
		}

#if defined(LWS_WITH_TLS) && defined(LWS_WITH_CLIENT)
		if (a->enable_client_ssl) {
			const char *cert_filepath =
					a->info->client_ssl_cert_filepath;
			const char *private_key_filepath =
				       a->info->client_ssl_private_key_filepath;
			const char *ca_filepath =
					a->info->client_ssl_ca_filepath;
			const char *cipher_list =
					a->info->client_ssl_cipher_list;

			memset(a->info, 0, sizeof(*a->info));
			a->info->client_ssl_cert_filepath = cert_filepath;
			a->info->client_ssl_private_key_filepath =
							private_key_filepath;
			a->info->client_ssl_ca_filepath = ca_filepath;
			a->info->client_ssl_cipher_list = cipher_list;
			a->info->options = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;
			lws_init_vhost_client_ssl(a->info, vhost);
		}
#endif

		return 0;
	}

	if (reason == LEJPCB_OBJECT_END &&
	    ctx->path_match == LEJPVP_MOUNTS + 1) {
		static const char * const mount_protocols[] = {
			"http://",
			"https://",
			"file://",
			"cgi://",
			">http://",
			">https://",
			"callback://",
			"gzip://",
		};

		if (!a->fresh_mount)
			return 0;

		if (!a->m.mountpoint || !a->m.origin) {
			lwsl_err("mountpoint and origin required\n");
			return 1;
		}
		lwsl_debug("adding mount %s\n", a->m.mountpoint);
		m = lwsws_align(a);
		memcpy(m, &a->m, sizeof(*m));
		if (a->last)
			a->last->mount_next = m;

		for (n = 0; n < (int)LWS_ARRAY_SIZE(mount_protocols); n++)
			if (!strncmp(a->m.origin, mount_protocols[n],
			     strlen(mount_protocols[n]))) {
				lwsl_info("----%s\n", a->m.origin);
				m->origin_protocol = (uint8_t)(unsigned int)n;
				m->origin = a->m.origin +
					    strlen(mount_protocols[n]);
				break;
			}

		if (n == (int)LWS_ARRAY_SIZE(mount_protocols)) {
			lwsl_err("unsupported protocol:// %s\n", a->m.origin);
			return 1;
		}

		a->p += sizeof(*m);
		if (!a->head)
			a->head = m;

		a->last = m;
		a->fresh_mount = 0;
	}

	/* we only match on the prepared path strings */
	if (!(reason & LEJP_FLAG_CB_IS_VALUE) || !ctx->path_match)
		return 0;

	switch (ctx->path_match - 1) {
	case LEJPVP_NAME:
		a->info->vhost_name = a->p;
		break;
	case LEJPVP_PORT:
		a->info->port = atoi(ctx->buf);
		return 0;
	case LEJPVP_INTERFACE:
		a->info->iface = a->p;
		break;
	case LEJPVP_UNIXSKT:
		if (arg_to_bool(ctx->buf))
			a->info->options |= (uint64_t)LWS_SERVER_OPTION_UNIX_SOCK;
		else
			a->info->options &= (uint64_t)~(LWS_SERVER_OPTION_UNIX_SOCK);
		return 0;
	case LEJPVP_UNIXSKT_PERMS:
		a->info->unix_socket_perms = a->p;
		break;
	case LEJPVP_STS:
		if (arg_to_bool(ctx->buf))
			a->info->options |= (uint64_t)LWS_SERVER_OPTION_STS;
		else
			a->info->options &= (uint64_t)~(LWS_SERVER_OPTION_STS);
		return 0;
#if defined(LWS_WITH_TLS)
	case LEJPVP_HOST_SSL_KEY:
		a->info->ssl_private_key_filepath = a->p;
		break;
	case LEJPVP_HOST_SSL_CERT:
		a->info->ssl_cert_filepath = a->p;
		break;
	case LEJPVP_HOST_SSL_CA:
		a->info->ssl_ca_filepath = a->p;
		break;
#endif
	case LEJPVP_ACCESS_LOG:
		a->info->log_filepath = a->p;
		break;
	case LEJPVP_MOUNTPOINT:
		a->m.mountpoint = a->p;
		a->m.mountpoint_len = (unsigned char)strlen(ctx->buf);
		break;
	case LEJPVP_ORIGIN:
		if (!strncmp(ctx->buf, "callback://", 11))
			a->m.protocol = a->p + 11;

		if (!a->m.origin)
			a->m.origin = a->p;
		break;
	case LEJPVP_DEFAULT:
		a->m.def = a->p;
		break;
	case LEJPVP_DEFAULT_AUTH_MASK:
		a->m.auth_mask = (unsigned int)atoi(ctx->buf);
		return 0;
	case LEJPVP_MOUNT_CACHE_MAX_AGE:
		a->m.cache_max_age = atoi(ctx->buf);
		return 0;
	case LEJPVP_MOUNT_CACHE_REUSE:
		a->m.cache_reusable = !!arg_to_bool(ctx->buf);
		return 0;
	case LEJPVP_MOUNT_CACHE_REVALIDATE:
		a->m.cache_revalidate = !!arg_to_bool(ctx->buf);
		return 0;
	case LEJPVP_MOUNT_CACHE_INTERMEDIARIES:
		a->m.cache_intermediaries = !!arg_to_bool(ctx->buf);;
		return 0;
	case LEJPVP_MOUNT_BASIC_AUTH:
#if defined(LWS_WITH_HTTP_BASIC_AUTH)
		a->m.basic_auth_login_file = a->p;
#endif
		break;
	case LEJPVP_CGI_TIMEOUT:
		a->m.cgi_timeout = atoi(ctx->buf);
		return 0;
	case LWJPVP_FO_LISTEN_QUEUE:
		a->info->fo_listen_queue = atoi(ctx->buf);
		return 0;
	case LEJPVP_KEEPALIVE_TIMEOUT:
		a->info->keepalive_timeout = atoi(ctx->buf);
		return 0;
#if defined(LWS_WITH_TLS)
#if defined(LWS_WITH_CLIENT)
	case LEJPVP_CLIENT_CIPHERS:
		a->info->client_ssl_cipher_list = a->p;
		break;
	case LEJPVP_CLIENT_TLS13_CIPHERS:
		a->info->client_tls_1_3_plus_cipher_list = a->p;
		break;
#endif

	case LEJPVP_CIPHERS:
		a->info->ssl_cipher_list = a->p;
		break;
	case LEJPVP_TLS13_CIPHERS:
		a->info->tls1_3_plus_cipher_list = a->p;
		break;
	case LEJPVP_ECDH_CURVE:
		a->info->ecdh_curve = a->p;
		break;
#endif
	case LEJPVP_PMO:
	case LEJPVP_CGI_ENV:
		mp_cgienv = lwsws_align(a);
		a->p += sizeof(*a->m.cgienv);

		mp_cgienv->next = a->m.cgienv;
		a->m.cgienv = mp_cgienv;

		n = lejp_get_wildcard(ctx, 0, a->p, lws_ptr_diff(a->end, a->p));
		mp_cgienv->name = a->p;
		a->p += n;
		mp_cgienv->value = a->p;
		mp_cgienv->options = NULL;
		//lwsl_notice("    adding pmo / cgi-env '%s' = '%s'\n",
		//		mp_cgienv->name, mp_cgienv->value);
		goto dostring;

	case LEJPVP_PROTOCOL_NAME_OPT:
		/* this catches, eg,
		 * vhosts[].ws-protocols[].xxx-protocol.yyy-option
		 * ie, these are options attached to a protocol with { }
		 */
		pvo = lwsws_align(a);
		a->p += sizeof(*a->pvo);

		n = lejp_get_wildcard(ctx, 1, a->p, lws_ptr_diff(a->end, a->p));
		/* ie, enable this protocol, no options yet */
		pvo->next = a->pvo->options;
		a->pvo->options = pvo;
		pvo->name = a->p;
		a->p += n;
		pvo->value = a->p;
		pvo->options = NULL;
		break;

	case LEJPVP_MOUNT_EXTRA_MIMETYPES:
		a->pvo_em = lwsws_align(a);
		a->p += sizeof(*a->pvo_em);

		n = lejp_get_wildcard(ctx, 0, a->p, lws_ptr_diff(a->end, a->p));
		/* ie, enable this protocol, no options yet */
		a->pvo_em->next = a->m.extra_mimetypes;
		a->m.extra_mimetypes = a->pvo_em;
		a->pvo_em->name = a->p;
		lwsl_notice("  + extra-mimetypes %s -> %s\n", a->p, ctx->buf);
		a->p += n;
		a->pvo_em->value = a->p;
		a->pvo_em->options = NULL;
		break;

	case LEJPVP_MOUNT_INTERPRET:
		a->pvo_int = lwsws_align(a);
		a->p += sizeof(*a->pvo_int);

		n = lejp_get_wildcard(ctx, 0, a->p, lws_ptr_diff(a->end, a->p));
		/* ie, enable this protocol, no options yet */
		a->pvo_int->next = a->m.interpret;
		a->m.interpret = a->pvo_int;
		a->pvo_int->name = a->p;
		lwsl_notice("  adding interpret %s -> %s\n", a->p,
			    ctx->buf);
		a->p += n;
		a->pvo_int->value = a->p;
		a->pvo_int->options = NULL;
		break;

	case LEJPVP_ENABLE_CLIENT_SSL:
		a->enable_client_ssl = !!arg_to_bool(ctx->buf);
		return 0;
#if defined(LWS_WITH_TLS) && defined(LWS_WITH_CLIENT)
	case LEJPVP_CLIENT_SSL_KEY:
		a->info->client_ssl_private_key_filepath = a->p;
		break;
	case LEJPVP_CLIENT_SSL_CERT:
		a->info->client_ssl_cert_filepath = a->p;
		break;
	case LEJPVP_CLIENT_SSL_CA:
		a->info->client_ssl_ca_filepath = a->p;
		break;
#endif

	case LEJPVP_NOIPV6:
		set_reset_flag(&a->info->options, ctx->buf,
			       LWS_SERVER_OPTION_DISABLE_IPV6);
		return 0;

	case LEJPVP_FLAG_ONLYRAW:
		set_reset_flag(&a->info->options, ctx->buf,
			    LWS_SERVER_OPTION_ADOPT_APPLY_LISTEN_ACCEPT_CONFIG);
		return 0;

	case LEJPVP_IPV6ONLY:
		a->info->options |= LWS_SERVER_OPTION_IPV6_V6ONLY_MODIFY;
		set_reset_flag(&a->info->options, ctx->buf,
			       LWS_SERVER_OPTION_IPV6_V6ONLY_VALUE);
		return 0;

	case LEJPVP_FLAG_CLIENT_CERT_REQUIRED:
		if (arg_to_bool(ctx->buf))
			a->info->options |=
			    LWS_SERVER_OPTION_REQUIRE_VALID_OPENSSL_CLIENT_CERT;
		return 0;

	case LEJPVP_IGNORE_MISSING_CERT:
		set_reset_flag(&a->info->options, ctx->buf,
				LWS_SERVER_OPTION_IGNORE_MISSING_CERT);
		return 0;

	case LEJPVP_FLAG_STRICT_HOST_CHECK:
		set_reset_flag(&a->info->options, ctx->buf,
			LWS_SERVER_OPTION_VHOST_UPG_STRICT_HOST_CHECK);
		return 0;

	case LEJPVP_ERROR_DOCUMENT_404:
		a->info->error_document_404 = a->p;
		break;

#if defined(LWS_WITH_TLS)
	case LEJPVP_SSL_OPTION_SET:
		a->info->ssl_options_set |= atol(ctx->buf);
		return 0;
	case LEJPVP_SSL_OPTION_CLEAR:
		a->info->ssl_options_clear |= atol(ctx->buf);
		return 0;

#if defined(LWS_WITH_CLIENT)
	case LEJPVP_SSL_CLIENT_OPTION_SET:
		a->info->ssl_client_options_set |= atol(ctx->buf);
		return 0;
	case LEJPVP_SSL_CLIENT_OPTION_CLEAR:
		a->info->ssl_client_options_clear |= atol(ctx->buf);
		return 0;
#endif

	case LEJPVP_ALPN:
		a->info->alpn = a->p;
		break;
#endif

	case LEJPVP_LISTEN_ACCEPT_ROLE:
		a->info->listen_accept_role = a->p;
		break;
	case LEJPVP_LISTEN_ACCEPT_PROTOCOL:
		a->info->listen_accept_protocol = a->p;
		break;

	case LEJPVP_FLAG_APPLY_LISTEN_ACCEPT:
		set_reset_flag(&a->info->options, ctx->buf,
			LWS_SERVER_OPTION_ADOPT_APPLY_LISTEN_ACCEPT_CONFIG);
		return 0;
	case LEJPVP_FLAG_FALLBACK_LISTEN_ACCEPT:
		lwsl_notice("vh %s: LEJPVP_FLAG_FALLBACK_LISTEN_ACCEPT: %s\n",
			    a->info->vhost_name, ctx->buf);
		set_reset_flag(&a->info->options, ctx->buf,
		      LWS_SERVER_OPTION_FALLBACK_TO_APPLY_LISTEN_ACCEPT_CONFIG);
		return 0;
	case LEJPVP_FLAG_ALLOW_NON_TLS:
		set_reset_flag(&a->info->options, ctx->buf,
			       LWS_SERVER_OPTION_ALLOW_NON_SSL_ON_SSL_PORT);
		return 0;
	case LEJPVP_FLAG_REDIRECT_HTTP:
		set_reset_flag(&a->info->options, ctx->buf,
			       LWS_SERVER_OPTION_REDIRECT_HTTP_TO_HTTPS);
		return 0;
	case LEJPVP_FLAG_ALLOW_HTTP_ON_HTTPS:
		set_reset_flag(&a->info->options, ctx->buf,
			       LWS_SERVER_OPTION_ALLOW_HTTP_ON_HTTPS_LISTENER);
		return 0;

	case LEJPVP_FLAG_DISABLE_NO_PROTOCOL_WS_UPGRADES:
		a->reject_ws_with_no_protocol = 1;
		return 0;

	case LEJPVP_FLAG_H2_HALF_CLOSED_LONG_POLL:
		set_reset_flag(&a->info->options, ctx->buf,
				LWS_SERVER_OPTION_VH_H2_HALF_CLOSED_LONG_POLL);
		return 0;

	default:
		return 0;
	}

dostring:
	p = ctx->buf;
	p[LEJP_STRING_CHUNK] = '\0';
	p1 = strstr(p, ESC_INSTALL_DATADIR);
	if (p1) {
		n = lws_ptr_diff(p1, p);
		if (n > a->end - a->p)
			n = lws_ptr_diff(a->end, a->p);
		lws_strncpy(a->p, p, (unsigned int)n + 1u);
		a->p += n;
		a->p += lws_snprintf(a->p, lws_ptr_diff_size_t(a->end, a->p), "%s",
				     LWS_INSTALL_DATADIR);
		p += n + (int)strlen(ESC_INSTALL_DATADIR);
	}

	a->p += lws_snprintf(a->p, lws_ptr_diff_size_t(a->end, a->p), "%s", p);
	if (reason == LEJPCB_VAL_STR_END)
		*(a->p)++ = '\0';

	return 0;
}

/*
 * returns 0 = OK, 1 = can't open, 2 = parsing error
 */

static int
lwsws_get_config(void *user, const char *f, const char * const *paths,
		 int count_paths, lejp_callback cb)
{
	unsigned char buf[128];
	struct lejp_ctx ctx;
	int n, m = 0, fd;

	memset(&ctx, 0, sizeof(ctx));

	fd = lws_open(f, O_RDONLY);
	if (fd < 0) {
		lwsl_err("Cannot open %s\n", f);
		return 2;
	}
	lwsl_info("%s: %s\n", __func__, f);
	lejp_construct(&ctx, cb, user, paths, (uint8_t)(unsigned int)count_paths);

	do {
		n = (int)read(fd, buf, sizeof(buf));
		if (!n)
			break;

		m = lejp_parse(&ctx, buf, n);
	} while (m == LEJP_CONTINUE);

	close(fd);
	n = (int32_t)ctx.line;
	lejp_destruct(&ctx);

	if (m < 0) {
		lwsl_err("%s(%u): parsing error %d: %s\n", f, n, m,
			 lejp_error_to_string(m));
		return 2;
	}

	return 0;
}

struct lws_dir_args {
	void *user;
	const char * const *paths;
	int count_paths;
	lejp_callback cb;
};

static int
lwsws_get_config_d_cb(const char *dirpath, void *user,
		      struct lws_dir_entry *lde)
{
	struct lws_dir_args *da = (struct lws_dir_args *)user;
	char path[256];

	if (lde->type != LDOT_FILE && lde->type != LDOT_UNKNOWN /* ZFS */)
		return 0;

	lws_snprintf(path, sizeof(path) - 1, "%s/%s", dirpath, lde->name);

	return lwsws_get_config(da->user, path, da->paths,
				da->count_paths, da->cb);
}

int
lwsws_get_config_globals(struct lws_context_creation_info *info, const char *d,
			 char **cs, int *len)
{
	struct lws_dir_args da;
	struct jpargs a;
#if defined(LWS_WITH_PLUGINS)
	const char * const *old = info->plugin_dirs;
#endif
	char dd[128];

	memset(&a, 0, sizeof(a));

	a.info = info;
	a.p = *cs;
	a.end = (a.p + *len) - 1;
	a.valid = 0;

	lwsws_align(&a);
#if defined(LWS_WITH_PLUGINS)
	info->plugin_dirs = (void *)a.p;
#endif
	a.plugin_dirs = (void *)a.p; /* writeable version */
	a.p += MAX_PLUGIN_DIRS * sizeof(void *);

#if defined(LWS_WITH_PLUGINS)
	/* copy any default paths */

	while (old && *old) {
		a.plugin_dirs[a.count_plugin_dirs++] = *old;
		old++;
	}
#endif

	lws_snprintf(dd, sizeof(dd) - 1, "%s/conf", d);
	if (lwsws_get_config(&a, dd, paths_global,
			     LWS_ARRAY_SIZE(paths_global), lejp_globals_cb) > 1)
		return 1;
	lws_snprintf(dd, sizeof(dd) - 1, "%s/conf.d", d);

	da.user = &a;
	da.paths = paths_global;
	da.count_paths = LWS_ARRAY_SIZE(paths_global),
	da.cb = lejp_globals_cb;

	if (lws_dir(dd, &da, lwsws_get_config_d_cb) > 1)
		return 1;

	a.plugin_dirs[a.count_plugin_dirs] = NULL;

	*cs = a.p;
	*len = lws_ptr_diff(a.end, a.p);

	return 0;
}

int
lwsws_get_config_vhosts(struct lws_context *context,
			struct lws_context_creation_info *info, const char *d,
			char **cs, int *len)
{
	struct lws_dir_args da;
	struct jpargs a;
	char dd[128];

	memset(&a, 0, sizeof(a));

	a.info = info;
	a.p = *cs;
	a.end = a.p + *len;
	a.valid = 0;
	a.context = context;
	a.protocols = info->protocols;
	a.pprotocols = info->pprotocols;
#if defined(LWS_ROLE_WS)
	a.extensions = info->extensions;
#endif

	lws_snprintf(dd, sizeof(dd) - 1, "%s/conf", d);
	if (lwsws_get_config(&a, dd, paths_vhosts,
			     LWS_ARRAY_SIZE(paths_vhosts), lejp_vhosts_cb) > 1)
		return 1;
	lws_snprintf(dd, sizeof(dd) - 1, "%s/conf.d", d);

	da.user = &a;
	da.paths = paths_vhosts;
	da.count_paths = LWS_ARRAY_SIZE(paths_vhosts),
	da.cb = lejp_vhosts_cb;

	if (lws_dir(dd, &da, lwsws_get_config_d_cb) > 1)
		return 1;

	*cs = a.p;
	*len = lws_ptr_diff(a.end, a.p);

	if (!a.any_vhosts) {
		lwsl_err("Need at least one vhost\n");
		return 1;
	}

//	lws_finalize_startup(context);

	return 0;
}
