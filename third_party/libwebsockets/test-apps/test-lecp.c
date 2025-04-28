/*
 * lejp test app
 *
 * Written in 2010-2019 by Andy Green <andy@warmcat.com>
 *
 * This file is made available under the Creative Commons CC0 1.0
 * Universal Public Domain Dedication.
 *
 * This demonstrates a minimal http server that performs a form GET with a couple
 * of parameters.  It dumps the parameters to the console log and redirects
 * to another page.
 */

#include <libwebsockets.h>
#include <string.h>


static const char * const reason_names[] = {
	"LECPCB_CONSTRUCTED",
	"LECPCB_DESTRUCTED",
	"LECPCB_START",
	"LECPCB_COMPLETE",
	"LECPCB_FAILED",
	"LECPCB_PAIR_NAME",
	"LECPCB_VAL_TRUE",
	"LECPCB_VAL_FALSE",
	"LECPCB_VAL_NULL",
	"LECPCB_VAL_NUM_INT",
	"LECPCB_VAL_RESERVED", /* float in lejp */
	"LECPCB_VAL_STR_START",
	"LECPCB_VAL_STR_CHUNK",
	"LECPCB_VAL_STR_END",
	"LECPCB_ARRAY_START",
	"LECPCB_ARRAY_END",
	"LECPCB_OBJECT_START",
	"LECPCB_OBJECT_END",
	"LECPCB_TAG_START",
	"LECPCB_TAG_END",
	"LECPCB_VAL_NUM_UINT",
	"LECPCB_VAL_UNDEFINED",
	"LECPCB_VAL_FLOAT16",
	"LECPCB_VAL_FLOAT32",
	"LECPCB_VAL_FLOAT64",
	"LECPCB_VAL_SIMPLE",
	"LECPCB_VAL_BLOB_START",
	"LECPCB_VAL_BLOB_CHUNK",
	"LECPCB_VAL_BLOB_END",
};

static const char * const tok[] = {
	"dummy___"
};

static signed char
cb(struct lecp_ctx *ctx, char reason)
{
	char buf[1024], *p = buf, *end = &buf[sizeof(buf)];
	int n;

	for (n = 0; n < ctx->sp; n++)
		*p++ = ' ';
	*p = '\0';

	lwsl_notice("%s%s: path %s match %d statckp %d\r\n", buf,
			reason_names[(unsigned int)(reason) &
			             (LEJP_FLAG_CB_IS_VALUE - 1)], ctx->path,
			ctx->path_match, ctx->pst[ctx->pst_sp].ppos);

	if (reason & LECP_FLAG_CB_IS_VALUE) {

		switch (reason) {
		case LECPCB_VAL_NUM_UINT:
			p += lws_snprintf(p, lws_ptr_diff_size_t(end, p),
					  "   value %llu ",
					  (unsigned long long)ctx->item.u.u64);
			break;
		case LECPCB_VAL_STR_START:
		case LECPCB_VAL_STR_CHUNK:
		case LECPCB_VAL_STR_END:
			p += lws_snprintf(p, lws_ptr_diff_size_t(end, p),
					  "   value '%s' ", ctx->buf);
			break;

		case LECPCB_VAL_BLOB_START:
		case LECPCB_VAL_BLOB_CHUNK:
		case LECPCB_VAL_BLOB_END:
			if (ctx->npos)
				lwsl_hexdump_notice(ctx->buf, (size_t)ctx->npos);
			break;

		case LECPCB_VAL_NUM_INT:
			p += lws_snprintf(p, lws_ptr_diff_size_t(end, p),
					  "   value %lld ",
					  (long long)ctx->item.u.i64);
			break;
		case LECPCB_VAL_FLOAT16:
		case LECPCB_VAL_FLOAT32:
		case LECPCB_VAL_FLOAT64:
			break;

		case LECPCB_VAL_SIMPLE:
			p += lws_snprintf(p, lws_ptr_diff_size_t(end, p),
					  "   simple %llu ",
					  (unsigned long long)ctx->item.u.u64);
			break;
		}
		if (ctx->ipos) {
			int n;

			p += lws_snprintf(p, lws_ptr_diff_size_t(end, p), "(array indexes: ");
			for (n = 0; n < ctx->ipos; n++)
				p += lws_snprintf(p, lws_ptr_diff_size_t(end, p), "%d ", ctx->i[n]);
			p += lws_snprintf(p, lws_ptr_diff_size_t(end, p), ") ");
		}

		lwsl_notice("%s \r\n", buf);

		(void)reason_names; /* NO_LOGS... */
		return 0;
	}

	switch (reason) {
	case LECPCB_COMPLETE:
		lwsl_notice("%sParsing Completed (LEJPCB_COMPLETE)\n", buf);
		break;
	case LECPCB_PAIR_NAME:
		lwsl_notice("%spath: '%s' (LEJPCB_PAIR_NAME)\n", buf, ctx->path);
		break;
	case LECPCB_TAG_START:
		lwsl_notice("LECPCB_TAG_START: %llu\r\n", (unsigned long long)ctx->item.u.u64);
		return 0;
	}

	return 0;
}

int
main(int argc, char *argv[])
{
	int fd, n = 1, ret = 1, m = 0;
	struct lecp_ctx ctx;
	char buf[128];

	lws_set_log_level(7, NULL);

	lwsl_notice("libwebsockets-test-lecp  (C) 2017 - 2021 andy@warmcat.com\n");
	lwsl_notice("  usage: cat my.cbor | libwebsockets-test-lecp\n\n");

	lecp_construct(&ctx, cb, NULL, tok, LWS_ARRAY_SIZE(tok));

	fd = 0;

	while (n > 0) {
		n = (int)read(fd, buf, sizeof(buf));
		if (n <= 0)
			continue;

		m = lecp_parse(&ctx, (uint8_t *)buf, (size_t)n);
		if (m < 0 && m != LEJP_CONTINUE) {
			lwsl_err("parse failed %d\n", m);
			goto bail;
		}
	}
	lwsl_notice("okay (%d)\n", m);
	ret = 0;
bail:
	lecp_destruct(&ctx);

	return ret;
}
