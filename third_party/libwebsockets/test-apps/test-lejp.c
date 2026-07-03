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
	"LEJPCB_CONSTRUCTED",
	"LEJPCB_DESTRUCTED",
	"LEJPCB_START",
	"LEJPCB_COMPLETE",
	"LEJPCB_FAILED",
	"LEJPCB_PAIR_NAME",
	"LEJPCB_VAL_TRUE",
	"LEJPCB_VAL_FALSE",
	"LEJPCB_VAL_NULL",
	"LEJPCB_VAL_NUM_INT",
	"LEJPCB_VAL_NUM_FLOAT",
	"LEJPCB_VAL_STR_START",
	"LEJPCB_VAL_STR_CHUNK",
	"LEJPCB_VAL_STR_END",
	"LEJPCB_ARRAY_START",
	"LEJPCB_ARRAY_END",
	"LEJPCB_OBJECT_START",
	"LEJPCB_OBJECT_END",
	"LEJPCB_OBJECT_END_PRE",
};

static const char * const tok[] = {
	"dummy___"
};

static signed char
cb(struct lejp_ctx *ctx, char reason)
{
	char buf[1024], *p = buf, *end = &buf[sizeof(buf)];
	int n;

	for (n = 0; n < ctx->sp; n++)
		*p++ = ' ';
	*p = '\0';

	if (reason & LEJP_FLAG_CB_IS_VALUE) {
		p += lws_snprintf(p, lws_ptr_diff_size_t(end, p), "   value '%s' ", ctx->buf);
		if (ctx->ipos) {
			int n;

			p += lws_snprintf(p, lws_ptr_diff_size_t(end, p), "(array indexes: ");
			for (n = 0; n < ctx->ipos; n++)
				p += lws_snprintf(p, lws_ptr_diff_size_t(end, p), "%d ", ctx->i[n]);
			p += lws_snprintf(p, lws_ptr_diff_size_t(end, p), ") ");
		}
		lwsl_notice("%s (%s)\r\n", buf,
		       reason_names[(unsigned int)
			(reason) & (LEJP_FLAG_CB_IS_VALUE - 1)]);

		(void)reason_names; /* NO_LOGS... */
		return 0;
	}

	switch (reason) {
	case LEJPCB_COMPLETE:
		lwsl_notice("%sParsing Completed (LEJPCB_COMPLETE)\n", buf);
		break;
	case LEJPCB_PAIR_NAME:
		lwsl_notice("%spath: '%s' (LEJPCB_PAIR_NAME)\n", buf, ctx->path);
		break;
	}

	lwsl_notice("%s%s: path %s match %d statckp %d\r\n", buf, reason_names[(unsigned int)
		(reason) & (LEJP_FLAG_CB_IS_VALUE - 1)], ctx->path,
		ctx->path_match, ctx->pst[ctx->pst_sp].ppos);

	return 0;
}

int
main(int argc, char *argv[])
{
	int fd, n = 1, ret = 1, m = 0;
	struct lejp_ctx ctx;
	char buf[128];

	lws_set_log_level(7, NULL);

	lwsl_notice("libwebsockets-test-lejp  (C) 2017 - 2018 andy@warmcat.com\n");
	lwsl_notice("  usage: cat my.json | libwebsockets-test-lejp\n\n");

	lejp_construct(&ctx, cb, NULL, tok, LWS_ARRAY_SIZE(tok));

	fd = 0;

	while (n > 0) {
		n = (int)read(fd, buf, sizeof(buf));
		if (n <= 0)
			continue;

		m = lejp_parse(&ctx, (uint8_t *)buf, n);
		if (m < 0 && m != LEJP_CONTINUE) {
			lwsl_err("parse failed %d\n", m);
			goto bail;
		}
	}
	lwsl_notice("okay (%d)\n", m);
	ret = 0;
bail:
	lejp_destruct(&ctx);

	return ret;
}
