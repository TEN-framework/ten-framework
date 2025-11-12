/*
 * Sigv4 support for Secure Streams
 *
 * libwebsockets - small server side websockets and web server implementation
 *
 * Copyright (C) 2020 Andy Green <andy@warmcat.com>
 *                    securestreams-dev@amazon.com
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

#include <private-lib-core.h>

struct sigv4_header {
	const char * name;
	const char * value;
};

#define MAX_HEADER_NUM 8
struct sigv4 {
	struct sigv4_header headers[MAX_HEADER_NUM];
	uint8_t	hnum;
	char	ymd[10];     /*YYYYMMDD*/
	const char *timestamp;
	const char *payload_hash;
	const char *region;
	const char *service;
};

static const uint8_t blob_idx[] = {
	LWS_SYSBLOB_TYPE_EXT_AUTH1,
	LWS_SYSBLOB_TYPE_EXT_AUTH2,
	LWS_SYSBLOB_TYPE_EXT_AUTH3,
	LWS_SYSBLOB_TYPE_EXT_AUTH4,
};

enum {
	LWS_SS_SIGV4_KEYID,
	LWS_SS_SIGV4_KEY,
	LWS_SS_SIGV4_BLOB_SLOTS
};

static inline int add_header(struct sigv4 *s, const char *name, const char *value)
{
	if (s->hnum >= MAX_HEADER_NUM) {
		lwsl_err("%s too many sigv4 headers\n", __func__);
		return -1;
	}

	s->headers[s->hnum].name = name;
	s->headers[s->hnum].value = value;
	s->hnum++;

	if (!strncmp(name, "x-amz-content-sha256", strlen("x-amz-content-sha256")))
		s->payload_hash = value;

	if (!strncmp(name, "x-amz-date", strlen("x-amz-date"))) {
		s->timestamp = value;
		strncpy(s->ymd, value, 8);
	}

	return 0;
}

static int
cmp_header(const void * a, const void * b)
{
	return strcmp(((struct sigv4_header *)a)->name,
			((struct sigv4_header *)b)->name);
}

static int
init_sigv4(struct lws *wsi, struct lws_ss_handle *h, struct sigv4 *s)
{
	lws_ss_metadata_t *polmd = h->policy->metadata;
	int m = 0;

	add_header(s, "host:", lws_hdr_simple_ptr(wsi, _WSI_TOKEN_CLIENT_HOST));

	while (polmd) {
		if (polmd->value__may_own_heap &&
		    ((uint8_t *)polmd->value__may_own_heap)[0] &&
		    h->metadata[m].value__may_own_heap) {
			/* consider all headers start with "x-amz-" need to be signed */
			if (!strncmp(polmd->value__may_own_heap, "x-amz-",
				     strlen("x-amz-"))) {
				if (add_header(s, polmd->value__may_own_heap,
					       h->metadata[m].value__may_own_heap))
					return -1;
			}
		}
		if (!strcmp(h->metadata[m].name, h->policy->aws_region) &&
		    h->metadata[m].value__may_own_heap)
			s->region = h->metadata[m].value__may_own_heap;

		if (!strcmp(h->metadata[m].name, h->policy->aws_service) &&
		    h->metadata[m].value__may_own_heap)
			s->service = h->metadata[m].value__may_own_heap;

		m++;
		polmd = polmd->next;
	}

	qsort(s->headers, s->hnum, sizeof(struct sigv4_header), cmp_header);

#if 0
	do {
		int i;
		for (i= 0; i<s->hnum; i++)
			lwsl_debug("%s hdr %s %s\n", __func__,
					s->headers[i].name, s->headers[i].value);

		lwsl_debug("%s service: %s region: %s\n", __func__,
				s->service, s->region);
	} while(0);
#endif

	return 0;
}

static void
bin2hex(uint8_t *in, size_t len, char *out)
{
	static const char *hex = "0123456789abcdef";
	size_t n;

	for (n = 0; n < len; n++) {
		*out++ = hex[(in[n] >> 4) & 0xf];
		*out++ = hex[in[n] & 15];
	}
	*out = '\0';
}

static int
hmacsha256(const uint8_t *key, size_t keylen, const uint8_t *txt,
			size_t txtlen, uint8_t *digest)
{
	struct lws_genhmac_ctx hmacctx;

	if (lws_genhmac_init(&hmacctx, LWS_GENHMAC_TYPE_SHA256,
				key, keylen))
		return -1;

	if (lws_genhmac_update(&hmacctx, txt, txtlen)) {
		lwsl_err("%s: hmac computation failed\n", __func__);
		lws_genhmac_destroy(&hmacctx, NULL);
		return -1;
	}

	if (lws_genhmac_destroy(&hmacctx, digest)) {
		lwsl_err("%s: problem destroying hmac\n", __func__);
		return -1;
	}

	return 0;
}

/* cut the last byte of the str */
static inline int hash_update_bite_str(struct lws_genhash_ctx *ctx, const char * str)
{
	int ret = 0;
	if ((ret = lws_genhash_update(ctx, (void *)str, strlen(str)-1))) {
		lws_genhash_destroy(ctx, NULL);
		lwsl_err("%s err %d line \n", __func__, ret);
	}
	return ret;
}

static inline int hash_update_str(struct lws_genhash_ctx *ctx, const char * str)
{
	int ret = 0;
	if ((ret = lws_genhash_update(ctx, (void *)str, strlen(str)))) {
		lws_genhash_destroy(ctx, NULL);
		lwsl_err("%s err %d \n", __func__, ret);
	}
	return ret;
}

static int
build_sign_string(struct lws *wsi, char *buf, size_t bufsz,
		struct lws_ss_handle *h, struct sigv4 *s)
{
	char hash[65], *end = &buf[bufsz - 1], *start;
	struct lws_genhash_ctx hash_ctx;
	uint8_t hash_bin[32];
	int i, ret = 0;

	start = buf;

	if ((ret = lws_genhash_init(&hash_ctx, LWS_GENHASH_TYPE_SHA256))) {
		lws_genhash_destroy(&hash_ctx, NULL);
		lwsl_err("%s genhash init err %d \n", __func__, ret);
		return -1;
	}
	/*
	 * hash canonical_request
	 */

	if (hash_update_str(&hash_ctx, h->policy->u.http.method) ||
			hash_update_str(&hash_ctx, "\n"))
		return -1;
	if (hash_update_str(&hash_ctx, lws_hdr_simple_ptr(wsi, _WSI_TOKEN_CLIENT_URI)) ||
			hash_update_str(&hash_ctx, "\n"))
		return -1;

	/* TODO, append query string */
	if (hash_update_str(&hash_ctx, "\n"))
		return -1;

	for (i = 0; i < s->hnum; i++) {
		if (hash_update_str(&hash_ctx, s->headers[i].name) ||
		    hash_update_str(&hash_ctx, s->headers[i].value) ||
		    hash_update_str(&hash_ctx, "\n"))
		return -1;

	}
	if (hash_update_str(&hash_ctx, "\n"))
		return -1;

	for (i = 0; i < s->hnum-1; i++) {
		if (hash_update_bite_str(&hash_ctx, s->headers[i].name) ||
		    hash_update_str(&hash_ctx, ";"))
			return -1;
	}
	if (hash_update_bite_str(&hash_ctx, s->headers[i].name) ||
	    hash_update_str(&hash_ctx, "\n") ||
	    hash_update_str(&hash_ctx, s->payload_hash))
		return -1;

	if ((ret = lws_genhash_destroy(&hash_ctx, hash_bin))) {
		lws_genhash_destroy(&hash_ctx, NULL);
		lwsl_err("%s lws_genhash error \n", __func__);
		return -1;
	}

	bin2hex(hash_bin, sizeof(hash_bin), hash);
	/*
	 * build sign string like the following
	 *
	 * "AWS4-HMAC-SHA256" + "\n" +
	 * timeStampISO8601Format + "\n" +
	 * date.Format(<YYYYMMDD>) + "/" + <region> + "/" + <service> + "/aws4_request" + "\n" +
	 * Hex(SHA256Hash(<CanonicalRequest>))
	 */
	buf = start;

	buf += lws_snprintf(buf, lws_ptr_diff_size_t(end, buf), "%s\n",
							"AWS4-HMAC-SHA256");
	buf += lws_snprintf(buf, lws_ptr_diff_size_t(end, buf), "%s\n",
							s->timestamp);
	buf += lws_snprintf(buf, lws_ptr_diff_size_t(end, buf), "%s/%s/%s/%s\n",
				s->ymd, s->region, s->service, "aws4_request");

	buf += lws_snprintf(buf, lws_ptr_diff_size_t(end, buf), "%s", hash);
	*buf++ = '\0';

	assert(buf <= start + bufsz);

	return 0;
}

/*
 * DateKey              = HMAC-SHA256("AWS4"+"<SecretAccessKey>", "<YYYYMMDD>")
 * DateRegionKey        = HMAC-SHA256(<DateKey>, "<aws-region>")
 * DateRegionServiceKey = HMAC-SHA256(<DateRegionKey>, "<aws-service>")
 * SigningKey           = HMAC-SHA256(<DateRegionServiceKey>, "aws4_request")
 */
static int
calc_signing_key(struct lws *wsi, struct lws_ss_handle *h,
		struct sigv4 *s, uint8_t *sign_key)
{
	uint8_t key[128], date_key[32], and_region_key[32],
		and_service_key[32], *kb;
	lws_system_blob_t *ab;
	size_t keylen;
	int n;

	ab = lws_system_get_blob(wsi->a.context,
				 blob_idx[h->policy->auth->blob_index],
				 LWS_SS_SIGV4_KEY);
	if (!ab)
		return -1;

	kb = key;

	*kb++ = 'A';
	*kb++ = 'W';
	*kb++ = 'S';
	*kb++ = '4';

	keylen = sizeof(key) - 4;
	if (lws_system_blob_get_size(ab) > keylen - 1)
		return -1;

	n = lws_system_blob_get(ab, kb, &keylen, 0);
	if (n < 0)
		return -1;

	kb[keylen] = '\0';

	hmacsha256((const uint8_t *)key, strlen((const char *)key),
		   (const uint8_t *)s->ymd, strlen(s->ymd), date_key);

	hmacsha256(date_key, sizeof(date_key), (const uint8_t *)s->region,
		   strlen(s->region), and_region_key);

	hmacsha256(and_region_key, sizeof(and_region_key),
		   (const uint8_t *)s->service,
		   strlen(s->service), and_service_key);

	hmacsha256(and_service_key, sizeof(and_service_key),
		   (uint8_t *)"aws4_request",
		   strlen("aws4_request"), sign_key);

	return 0;
}

/* Sample auth string:
 *
 * 'Authorization: AWS4-HMAC-SHA256 Credential=AKIAVHWASOFE7TJ7ZUQY/20200731/us-west-2/s3/aws4_request,
* SignedHeaders=host;x-amz-content-sha256;x-amz-date, \
* Signature=ad9fb75ff3b46c7990e3e8f090abfdd6c01fd67761a517111694377e20698377'
*/
static int
build_auth_string(struct lws *wsi, char * buf, size_t bufsz,
		struct lws_ss_handle *h, struct sigv4 *s,
		uint8_t *signature_bin)
{
	char *start = buf, *end = &buf[bufsz - 1];
	char *c;
	lws_system_blob_t *ab;
	size_t keyidlen = 128; // max keyid len is 128
	int n;

	buf += lws_snprintf(buf, lws_ptr_diff_size_t(end, buf), "%s",
			    "AWS4-HMAC-SHA256 ");

	ab = lws_system_get_blob(wsi->a.context,
				 blob_idx[h->policy->auth->blob_index],
				 LWS_SS_SIGV4_KEYID);
	if (!ab)
		return -1;

	buf += lws_snprintf(buf, lws_ptr_diff_size_t(end, buf), "%s",
							"Credential=");
	n = lws_system_blob_get(ab,(uint8_t *)buf, &keyidlen, 0);
	if (n < 0)
		return -1;
	buf += keyidlen;

	buf += lws_snprintf(buf, lws_ptr_diff_size_t(end, buf), "/%s/%s/%s/%s, ",
				s->ymd, s->region, s->service, "aws4_request");
	buf += lws_snprintf(buf, lws_ptr_diff_size_t(end, buf), "%s",
							"SignedHeaders=");
	for (n = 0; n < s->hnum; n++) {
		buf += lws_snprintf(buf, lws_ptr_diff_size_t(end, buf),
					"%s",s->headers[n].name);
		buf--; /* remove ':' */
		*buf++ = ';';
	}
	c = buf - 1;
	*c = ','; /* overwrite ';' back to ',' */

	buf += lws_snprintf(buf, lws_ptr_diff_size_t(end, buf),
			    "%s", " Signature=");
	bin2hex(signature_bin, 32, buf);

	assert(buf+65 <= start + bufsz);

	lwsl_debug("%s %s\n", __func__, start);

	return 0;

}

int
lws_ss_apply_sigv4(struct lws *wsi, struct lws_ss_handle *h,
		     unsigned char **p, unsigned char *end)
{
	uint8_t buf[512], sign_key[32], signature_bin[32], *bp;
	struct sigv4 s;

	memset(&s, 0, sizeof(s));

	bp = buf;

	init_sigv4(wsi, h, &s);
	if (!s.timestamp || !s.payload_hash) {
		lwsl_err("%s missing headers\n", __func__);
		return -1;
	}

	if (build_sign_string(wsi, (char *)bp, sizeof(buf), h, &s))
		return -1;

	if (calc_signing_key(wsi, h, &s, sign_key))
		return -1;

	hmacsha256(sign_key, sizeof(sign_key), (const uint8_t *)buf,
			      strlen((const char *)buf), signature_bin);

	bp = buf; /* reuse for auth_str */
	if (build_auth_string(wsi, (char *)bp, sizeof(buf), h, &s,
				signature_bin))
		return -1;

	if (lws_add_http_header_by_name(wsi,
					(const uint8_t *)"Authorization:", buf,
					(int)strlen((const char*)buf), p, end))
		return -1;

	return 0;
}

int
lws_ss_sigv4_set_aws_key(struct lws_context* context, uint8_t idx,
		                const char * keyid, const char * key)
{
	const char * s[] = { keyid, key };
	lws_system_blob_t *ab;
	int i;

	if (idx > LWS_ARRAY_SIZE(blob_idx))
		return -1;

	for (i = 0; i < LWS_SS_SIGV4_BLOB_SLOTS; i++) {
		ab = lws_system_get_blob(context, blob_idx[idx], i);
		if (!ab)
			return -1;

		lws_system_blob_heap_empty(ab);

		if (lws_system_blob_heap_append(ab, (const uint8_t *)s[i],
						strlen(s[i]))) {
			lwsl_err("%s: can't store %d \n", __func__, i);

			return -1;
		}
	}

	return 0;
}

#if defined(__linux__) || defined(__APPLE__) || defined(WIN32) || \
	defined(__FreeBSD__) || defined(__NetBSD__) || defined(__ANDROID__) || \
	defined(__sun) || defined(__OpenBSD__)

/* ie, if we have filesystem ops */

int
lws_aws_filesystem_credentials_helper(const char *path, const char *kid,
				      const char *ak, char **aws_keyid,
				      char **aws_key)
{
	char *str = NULL, *val = NULL, *line = NULL, sth[128];
	size_t len = sizeof(sth);
	const char *home = "";
	int i, poff = 0;
	ssize_t rd;
	FILE *fp;

	*aws_keyid = *aws_key = NULL;

	if (path[0] == '~') {
		home = getenv("HOME");
		if (home && strlen(home) > sizeof(sth) - 1) /* coverity */
			return -1;
		else {
			if (!home)
				home = "";

			poff = 1;
		}
	}
	lws_snprintf(sth, sizeof(sth), "%s%s", home, path + poff);

	fp = fopen(sth, "r");
	if (!fp) {
		lwsl_err("%s can't open '%s'\n", __func__, sth);

		return -1;
	}

	while ((rd = getline(&line, &len, fp)) != -1) {
		for (i = 0; i < 2; i++) {
			size_t slen;

			if (strncmp(line, i ? kid : ak, strlen(i ? kid : ak)))
				continue;

			str = strchr(line, '=');
			if (!str)
				continue;

			str++;

			/* only read the first key for each */
			if (*(i ? aws_keyid : aws_key))
				continue;

			/*
			 * Trim whitespace from the start and end
			 */

			slen = (size_t)(rd - lws_ptr_diff(str, line));

			while (slen && *str == ' ') {
				str++;
				slen--;
			}

			while (slen && (str[slen - 1] == '\r' ||
					str[slen - 1] == '\n' ||
					str[slen - 1] == ' '))
				slen--;

			val = malloc(slen + 1);
			if (!val)
				goto bail;

			strncpy(val, str, slen);
			val[slen] = '\0';

			*(i ? aws_keyid : aws_key) = val;

		}
	}

bail:
	fclose(fp);

	if (line)
		free(line);

	if (!*aws_keyid || !*aws_key) {
		if (*aws_keyid) {
			free(*aws_keyid);
			*aws_keyid = NULL;
		}
		if (*aws_key) {
			free(*aws_key);
			*aws_key = NULL;
		}
		lwsl_err("%s can't find aws credentials! \
				please check %s\n", __func__, path);
		return -1;
	}

	lwsl_info("%s: '%s' '%s'\n", __func__, *aws_keyid, *aws_key);

	return 0;
}
#endif
