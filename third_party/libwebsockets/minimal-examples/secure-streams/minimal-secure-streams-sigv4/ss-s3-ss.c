/*
 * S3 Put Object via Secure Streams minimal siv4 example
 *
 * Written in 2010-2020 by Andy Green <andy@warmcat.com>
 *			   Amit Pachore <apachor@amazon.com>
 *                         securestreams-dev@amazon.com
 *
 * This file is made available under the Creative Commons CC0 1.0
 * Universal Public Domain Dedication.
 */

#include <libwebsockets.h>
#include <assert.h>
#include "ss-s3-put.h"

extern int interrupted, bad;

static lws_ss_state_return_t
ss_s3_rx(void *userobj, const uint8_t *buf, size_t len, int flags)
{
	// ss_s3_put_t *m = (ss_s3_put_t *)userobj;

	if (flags & LWSSS_FLAG_EOM) {
		bad = 0;
		interrupted = 1; /* this example wants to exit after rx */
		return LWSSSSRET_DESTROY_ME;
	}

	lwsl_user("%s: len %d, flags: %d\n", __func__, (int)len, flags);
	lwsl_hexdump_err(buf, len);

	return LWSSSSRET_OK;
}

static lws_ss_state_return_t
ss_s3_tx(void *userobj, lws_ss_tx_ordinal_t ord, uint8_t *buf, size_t *len,
	 int *flags)
{
	ss_s3_put_t *m = (ss_s3_put_t *)userobj;

	if (!m->pos)
		*flags |= LWSSS_FLAG_SOM;

	lwsl_user("%s: Send... total: %ld, pos: %ld\n", __func__,
		  (long)m->total, (long)m->pos);

	if (*len > m->total - m->pos)
		*len = m->total - m->pos;

	if (!*len)
		return LWSSSSRET_TX_DONT_SEND;

	memcpy(buf, m->buf + m->pos, *len);
	m->pos += *len;

	if (m->pos == m->total) {
		*flags |= LWSSS_FLAG_EOM;
		// m->pos = 0; /* we only want to send once */
	} else
		return lws_ss_request_tx(m->ss);

	return LWSSSSRET_OK;
}

static const char *awsService	= "s3",
		  *awsRegion	= "us-west-2",
		  *s3bucketName = "sstest2020",
#if 1
		  *s3ObjName    = "SSs3upload2.txt";
#else
		  /* test huge string sigv4 hashing works */
		  *s3ObjName	= "SSs3uploadaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa2.txt";
#endif
static char timestamp[32], payload_hash[65];
static uint8_t jpl[1 * 1024];


static void
create_payload(uint8_t *buf, size_t s)
{
	int i;

	for (i = 0; i < (int)s; i++)
		buf[i] = (uint8_t)('a' + i % 16);
}

static void set_time(char *t)
{
	/*20150830T123600Z*/
	time_t ti = time(NULL);
#if defined(LWS_HAVE_GMTIME_R)
	struct tm tmp;
	struct tm *tm = gmtime_r(&ti, &tmp);
#else
	struct tm *tm = gmtime(&ti);
#endif
	assert(tm);
	strftime(t, 20, "%Y%m%dT%H%M%SZ", tm);
}

static void bin2hex(uint8_t *in, size_t len, char *out)
{
	static const char *hex = "0123456789abcdef";
	size_t n;

	for (n = 0; n < len; n++) {
		*out++ = hex[(in[n] >> 4) & 0xf];
		*out++ = hex[in[n] & 15];
	}
	*out = '\0';
}

static void sigv4_sha256hash_payload(uint8_t *payload, size_t len, char *hash)
{
	struct lws_genhash_ctx hash_ctx;
	uint8_t hash_bin[32];

	if (lws_genhash_init(&hash_ctx, LWS_GENHASH_TYPE_SHA256) ||
		/*
		 * If there is no payload, you must provide the hash of an
		 * empty string...
		 */
	    lws_genhash_update(&hash_ctx,
			       payload ? (void *)payload : (void *)"",
			       payload ? len : 0u) ||
	    lws_genhash_destroy(&hash_ctx, hash_bin))
	{

		lws_genhash_destroy(&hash_ctx, NULL);
		lwsl_err("%s lws_genhash failed\n", __func__);

		return;
	}

	bin2hex(hash_bin, 32, hash);
}

static lws_ss_state_return_t
ss_s3_state(void *userobj, void *sh, lws_ss_constate_t state,
                    lws_ss_tx_ordinal_t ack)
{
	ss_s3_put_t *m = (ss_s3_put_t *)userobj;

	lwsl_user("%s: %s %s, ord 0x%x\n", __func__, lws_ss_tag(m->ss),
		  lws_ss_state_name((int)state), (unsigned int)ack);

	switch (state) {
	case LWSSSCS_CREATING:
		create_payload(jpl, sizeof(jpl));
		m->buf = (uint8_t *)jpl;
		m->total = sizeof(jpl);

		sigv4_sha256hash_payload(m->buf, m->total, payload_hash);
		memset(timestamp, 0, sizeof(timestamp));
		set_time(timestamp);

		if (lws_ss_set_metadata(m->ss, "s3bucket",
				    s3bucketName, strlen(s3bucketName)) ||
		   lws_ss_set_metadata(m->ss, "s3Obj",
				    s3ObjName, strlen(s3ObjName)) ||
		   lws_ss_set_metadata(m->ss, "ctype",
				    "text/plain", strlen("text/plain")) ||
		   lws_ss_set_metadata(m->ss, "region",
				    awsRegion, strlen(awsRegion)) ||
		   lws_ss_set_metadata(m->ss, "service",
				    awsService, strlen(awsService)) ||
		   lws_ss_set_metadata(m->ss, "xacl",
				    "bucket-owner-full-control",
				    strlen("bucket-owner-full-control")) ||
		   lws_ss_set_metadata(m->ss, "xcsha256",
				    payload_hash, strlen(payload_hash)) ||
		   lws_ss_set_metadata(m->ss, "xdate",
				    timestamp, strlen(timestamp)))
			return LWSSSSRET_DESTROY_ME;

		return lws_ss_request_tx_len(m->ss, m->total);

	case LWSSSCS_CONNECTED:
		return lws_ss_request_tx(m->ss);

	case LWSSSCS_DISCONNECTED:
		return LWSSSSRET_DESTROY_ME;

	case LWSSSCS_ALL_RETRIES_FAILED:
		/* if we're out of retries, we want to close the app and FAIL */
		bad = 1;
		return LWSSSSRET_DESTROY_ME;

	case LWSSSCS_QOS_ACK_REMOTE:
		bad = 0;
		break;

	case LWSSSCS_QOS_NACK_REMOTE:
		bad = 1;
		break;

	case LWSSSCS_DESTROYING:
		interrupted = 1;
		break;

	default:
		break;
	}

	return 0;
}

const lws_ss_info_t s3_ssi = {
	.handle_offset		 = offsetof(ss_s3_put_t, ss),
	.opaque_user_data_offset = offsetof(ss_s3_put_t, opaque_data),
	.rx			 = ss_s3_rx,
	.tx			 = ss_s3_tx,
	.state			 = ss_s3_state,
	.user_alloc		 = sizeof(ss_s3_put_t),
	.streamtype		 = "s3PutObj"
};
