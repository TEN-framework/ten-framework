/*
 * libwebsockets ACME client protocol plugin
 *
 * Copyright (C) 2010 - 2022 Andy Green <andy@warmcat.com>
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
 *
 *  This implementation follows draft 7 of the IETF standard, and falls back
 *  to whatever differences exist for Boulder's tls-sni-01 challenge.
 *  tls-sni-02 is also supported.
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
#include <stdlib.h>

#include <sys/stat.h>
#include <fcntl.h>

typedef enum {
	ACME_STATE_DIRECTORY,	/* get the directory JSON using GET + parse */
	ACME_STATE_NEW_NONCE,	/* get the replay nonce */
	ACME_STATE_NEW_ACCOUNT,	/* register a new RSA key + email combo */
	ACME_STATE_NEW_ORDER,	/* start the process to request a cert */
	ACME_STATE_AUTHZ,	/* */
	ACME_STATE_START_CHALL, /* notify server ready for one challenge */
	ACME_STATE_POLLING,	/* he should be trying our challenge */
	ACME_STATE_POLLING_CSR,	/* sent CSR, checking result */
	ACME_STATE_DOWNLOAD_CERT,

	ACME_STATE_FINISHED
} lws_acme_state;

struct acme_connection {
	char buf[4096];
	char replay_nonce[64];
	char chall_token[64];
	char challenge_uri[256];
	char detail[64];
	char status[16];
	char key_auth[256];
	char http01_mountpoint[256];
	struct lws_http_mount mount;
	char urls[6][100]; /* directory contents */
	char active_url[100];
	char authz_url[100];
	char order_url[100];
	char finalize_url[100];
	char cert_url[100];
	char acct_id[100];
	char *kid;
	lws_acme_state state;
	struct lws_client_connect_info i;
	struct lejp_ctx jctx;
	struct lws_context_creation_info ci;
	struct lws_vhost *vhost;

	struct lws *cwsi;

	const char *real_vh_name;
	const char *real_vh_iface;

	char *alloc_privkey_pem;

	char *dest;
	int pos;
	int len;
	int resp;
	int cpos;

	int real_vh_port;
	int goes_around;

	size_t len_privkey_pem;

	unsigned int yes;
	unsigned int use:1;
	unsigned int is_sni_02:1;
};

struct per_vhost_data__lws_acme_client {
	struct lws_context *context;
	struct lws_vhost *vhost;
	const struct lws_protocols *protocol;

	/*
	 * the vhd is allocated for every vhost using the plugin.
	 * But ac is only allocated when we are doing the server auth.
	 */
	struct acme_connection *ac;

	struct lws_jwk jwk;
	struct lws_genrsa_ctx rsactx;

	char *pvo_data;
	char *pvop[LWS_TLS_TOTAL_COUNT];
	const char *pvop_active[LWS_TLS_TOTAL_COUNT];
	int count_live_pss;
	char *dest;
	int pos;
	int len;

	int fd_updated_cert; /* these are opened while we have root... */
	int fd_updated_key; /* ...if nonempty next startup will replace old */
};

static int
callback_chall_http01(struct lws *wsi, enum lws_callback_reasons reason,
        void *user, void *in, size_t len)
{
	struct lws_vhost *vhost = lws_get_vhost(wsi);
	struct acme_connection *ac = lws_vhost_user(vhost);
	uint8_t buf[LWS_PRE + 2048], *start = &buf[LWS_PRE], *p = start,
		*end = &buf[sizeof(buf) - 1];
	int n;

	switch (reason) {
	case LWS_CALLBACK_HTTP:
		lwsl_wsi_notice(wsi, "CA connection received, key_auth %s",
			    ac->key_auth);

		if (lws_add_http_header_status(wsi, HTTP_STATUS_OK, &p, end)) {
			lwsl_wsi_warn(wsi, "add status failed");
			return -1;
		}

		if (lws_add_http_header_by_token(wsi,
					WSI_TOKEN_HTTP_CONTENT_TYPE,
					(unsigned char *)"text/plain", 10,
					&p, end)) {
			lwsl_wsi_warn(wsi, "add content_type failed");
			return -1;
		}

		n = (int)strlen(ac->key_auth);
		if (lws_add_http_header_content_length(wsi, (lws_filepos_t)n, &p, end)) {
			lwsl_wsi_warn(wsi, "add content_length failed");
			return -1;
		}

		if (lws_add_http_header_by_token(wsi,
					WSI_TOKEN_HTTP_CONTENT_DISPOSITION,
					(unsigned char *)"attachment", 10,
					&p, end)) {
			lwsl_wsi_warn(wsi, "add content_dispo failed");
			return -1;
		}

		if (lws_finalize_write_http_header(wsi, start, &p, end)) {
			lwsl_wsi_warn(wsi, "finalize http header failed");
			return -1;
		}

		lws_callback_on_writable(wsi);
		return 0;

	case LWS_CALLBACK_HTTP_WRITEABLE:
		p += lws_snprintf((char *)p, lws_ptr_diff_size_t(end, p), "%s", ac->key_auth);
		// lwsl_notice("%s: len %d\n", __func__, lws_ptr_diff(p, start));
		if (lws_write(wsi, (uint8_t *)start, lws_ptr_diff_size_t(p, start),
			      LWS_WRITE_HTTP_FINAL) != lws_ptr_diff(p, start)) {
			lwsl_wsi_err(wsi, "_write content failed");
			return 1;
		}

		if (lws_http_transaction_completed(wsi))
			return -1;

		return 0;

	default:
		break;
	}

	return lws_callback_http_dummy(wsi, reason, user, in, len);
}

static const struct lws_protocols chall_http01_protocols[] = {
	{ "http", callback_chall_http01, 0, 0, 0, NULL, 0 },
	{ NULL, NULL, 0, 0, 0, NULL, 0 }
};

static int
jws_create_packet(struct lws_jwe *jwe, const char *payload, size_t len,
		  const char *nonce, const char *url, const char *kid,
		  char *out, size_t out_len, struct lws_context *context)
{
	char *buf, *start, *p, *end, *p1, *end1;
	struct lws_jws jws;
	int n, m;

	lws_jws_init(&jws, &jwe->jwk, context);

	/*
	 * This buffer is local to the function, the actual output is prepared
	 * into out.  Only the plaintext protected header
	 * (which contains the public key, 512 bytes for 4096b) goes in
	 * here temporarily.
	 */
	n = LWS_PRE + 2048;
	buf = malloc((unsigned int)n);
	if (!buf) {
		lwsl_warn("%s: malloc %d failed\n", __func__, n);
		return -1;
	}

	p = start = buf + LWS_PRE;
	end = buf + n - LWS_PRE - 1;

	/*
	 * temporary JWS protected header plaintext
	 */
	if (!jwe->jose.alg || !jwe->jose.alg->alg)
		goto bail;

	p += lws_snprintf(p, lws_ptr_diff_size_t(end, p), "{\"alg\":\"RS256\"");
	if (kid)
		p += lws_snprintf(p, lws_ptr_diff_size_t(end, p), ",\"kid\":\"%s\"", kid);
	else {
		p += lws_snprintf(p, lws_ptr_diff_size_t(end, p), ",\"jwk\":");
		m = lws_ptr_diff(end, p);
		n = lws_jwk_export(&jwe->jwk, 0, p, &m);
		if (n < 0) {
			lwsl_notice("failed to export jwk\n");
			goto bail;
		}
		p += n;
	}
	p += lws_snprintf(p, lws_ptr_diff_size_t(end, p), ",\"url\":\"%s\"", url);
	p += lws_snprintf(p, lws_ptr_diff_size_t(end, p), ",\"nonce\":\"%s\"}", nonce);

	/*
	 * prepare the signed outer JSON with all the parts in
	 */
	p1 = out;
	end1 = out + out_len - 1;

	p1 += lws_snprintf(p1, lws_ptr_diff_size_t(end1, p1), "{\"protected\":\"");
	jws.map_b64.buf[LJWS_JOSE] = p1;
	n = lws_jws_base64_enc(start, lws_ptr_diff_size_t(p, start), p1, lws_ptr_diff_size_t(end1, p1));
	if (n < 0) {
		lwsl_notice("%s: failed to encode protected\n", __func__);
		goto bail;
	}
	jws.map_b64.len[LJWS_JOSE] = (uint32_t)n;
	p1 += n;

	p1 += lws_snprintf(p1, lws_ptr_diff_size_t(end1, p1), "\",\"payload\":\"");
	jws.map_b64.buf[LJWS_PYLD] = p1;
	n = lws_jws_base64_enc(payload, len, p1, lws_ptr_diff_size_t(end1, p1));
	if (n < 0) {
		lwsl_notice("%s: failed to encode payload\n", __func__);
		goto bail;
	}
	jws.map_b64.len[LJWS_PYLD] = (uint32_t)n;
	p1 += n;

	p1 += lws_snprintf(p1, lws_ptr_diff_size_t(end1, p1), "\",\"signature\":\"");

	/*
	 * taking the b64 protected header and the b64 payload, sign them
	 * and place the signature into the packet
	 */
	n = lws_jws_sign_from_b64(&jwe->jose, &jws, p1, lws_ptr_diff_size_t(end1, p1));
	if (n < 0) {
		lwsl_notice("sig gen failed\n");

		goto bail;
	}
	jws.map_b64.buf[LJWS_SIG] = p1;
	jws.map_b64.len[LJWS_SIG] = (uint32_t)n;

	p1 += n;
	p1 += lws_snprintf(p1, lws_ptr_diff_size_t(end1, p1), "\"}");

	free(buf);

	return lws_ptr_diff(p1, out);

bail:
	lws_jws_destroy(&jws);
	free(buf);

	return -1;
}

static int
callback_acme_client(struct lws *wsi, enum lws_callback_reasons reason,
		void *user, void *in, size_t len);

#define LWS_PLUGIN_PROTOCOL_LWS_ACME_CLIENT \
{ \
	"lws-acme-client", \
	callback_acme_client, \
	0, \
	512, \
	0, NULL, 0 \
}

/* directory JSON parsing */

static const char * const jdir_tok[] = {
	"keyChange",
	"meta.termsOfService",
	"newAccount",
	"newNonce",
	"newOrder",
	"revokeCert",
};

enum enum_jdir_tok {
	JAD_KEY_CHANGE_URL,
	JAD_TOS_URL,
	JAD_NEW_ACCOUNT_URL,
	JAD_NEW_NONCE_URL,
	JAD_NEW_ORDER_URL,
	JAD_REVOKE_CERT_URL,
};

static signed char
cb_dir(struct lejp_ctx *ctx, char reason)
{
	struct per_vhost_data__lws_acme_client *s =
		(struct per_vhost_data__lws_acme_client *)ctx->user;

	if (reason == LEJPCB_VAL_STR_START && ctx->path_match) {
		s->pos = 0;
		s->len = sizeof(s->ac->urls[0]) - 1;
		s->dest = s->ac->urls[ctx->path_match - 1];
		return 0;
	}

	if (!(reason & LEJP_FLAG_CB_IS_VALUE) || !ctx->path_match)
		return 0;

	if (s->pos + ctx->npos > s->len) {
		lwsl_notice("url too long\n");
		return -1;
	}

	memcpy(s->dest + s->pos, ctx->buf, ctx->npos);
	s->pos += ctx->npos;
	s->dest[s->pos] = '\0';

	return 0;
}


/* order JSON parsing */

static const char * const jorder_tok[] = {
	"status",
	"expires",
	"identifiers[].type",
	"identifiers[].value",
	"authorizations",
	"finalize",
	"certificate"
};

enum enum_jorder_tok {
	JAO_STATUS,
	JAO_EXPIRES,
	JAO_IDENTIFIERS_TYPE,
	JAO_IDENTIFIERS_VALUE,
	JAO_AUTHORIZATIONS,
	JAO_FINALIZE,
	JAO_CERT
};

static signed char
cb_order(struct lejp_ctx *ctx, char reason)
{
	struct acme_connection *s = (struct acme_connection *)ctx->user;

	if (reason == LEJPCB_CONSTRUCTED)
		s->authz_url[0] = '\0';

	if (!(reason & LEJP_FLAG_CB_IS_VALUE) || !ctx->path_match)
		return 0;

	switch (ctx->path_match - 1) {
	case JAO_STATUS:
		lws_strncpy(s->status, ctx->buf, sizeof(s->status));
		break;
	case JAO_EXPIRES:
		break;
	case JAO_IDENTIFIERS_TYPE:
		break;
	case JAO_IDENTIFIERS_VALUE:
		break;
	case JAO_AUTHORIZATIONS:
		lws_snprintf(s->authz_url, sizeof(s->authz_url), "%s",
			     ctx->buf);
		break;
	case JAO_FINALIZE:
		lws_snprintf(s->finalize_url, sizeof(s->finalize_url), "%s",
				ctx->buf);
		break;
	case JAO_CERT:
		lws_snprintf(s->cert_url, sizeof(s->cert_url), "%s", ctx->buf);
		break;
	}

	return 0;
}

/* authz JSON parsing */

static const char * const jauthz_tok[] = {
	"identifier.type",
	"identifier.value",
	"status",
	"expires",
	"challenges[].type",
	"challenges[].status",
	"challenges[].url",
	"challenges[].token",
	"detail"
};

enum enum_jauthz_tok {
	JAAZ_ID_TYPE,
	JAAZ_ID_VALUE,
	JAAZ_STATUS,
	JAAZ_EXPIRES,
	JAAZ_CHALLENGES_TYPE,
	JAAZ_CHALLENGES_STATUS,
	JAAZ_CHALLENGES_URL,
	JAAZ_CHALLENGES_TOKEN,
	JAAZ_DETAIL,
};

static signed char
cb_authz(struct lejp_ctx *ctx, char reason)
{
	struct acme_connection *s = (struct acme_connection *)ctx->user;

	if (reason == LEJPCB_CONSTRUCTED) {
		s->yes = 0;
		s->use = 0;
		s->chall_token[0] = '\0';
	}

	if (!(reason & LEJP_FLAG_CB_IS_VALUE) || !ctx->path_match)
		return 0;

	switch (ctx->path_match - 1) {
	case JAAZ_ID_TYPE:
		break;
	case JAAZ_ID_VALUE:
		break;
	case JAAZ_STATUS:
		break;
	case JAAZ_EXPIRES:
		break;
	case JAAZ_DETAIL:
		lws_snprintf(s->detail, sizeof(s->detail), "%s", ctx->buf);
		break;
	case JAAZ_CHALLENGES_TYPE:
		lwsl_notice("JAAZ_CHALLENGES_TYPE: %s\n", ctx->buf);
		s->use = !strcmp(ctx->buf, "http-01");
		break;
	case JAAZ_CHALLENGES_STATUS:
		lws_strncpy(s->status, ctx->buf, sizeof(s->status));
		break;
	case JAAZ_CHALLENGES_URL:
		lwsl_notice("JAAZ_CHALLENGES_URL: %s %d\n", ctx->buf, s->use);
		if (s->use) {
			lws_strncpy(s->challenge_uri, ctx->buf,
				    sizeof(s->challenge_uri));
			s->yes = s->yes | 2;
		}
		break;
	case JAAZ_CHALLENGES_TOKEN:
		lwsl_notice("JAAZ_CHALLENGES_TOKEN: %s %d\n", ctx->buf, s->use);
		if (s->use) {
			lws_strncpy(s->chall_token, ctx->buf,
				    sizeof(s->chall_token));
			s->yes = s->yes | 1;
		}
		break;
	}

	return 0;
}

/* challenge accepted JSON parsing */

static const char * const jchac_tok[] = {
	"type",
	"status",
	"uri",
	"token",
	"error.detail"
};

enum enum_jchac_tok {
	JCAC_TYPE,
	JCAC_STATUS,
	JCAC_URI,
	JCAC_TOKEN,
	JCAC_DETAIL,
};

static signed char
cb_chac(struct lejp_ctx *ctx, char reason)
{
	struct acme_connection *s = (struct acme_connection *)ctx->user;

	if (reason == LEJPCB_CONSTRUCTED) {
		s->yes = 0;
		s->use = 0;
	}

	if (!(reason & LEJP_FLAG_CB_IS_VALUE) || !ctx->path_match)
		return 0;

	switch (ctx->path_match - 1) {
	case JCAC_TYPE:
		if (strcmp(ctx->buf, "http-01"))
			return 1;
		break;
	case JCAC_STATUS:
		lws_strncpy(s->status, ctx->buf, sizeof(s->status));
		break;
	case JCAC_URI:
		s->yes = s->yes | 2;
		break;
	case JCAC_TOKEN:
		lws_strncpy(s->chall_token, ctx->buf, sizeof(s->chall_token));
		s->yes = s->yes | 1;
		break;
	case JCAC_DETAIL:
		lws_snprintf(s->detail, sizeof(s->detail), "%s", ctx->buf);
		break;
	}

	return 0;
}

static int
lws_acme_report_status(struct lws_vhost *v, int state, const char *json)
{
	lws_callback_vhost_protocols_vhost(v, LWS_CALLBACK_VHOST_CERT_UPDATE,
					   (void *)json, (unsigned int)state);

	return 0;
}

/*
 * Notice: trashes i and url
 */
static struct lws *
lws_acme_client_connect(struct lws_context *context, struct lws_vhost *vh,
		struct lws **pwsi, struct lws_client_connect_info *i,
		char *url, const char *method)
{
	const char *prot, *p;
	char path[200], _url[256];
	struct lws *wsi;

	memset(i, 0, sizeof(*i));
	i->port = 443;
	lws_strncpy(_url, url, sizeof(_url));
	if (lws_parse_uri(_url, &prot, &i->address, &i->port, &p)) {
		lwsl_err("unable to parse uri %s\n", url);

		return NULL;
	}

	/* add back the leading / on path */
	path[0] = '/';
	lws_strncpy(path + 1, p, sizeof(path) - 1);
	i->path = path;
	i->context = context;
	i->vhost = vh;
	i->ssl_connection = LCCSCF_USE_SSL;
	i->host = i->address;
	i->origin = i->address;
	i->method = method;
	i->pwsi = pwsi;
	i->protocol = "lws-acme-client";

	wsi = lws_client_connect_via_info(i);
	if (!wsi) {
		lws_snprintf(path, sizeof(path) - 1,
			     "Unable to connect to %s", url);
		lwsl_notice("%s: %s\n", __func__, path);
		lws_acme_report_status(vh, LWS_CUS_FAILED, path);
	}

	return wsi;
}

static void
lws_acme_finished(struct per_vhost_data__lws_acme_client *vhd)
{
	lwsl_notice("%s\n", __func__);

	if (vhd->ac) {
		if (vhd->ac->vhost)
			lws_vhost_destroy(vhd->ac->vhost);
		if (vhd->ac->alloc_privkey_pem)
			free(vhd->ac->alloc_privkey_pem);
		free(vhd->ac);
	}

	lws_genrsa_destroy(&vhd->rsactx);
	lws_jwk_destroy(&vhd->jwk);

	vhd->ac = NULL;
#if defined(LWS_WITH_ESP32)
	lws_esp32.acme = 0; /* enable scanning */
#endif
}

static const char * const pvo_names[] = {
	"country",
	"state",
	"locality",
	"organization",
	"common-name",
	"subject-alt-name",
	"email",
	"directory-url",
	"auth-path",
	"cert-path",
	"key-path",
};

static int
lws_acme_load_create_auth_keys(struct per_vhost_data__lws_acme_client *vhd,
		int bits)
{
	int n;

	if (!lws_jwk_load(&vhd->jwk, vhd->pvop[LWS_TLS_SET_AUTH_PATH],
				NULL, NULL))
		return 0;

	vhd->jwk.kty = LWS_GENCRYPTO_KTY_RSA;

	lwsl_notice("Generating ACME %d-bit keypair... "
			"will take a little while\n", bits);
	n = lws_genrsa_new_keypair(vhd->context, &vhd->rsactx, LGRSAM_PKCS1_1_5,
			vhd->jwk.e, bits);
	if (n) {
		lwsl_vhost_warn(vhd->vhost, "failed to create keypair");
		return 1;
	}

	lwsl_notice("...keypair generated\n");

	if (lws_jwk_save(&vhd->jwk, vhd->pvop[LWS_TLS_SET_AUTH_PATH])) {
		lwsl_vhost_warn(vhd->vhost, "unable to save %s",
				vhd->pvop[LWS_TLS_SET_AUTH_PATH]);
		return 1;
	}

	return 0;
}

static int
lws_acme_start_acquisition(struct per_vhost_data__lws_acme_client *vhd,
		struct lws_vhost *v)
{
	char buf[128];

	/* ...and we were given enough info to do the update? */

	if (!vhd->pvop[LWS_TLS_REQ_ELEMENT_COMMON_NAME])
		return -1;

	/*
	 * ...well... we should try to do something about it then...
	 */
	lwsl_vhost_notice(vhd->vhost, "ACME cert needs creating / updating:  "
			"vhost %s", lws_get_vhost_name(vhd->vhost));

	vhd->ac = malloc(sizeof(*vhd->ac));
	memset(vhd->ac, 0, sizeof(*vhd->ac));

	/*
	 * So if we don't have it, the first job is get the directory.
	 *
	 * If we already have the directory, jump straight into trying
	 * to register our key.
	 *
	 * We always try to register the keys... if it's not the first
	 * time, we will get a JSON body in the (legal, nonfatal)
	 * response like this
	 *
	 * {
	 *   "type": "urn:acme:error:malformed",
	 *   "detail": "Registration key is already in use",
	 *   "status": 409
	 * }
	 */
	if (!vhd->ac->urls[0][0]) {
		vhd->ac->state = ACME_STATE_DIRECTORY;
		lws_snprintf(buf, sizeof(buf) - 1, "%s",
				vhd->pvop_active[LWS_TLS_SET_DIR_URL]);
	} else {
		vhd->ac->state = ACME_STATE_NEW_ACCOUNT;
		lws_snprintf(buf, sizeof(buf) - 1, "%s",
				vhd->ac->urls[JAD_NEW_ACCOUNT_URL]);
	}

	vhd->ac->real_vh_port = lws_get_vhost_port(vhd->vhost);
	vhd->ac->real_vh_name = lws_get_vhost_name(vhd->vhost);
	vhd->ac->real_vh_iface = lws_get_vhost_iface(vhd->vhost);

	lws_acme_report_status(vhd->vhost, LWS_CUS_STARTING, NULL);

#if defined(LWS_WITH_ESP32)
	lws_acme_report_status(vhd->vhost, LWS_CUS_CREATE_KEYS,
			"Generating keys, please wait");
	if (lws_acme_load_create_auth_keys(vhd, 2048))
		goto bail;
	lws_acme_report_status(vhd->vhost, LWS_CUS_CREATE_KEYS,
			"Auth keys created");
#endif

	if (lws_acme_client_connect(vhd->context, vhd->vhost,
				&vhd->ac->cwsi, &vhd->ac->i, buf, "GET"))
		return 0;

#if defined(LWS_WITH_ESP32)
bail:
#endif
	free(vhd->ac);
	vhd->ac = NULL;

	return 1;
}

static int
callback_acme_client(struct lws *wsi, enum lws_callback_reasons reason,
		void *user, void *in, size_t len)
{
	struct per_vhost_data__lws_acme_client *vhd =
		(struct per_vhost_data__lws_acme_client *)
		lws_protocol_vh_priv_get(lws_get_vhost(wsi),
				lws_get_protocol(wsi));
	char buf[LWS_PRE + 2536], *start = buf + LWS_PRE, *p = start,
		 *end = buf + sizeof(buf) - 1, digest[32], *failreason = NULL;
	const struct lws_protocol_vhost_options *pvo;
	struct lws_acme_cert_aging_args *caa;
	struct acme_connection *ac = NULL;
	unsigned char **pp, *pend;
	const char *content_type;
	struct lws_jwe jwe;
	struct lws *cwsi;
	int n, m;

	if (vhd)
		ac = vhd->ac;

	lws_jwe_init(&jwe, lws_get_context(wsi));

	switch ((int)reason) {
	case LWS_CALLBACK_PROTOCOL_INIT:
		if (vhd)
			return 0;
		vhd = lws_protocol_vh_priv_zalloc(lws_get_vhost(wsi),
				lws_get_protocol(wsi),
				sizeof(struct per_vhost_data__lws_acme_client));
		if (!vhd)
			return -1;

		vhd->context = lws_get_context(wsi);
		vhd->protocol = lws_get_protocol(wsi);
		vhd->vhost = lws_get_vhost(wsi);

		/* compute how much we need to hold all the pvo payloads */
		m = 0;
		pvo = (const struct lws_protocol_vhost_options *)in;
		while (pvo) {
			m += (int)strlen(pvo->value) + 1;
			pvo = pvo->next;
		}
		p = vhd->pvo_data = malloc((unsigned int)m);
		if (!p)
			return -1;

		pvo = (const struct lws_protocol_vhost_options *)in;
		while (pvo) {
			start = p;
			n = (int)strlen(pvo->value) + 1;
			memcpy(start, pvo->value, (unsigned int)n);
			p += n;

			for (m = 0; m < (int)LWS_ARRAY_SIZE(pvo_names); m++)
				if (!strcmp(pvo->name, pvo_names[m]))
					vhd->pvop[m] = start;

			pvo = pvo->next;
		}

		n = 0;
		for (m = 0; m < (int)LWS_ARRAY_SIZE(pvo_names); m++) {
			if (!vhd->pvop[m] &&
				m >= LWS_TLS_REQ_ELEMENT_COMMON_NAME &&
				m != LWS_TLS_REQ_ELEMENT_SUBJECT_ALT_NAME) {
				lwsl_notice("%s: require pvo '%s'\n", __func__,
					    pvo_names[m]);
				n |= 1;
			} else {
				if (vhd->pvop[m])
					lwsl_info("  %s: %s\n", pvo_names[m],
						  vhd->pvop[m]);
			}
		}
		if (n) {
			free(vhd->pvo_data);
			vhd->pvo_data = NULL;

			return -1;
		}

#if !defined(LWS_WITH_ESP32)
		/*
		 * load (or create) the registration keypair while we
		 * still have root
		 */
		if (lws_acme_load_create_auth_keys(vhd, 4096))
			return 1;

		/*
		 * in case we do an update, open the update files while we
		 * still have root
		 */
		lws_snprintf(buf, sizeof(buf) - 1, "%s.upd",
				vhd->pvop[LWS_TLS_SET_CERT_PATH]);
		vhd->fd_updated_cert = lws_open(buf,
						LWS_O_WRONLY | LWS_O_CREAT |
						LWS_O_TRUNC
		/*do not replace \n to \r\n on Windows */
		#ifdef WIN32
			| O_BINARY
		#endif
			, 0600);
		if (vhd->fd_updated_cert < 0) {
			lwsl_err("unable to create update cert file %s\n", buf);
			return -1;
		}
		lws_snprintf(buf, sizeof(buf) - 1, "%s.upd",
				vhd->pvop[LWS_TLS_SET_KEY_PATH]);
		vhd->fd_updated_key = lws_open(buf, LWS_O_WRONLY | LWS_O_CREAT |
			/*do not replace \n to \r\n on Windows */
		#ifdef WIN32
			O_BINARY |
		#endif
			LWS_O_TRUNC, 0600);
		if (vhd->fd_updated_key < 0) {
			lwsl_vhost_err(vhd->vhost, "unable to create update key file %s", buf);

			return -1;
		}
#endif
		break;

	case LWS_CALLBACK_PROTOCOL_DESTROY:
		if (vhd && vhd->pvo_data) {
			free(vhd->pvo_data);
			vhd->pvo_data = NULL;
		}
		if (vhd)
			lws_acme_finished(vhd);
		break;

	case LWS_CALLBACK_VHOST_CERT_AGING:
		if (!vhd)
			break;

		caa = (struct lws_acme_cert_aging_args *)in;
		/*
		 * Somebody is telling us about a cert some vhost is using.
		 *
		 * First see if the cert is getting close enough to expiry that
		 * we *want* to do something about it.
		 */
		if ((int)(ssize_t)len > 14)
			break;

		/*
		 * ...is this a vhost we were configured on?
		 */
		if (vhd->vhost != caa->vh)
			return 1;

		for (n = 0; n < (int)LWS_ARRAY_SIZE(vhd->pvop);n++)
			if (caa->element_overrides[n])
				vhd->pvop_active[n] = caa->element_overrides[n];
			else
				vhd->pvop_active[n] = vhd->pvop[n];

		lwsl_notice("starting acme acquisition on %s: %s\n",
				lws_get_vhost_name(caa->vh),
				vhd->pvop_active[LWS_TLS_SET_DIR_URL]);

		lws_acme_start_acquisition(vhd, caa->vh);
		break;

	/*
	 * Client
	 */

	case LWS_CALLBACK_ESTABLISHED_CLIENT_HTTP:
		if (!ac)
			break;

		ac->resp = (int)lws_http_client_http_response(wsi);

		/* we get a new nonce each time */
		if (lws_hdr_total_length(wsi, WSI_TOKEN_REPLAY_NONCE) &&
				lws_hdr_copy(wsi, ac->replay_nonce,
					sizeof(ac->replay_nonce),
					WSI_TOKEN_REPLAY_NONCE) < 0) {
			lwsl_vhost_warn(vhd->vhost, "nonce too large");

			goto failed;
		}

		switch (ac->state) {
		case ACME_STATE_DIRECTORY:
			lejp_construct(&ac->jctx, cb_dir, vhd, jdir_tok,
					LWS_ARRAY_SIZE(jdir_tok));
			break;

		case ACME_STATE_NEW_NONCE:
			/*
			 *  we try to register our keys next.
			 *  It's OK if it ends up they're already registered,
			 *  this eliminates any gaps where we stored the key
			 *  but registration did not complete for some reason...
			 */
			ac->state = ACME_STATE_NEW_ACCOUNT;
			lws_acme_report_status(vhd->vhost, LWS_CUS_REG, NULL);

			strcpy(buf, ac->urls[JAD_NEW_ACCOUNT_URL]);
			cwsi = lws_acme_client_connect(vhd->context, vhd->vhost,
					&ac->cwsi, &ac->i, buf, "POST");
			if (!cwsi) {
				lwsl_vhost_warn(vhd->vhost, "failed to connect to acme");
				goto failed;
			}

			return -1;

		case ACME_STATE_NEW_ACCOUNT:
			if (!lws_hdr_total_length(wsi,
						  WSI_TOKEN_HTTP_LOCATION)) {
				lwsl_vhost_warn(vhd->vhost, "no Location");
				goto failed;
			}

			if (lws_hdr_copy(wsi, ac->acct_id, sizeof(ac->acct_id),
					 WSI_TOKEN_HTTP_LOCATION) < 0) {
				lwsl_vhost_warn(vhd->vhost, "Location too large");
				goto failed;
			}

			ac->kid = ac->acct_id;

			lwsl_vhost_notice(vhd->vhost, "Location: %s", ac->acct_id);
			break;

		case ACME_STATE_NEW_ORDER:
			if (lws_hdr_copy(wsi, ac->order_url,
					 sizeof(ac->order_url),
					 WSI_TOKEN_HTTP_LOCATION) < 0) {
				lwsl_vhost_warn(vhd->vhost, "missing cert location");

				goto failed;
			}

			lejp_construct(&ac->jctx, cb_order, ac, jorder_tok,
					LWS_ARRAY_SIZE(jorder_tok));
			break;

		case ACME_STATE_AUTHZ:
			lejp_construct(&ac->jctx, cb_authz, ac, jauthz_tok,
					LWS_ARRAY_SIZE(jauthz_tok));
			break;

		case ACME_STATE_START_CHALL:
			lejp_construct(&ac->jctx, cb_chac, ac, jchac_tok,
					LWS_ARRAY_SIZE(jchac_tok));
			break;

		case ACME_STATE_POLLING:
		case ACME_STATE_POLLING_CSR:
			lejp_construct(&ac->jctx, cb_order, ac, jorder_tok,
					LWS_ARRAY_SIZE(jorder_tok));
			break;

		case ACME_STATE_DOWNLOAD_CERT:
			ac->cpos = 0;
			break;

		default:
			break;
		}
		break;

	case LWS_CALLBACK_CLIENT_APPEND_HANDSHAKE_HEADER:
		if (!ac)
			break;

		switch (ac->state) {
		case ACME_STATE_DIRECTORY:
		case ACME_STATE_NEW_NONCE:
			break;

		case ACME_STATE_NEW_ACCOUNT:
			p += lws_snprintf(p, lws_ptr_diff_size_t(end, p), "{"
				"\"termsOfServiceAgreed\":true"
				",\"contact\": [\"mailto:%s\"]}",
				vhd->pvop_active[LWS_TLS_REQ_ELEMENT_EMAIL]);

			strcpy(ac->active_url, ac->urls[JAD_NEW_ACCOUNT_URL]);
pkt_add_hdrs:
			if (lws_gencrypto_jwe_alg_to_definition("RSA1_5",
						&jwe.jose.alg)) {
				ac->len = 0;
				lwsl_notice("%s: no RSA1_5\n", __func__);
				goto failed;
			}
			jwe.jwk = vhd->jwk;

			ac->len = jws_create_packet(&jwe,
					start, lws_ptr_diff_size_t(p, start),
					ac->replay_nonce,
					ac->active_url,
					ac->kid,
					&ac->buf[LWS_PRE],
					sizeof(ac->buf) - LWS_PRE,
					lws_get_context(wsi));
			if (ac->len < 0) {
				ac->len = 0;
				lwsl_notice("jws_create_packet failed\n");
				goto failed;
			}

			pp = (unsigned char **)in;
			pend = (*pp) + len;

			ac->pos = 0;
			content_type = "application/jose+json";

			if (lws_add_http_header_by_token(wsi,
						WSI_TOKEN_HTTP_CONTENT_TYPE,
						(uint8_t *)content_type, 21, pp,
						pend)) {
				lwsl_vhost_warn(vhd->vhost, "could not add content type");
				goto failed;
			}

			n = sprintf(buf, "%d", ac->len);
			if (lws_add_http_header_by_token(wsi,
						WSI_TOKEN_HTTP_CONTENT_LENGTH,
						(uint8_t *)buf, n, pp, pend)) {
				lwsl_vhost_warn(vhd->vhost, "could not add content length");
				goto failed;
			}

			lws_client_http_body_pending(wsi, 1);
			lws_callback_on_writable(wsi);
			break;

		case ACME_STATE_NEW_ORDER:
			p += lws_snprintf(p, lws_ptr_diff_size_t(end, p),
					"{"
					"\"identifiers\":[{"
					"\"type\":\"dns\","
					"\"value\":\"%s\""
					"}]"
					"}",
			vhd->pvop_active[LWS_TLS_REQ_ELEMENT_COMMON_NAME]);

			strcpy(ac->active_url, ac->urls[JAD_NEW_ORDER_URL]);
			goto pkt_add_hdrs;

		case ACME_STATE_AUTHZ:
			strcpy(ac->active_url, ac->authz_url);
			goto pkt_add_hdrs;

		case ACME_STATE_START_CHALL:
			p = start;
			end = &buf[sizeof(buf) - 1];

			p += lws_snprintf(p, lws_ptr_diff_size_t(end, p), "{}");
			strcpy(ac->active_url, ac->challenge_uri);
			goto pkt_add_hdrs;

		case ACME_STATE_POLLING:
			strcpy(ac->active_url, ac->order_url);
			goto pkt_add_hdrs;

		case ACME_STATE_POLLING_CSR:
			if (ac->goes_around)
				break;
			lwsl_vhost_notice(vhd->vhost, "Generating ACME CSR... may take a little while");
			p += lws_snprintf(p, lws_ptr_diff_size_t(end, p), "{\"csr\":\"");
			n = lws_tls_acme_sni_csr_create(vhd->context,
					&vhd->pvop_active[0],
					(uint8_t *)p, lws_ptr_diff_size_t(end, p),
					&ac->alloc_privkey_pem,
					&ac->len_privkey_pem);
			if (n < 0) {
				lwsl_vhost_warn(vhd->vhost, "CSR generation failed");
				goto failed;
			}
			p += n;
			p += lws_snprintf(p, lws_ptr_diff_size_t(end, p), "\"}");
			strcpy(ac->active_url, ac->finalize_url);
			goto pkt_add_hdrs;

		case ACME_STATE_DOWNLOAD_CERT:
			strcpy(ac->active_url, ac->cert_url);
			goto pkt_add_hdrs;
			break;

		default:
			break;
		}
		break;

	case LWS_CALLBACK_CLIENT_HTTP_WRITEABLE:

		if (!ac)
			break;

		if (ac->pos == ac->len)
			break;

		ac->buf[LWS_PRE + ac->len] = '\0';
		if (lws_write(wsi, (uint8_t *)ac->buf + LWS_PRE,
					(size_t)ac->len, LWS_WRITE_HTTP_FINAL) < 0)
			return -1;

		ac->pos = ac->len;
		lws_client_http_body_pending(wsi, 0);
		break;

	/* chunked content */
	case LWS_CALLBACK_RECEIVE_CLIENT_HTTP_READ:
		if (!ac)
			return -1;

		switch (ac->state) {
		case ACME_STATE_POLLING_CSR:
		case ACME_STATE_POLLING:
		case ACME_STATE_START_CHALL:
		case ACME_STATE_AUTHZ:
		case ACME_STATE_NEW_ORDER:
		case ACME_STATE_DIRECTORY:

			m = lejp_parse(&ac->jctx, (uint8_t *)in, (int)len);
			if (m < 0 && m != LEJP_CONTINUE) {
				lwsl_notice("lejp parse failed %d\n", m);
				goto failed;
			}
			break;

		case ACME_STATE_NEW_ACCOUNT:
			break;

		case ACME_STATE_DOWNLOAD_CERT:
			/*
			 * It should be the DER cert...
			 * ACME 2.0 can send certs chain with 3 certs, store only first bytes
			 */
			if ((unsigned int)ac->cpos + len > sizeof(ac->buf))
				len = sizeof(ac->buf) - (unsigned int)ac->cpos;

			if (len) {
				memcpy(&ac->buf[ac->cpos], in, len);
				ac->cpos += (int)len;
			}
			break;
		default:
			break;
		}
		break;

	/* unchunked content */
	case LWS_CALLBACK_RECEIVE_CLIENT_HTTP:
		if (!ac)
			return -1;

		switch (ac->state) {
		default:
			{
				char buffer[2048 + LWS_PRE];
				char *px = buffer + LWS_PRE;
				int lenx = sizeof(buffer) - LWS_PRE;

				if (lws_http_client_read(wsi, &px, &lenx) < 0)
					return -1;
			}
			break;
		}
		break;

	case LWS_CALLBACK_COMPLETED_CLIENT_HTTP:

		if (!ac)
			return -1;

		switch (ac->state) {
		case ACME_STATE_DIRECTORY:
			lejp_destruct(&ac->jctx);

			/* check dir validity */

			for (n = 0; n < 6; n++)
				lwsl_notice("   %d: %s\n", n, ac->urls[n]);

			ac->state = ACME_STATE_NEW_NONCE;

			strcpy(buf, ac->urls[JAD_NEW_NONCE_URL]);
			cwsi = lws_acme_client_connect(vhd->context, vhd->vhost,
					&ac->cwsi, &ac->i, buf,
					"GET");
			if (!cwsi) {
				lwsl_notice("%s: failed to connect to acme\n",
						__func__);
				goto failed;
			}
			return -1; /* close the completed client connection */

		case ACME_STATE_NEW_ACCOUNT:
			if ((ac->resp >= 200 && ac->resp < 299) ||
			    ac->resp == 409) {
				/*
				 * Our account already existed, or exists now.
				 *
				 */
				ac->state = ACME_STATE_NEW_ORDER;

				strcpy(buf, ac->urls[JAD_NEW_ORDER_URL]);
				cwsi = lws_acme_client_connect(vhd->context,
						vhd->vhost, &ac->cwsi,
						&ac->i, buf, "POST");
				if (!cwsi)
					lwsl_notice("%s: failed to connect\n",
							__func__);

				/* close the completed client connection */
				return -1;
			} else {
				lwsl_notice("newAccount replied %d\n",
						ac->resp);
				goto failed;
			}
			return -1; /* close the completed client connection */

		case ACME_STATE_NEW_ORDER:
			lejp_destruct(&ac->jctx);
			if (!ac->authz_url[0]) {
				lwsl_notice("no authz\n");
				goto failed;
			}

			/*
			 * Move on to requesting a cert auth.
			 */
			ac->state = ACME_STATE_AUTHZ;
			lws_acme_report_status(vhd->vhost, LWS_CUS_AUTH,
					NULL);

			strcpy(buf, ac->authz_url);
			cwsi = lws_acme_client_connect(vhd->context,
					vhd->vhost, &ac->cwsi,
					&ac->i, buf, "POST");
			if (!cwsi)
				lwsl_notice("%s: failed to connect\n", __func__);

			return -1; /* close the completed client connection */

		case ACME_STATE_AUTHZ:
			lejp_destruct(&ac->jctx);
			if (ac->resp / 100 == 4) {
				lws_snprintf(buf, sizeof(buf),
						"Auth failed: %s", ac->detail);
				failreason = buf;
				lwsl_vhost_warn(vhd->vhost, "auth failed");
				goto failed;
			}
			lwsl_vhost_info(vhd->vhost, "chall: %s (%d)\n", ac->chall_token, ac->resp);
			if (!ac->chall_token[0]) {
				lwsl_vhost_warn(vhd->vhost, "no challenge");
				goto failed;
			}

			ac->state = ACME_STATE_START_CHALL;
			lws_acme_report_status(vhd->vhost, LWS_CUS_CHALLENGE,
					NULL);

			memset(&ac->ci, 0, sizeof(ac->ci));

			/* compute the key authorization */

			p = ac->key_auth;
			end = p + sizeof(ac->key_auth) - 1;

			p += lws_snprintf(p, lws_ptr_diff_size_t(end, p), "%s.", ac->chall_token);
			lws_jwk_rfc7638_fingerprint(&vhd->jwk, digest);
			n = lws_jws_base64_enc(digest, 32, p, lws_ptr_diff_size_t(end, p));
			if (n < 0)
				goto failed;

			lwsl_vhost_notice(vhd->vhost, "key_auth: '%s'", ac->key_auth);

			lws_snprintf(ac->http01_mountpoint,
					sizeof(ac->http01_mountpoint),
					"/.well-known/acme-challenge/%s",
					ac->chall_token);

			memset(&ac->mount, 0, sizeof (struct lws_http_mount));
			ac->mount.protocol = "http";
			ac->mount.mountpoint = ac->http01_mountpoint;
			ac->mount.mountpoint_len = (unsigned char)
				strlen(ac->http01_mountpoint);
			ac->mount.origin_protocol = LWSMPRO_CALLBACK;

			ac->ci.mounts = &ac->mount;

			/* listen on the same port as the vhost that triggered us */
			ac->ci.port = 80;

			/* make ourselves protocols[0] for the new vhost */
			ac->ci.protocols = chall_http01_protocols;

			/*
			 * vhost .user points to the ac associated with the
			 * temporary vhost
			 */
			ac->ci.user = ac;

			ac->vhost = lws_create_vhost(lws_get_context(wsi),
					&ac->ci);
			if (!ac->vhost)
				goto failed;

			lwsl_vhost_notice(vhd->vhost, "challenge_uri %s", ac->challenge_uri);

			/*
			 * The challenge-specific vhost is up... let the ACME
			 * server know we are ready to roll...
			 */
			ac->goes_around = 0;
			cwsi = lws_acme_client_connect(vhd->context, vhd->vhost,
						       &ac->cwsi, &ac->i,
						       ac->challenge_uri,
						       "POST");
			if (!cwsi) {
				lwsl_vhost_warn(vhd->vhost, "Connect failed");
				goto failed;
			}
			return -1; /* close the completed client connection */

		case ACME_STATE_START_CHALL:
			lwsl_vhost_notice(vhd->vhost, "COMPLETED start chall: %s",
				          ac->challenge_uri);
poll_again:
			ac->state = ACME_STATE_POLLING;
			lws_acme_report_status(vhd->vhost, LWS_CUS_CHALLENGE,
					       NULL);

			if (ac->goes_around++ == 20) {
				lwsl_notice("%s: too many chall retries\n",
						__func__);

				goto failed;
			}

			strcpy(buf, ac->order_url);
			cwsi = lws_acme_client_connect(vhd->context, vhd->vhost,
						       &ac->cwsi, &ac->i, buf,
						       "POST");
			if (!cwsi) {
				lwsl_vhost_warn(vhd->vhost, "failed to connect to acme");

				goto failed;
			}
			return -1; /* close the completed client connection */

		case ACME_STATE_POLLING:

			if (ac->resp == 202 && strcmp(ac->status, "invalid") &&
					       strcmp(ac->status, "valid"))
				goto poll_again;

			if (!strcmp(ac->status, "pending"))
				goto poll_again;

			if (!strcmp(ac->status, "invalid")) {
				lwsl_vhost_warn(vhd->vhost, "Challenge failed");
				lws_snprintf(buf, sizeof(buf),
						"Challenge Invalid: %s",
						ac->detail);
				failreason = buf;
				goto failed;
			}

			lwsl_vhost_notice(vhd->vhost, "ACME challenge passed");

			/*
			 * The challenge was validated... so delete the
			 * temp vhost now its job is done
			 */
			if (ac->vhost)
				lws_vhost_destroy(ac->vhost);
			ac->vhost = NULL;

			/*
			 * now our JWK is accepted as authorized to make
			 * requests for the domain, next move is create the
			 * CSR signed with the JWK, and send it to the ACME
			 * server to request the actual certs.
			 */
			ac->state = ACME_STATE_POLLING_CSR;
			lws_acme_report_status(vhd->vhost, LWS_CUS_REQ, NULL);
			ac->goes_around = 0;

			strcpy(buf, ac->finalize_url);
			cwsi = lws_acme_client_connect(vhd->context, vhd->vhost,
						       &ac->cwsi, &ac->i, buf,
						       "POST");
			if (!cwsi) {
				lwsl_vhost_warn(vhd->vhost, "Failed to connect to acme");

				goto failed;
			}
			return -1; /* close the completed client connection */

		case ACME_STATE_POLLING_CSR:
			if (ac->resp < 200 || ac->resp > 202) {
				lwsl_notice("CSR poll failed on resp %d\n",
						ac->resp);
				goto failed;
			}

			if (ac->resp != 200) {
				if (ac->goes_around++ == 30) {
					lwsl_vhost_warn(vhd->vhost, "Too many retries");

					goto failed;
				}
				strcpy(buf, ac->finalize_url);
				cwsi = lws_acme_client_connect(vhd->context,
						vhd->vhost,
						&ac->cwsi, &ac->i, buf,
						"POST");
				if (!cwsi) {
					lwsl_vhost_warn(vhd->vhost,
						"Failed to connect to acme");

					goto failed;
				}
				/* close the completed client connection */
				return -1;
			}

			ac->state = ACME_STATE_DOWNLOAD_CERT;

			strcpy(buf, ac->cert_url);
			cwsi = lws_acme_client_connect(vhd->context, vhd->vhost,
						       &ac->cwsi, &ac->i, buf,
						       "POST");
			if (!cwsi) {
				lwsl_vhost_warn(vhd->vhost, "Failed to connect to acme");

				goto failed;
			}
			return -1;

		case ACME_STATE_DOWNLOAD_CERT:

			if (ac->resp != 200) {
				lwsl_vhost_warn(vhd->vhost, "Download cert failed on resp %d",
					    ac->resp);
				goto failed;
			}
			lwsl_vhost_notice(vhd->vhost, "The cert was sent..");

			lws_acme_report_status(vhd->vhost, LWS_CUS_ISSUE, NULL);

			/*
			 * That means we have the issued cert in
			 * ac->buf, length in ac->cpos; and the key in
			 * ac->alloc_privkey_pem, length in
			 * ac->len_privkey_pem.
			 * ACME 2.0 can send certs chain with 3 certs, we need save only first
			 */
			{
				char *end_cert = strstr(ac->buf, "END CERTIFICATE-----");

				if (end_cert) {
					ac->cpos = (int)(lws_ptr_diff_size_t(end_cert, ac->buf) + sizeof("END CERTIFICATE-----") - 1);
				} else {
					ac->cpos = 0;
					lwsl_vhost_err(vhd->vhost, "Unable to find ACME cert!");
					goto failed;
				}
			}
			n = lws_plat_write_cert(vhd->vhost, 0,
					vhd->fd_updated_cert,
					ac->buf,
					(size_t)ac->cpos);
			if (n) {
				lwsl_vhost_err(vhd->vhost, "unable to write ACME cert! %d", n);
				goto failed;
			}

			/*
			 * don't close it... we may update the certs
			 * again
			 */
			if (lws_plat_write_cert(vhd->vhost, 1,
						vhd->fd_updated_key,
						ac->alloc_privkey_pem,
						ac->len_privkey_pem)) {
				lwsl_vhost_err(vhd->vhost, "unable to write ACME key!");
				goto failed;
			}

			/*
			 * we have written the persistent copies
			 */
			lwsl_vhost_notice(vhd->vhost, "Updated certs written for %s "
					"to %s.upd and %s.upd",
				vhd->pvop_active[LWS_TLS_REQ_ELEMENT_COMMON_NAME],
				vhd->pvop_active[LWS_TLS_SET_CERT_PATH],
				vhd->pvop_active[LWS_TLS_SET_KEY_PATH]);

			/* notify lws there was a cert update */

			if (lws_tls_cert_updated(vhd->context,
					vhd->pvop_active[LWS_TLS_SET_CERT_PATH],
					vhd->pvop_active[LWS_TLS_SET_KEY_PATH],
						ac->buf, (size_t)ac->cpos,
						ac->alloc_privkey_pem,
						ac->len_privkey_pem)) {
				lwsl_vhost_warn(vhd->vhost, "problem setting certs");
			}

			lws_acme_finished(vhd);
			lws_acme_report_status(vhd->vhost,
					LWS_CUS_SUCCESS, NULL);

			return -1;

		default:
			break;
		}
		break;

	case LWS_CALLBACK_USER + 0xac33:
		if (!vhd)
			break;
		cwsi = lws_acme_client_connect(vhd->context, vhd->vhost,
				&ac->cwsi, &ac->i,
				ac->challenge_uri,
				"GET");
		if (!cwsi) {
			lwsl_vhost_warn(vhd->vhost, "Failed to connect");
			goto failed;
		}
		break;

	default:
		break;
	}

	return 0;

failed:
	lwsl_vhost_warn(vhd->vhost, "Failed out");
	lws_acme_report_status(vhd->vhost, LWS_CUS_FAILED, failreason);
	lws_acme_finished(vhd);

	return -1;
}

#if !defined (LWS_PLUGIN_STATIC)

LWS_VISIBLE const struct lws_protocols lws_acme_client_protocols[] = {
	LWS_PLUGIN_PROTOCOL_LWS_ACME_CLIENT
};

LWS_VISIBLE const lws_plugin_protocol_t protocol_lws_acme_client = {
	.hdr = {
		"acme client",
		"lws_protocol_plugin",
		LWS_BUILD_HASH,
		LWS_PLUGIN_API_MAGIC
	},

	.protocols = lws_acme_client_protocols,
	.count_protocols = LWS_ARRAY_SIZE(lws_acme_client_protocols),
	.extensions = NULL,
	.count_extensions = 0,
};

#endif
