/*
 * ws protocol handler plugin for "fulltext demo"
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

#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifdef WIN32
#include <io.h>
#endif
#include <stdio.h>

struct vhd_fts_demo {
	const char *indexpath;
};

struct pss_fts_demo {
	struct lwsac *result;
	struct lws_fts_result_autocomplete *ac;
	struct lws_fts_result_filepath *fp;

	uint32_t *li;
	int done;

	uint8_t first:1;
	uint8_t ac_done:1;

	uint8_t fp_init_done:1;
};

static int
callback_fts(struct lws *wsi, enum lws_callback_reasons reason, void *user,
	     void *in, size_t len)
{
	struct vhd_fts_demo *vhd = (struct vhd_fts_demo *)
		lws_protocol_vh_priv_get(lws_get_vhost(wsi),
					 lws_get_protocol(wsi));
	struct pss_fts_demo *pss = (struct pss_fts_demo *)user;
	uint8_t buf[LWS_PRE + 2048], *start = &buf[LWS_PRE], *p = start,
		*end = &buf[sizeof(buf) - 1];
	struct lws_fts_search_params params;
	const char *ccp = (const char *)in;
	struct lws_fts_result *result;
	struct lws_fts_file *jtf;
	int n;

	switch (reason) {

	case LWS_CALLBACK_PROTOCOL_INIT:
		vhd = lws_protocol_vh_priv_zalloc(lws_get_vhost(wsi),
			     lws_get_protocol(wsi),sizeof(struct vhd_fts_demo));
		if (!vhd)
			return 0;
		if (lws_pvo_get_str(in, "indexpath",
				    (const char **)&vhd->indexpath))
			return 1;

		return 0;

	case LWS_CALLBACK_HTTP:

		pss->first = 1;
		pss->ac_done = 0;

		/*
		 * we have a "subdirectory" selecting the task
		 *
		 * /a/ = autocomplete
		 * /r/ = results
		 */

		if (strncmp(ccp, "/a/", 3) && strncmp(ccp, "/r/", 3))
			goto reply_404;

		memset(&params, 0, sizeof(params));

		params.needle = ccp + 3;
		if (*(ccp + 1) == 'a')
			params.flags = LWSFTS_F_QUERY_AUTOCOMPLETE;
		if (*(ccp + 1) == 'r')
			params.flags = LWSFTS_F_QUERY_FILES |
				       LWSFTS_F_QUERY_FILE_LINES |
				       LWSFTS_F_QUERY_QUOTE_LINE;
		params.max_autocomplete = 10;
		params.max_files = 10;

		jtf = lws_fts_open(vhd->indexpath);
		if (!jtf) {
			lwsl_err("unable to open %s\n", vhd->indexpath);
			/* we'll inform the client in the JSON */
			goto reply_200;
		}

		result = lws_fts_search(jtf, &params);
		lws_fts_close(jtf);
		if (result) {
			pss->result = params.results_head;
			pss->ac = result->autocomplete_head;
			pss->fp = result->filepath_head;
		}
		/* NULL result will be told in the json as "indexed": 0 */

reply_200:
		if (lws_add_http_common_headers(wsi, HTTP_STATUS_OK,
						"text/html",
					LWS_ILLEGAL_HTTP_CONTENT_LEN, &p, end))
			return 1;

		if (lws_finalize_write_http_header(wsi, start, &p, end))
			return 1;

		lws_callback_on_writable(wsi);
		return 0;

reply_404:
		if (lws_add_http_common_headers(wsi, HTTP_STATUS_NOT_FOUND,
						"text/html",
					LWS_ILLEGAL_HTTP_CONTENT_LEN, &p, end))
			return 1;

		if (lws_finalize_write_http_header(wsi, start, &p, end))
			return 1;
		return lws_http_transaction_completed(wsi);

	case LWS_CALLBACK_CLOSED_HTTP:
		if (pss && pss->result)
			lwsac_free(&pss->result);
		break;

	case LWS_CALLBACK_HTTP_WRITEABLE:

		if (!pss)
			break;

		n = LWS_WRITE_HTTP;
		if (pss->first)
			p += lws_snprintf((char *)p, lws_ptr_diff_size_t(end, p),
				"{\"indexed\": %d, \"ac\": [", !!pss->result);

		while (pss->ac && lws_ptr_diff(end, p) > 256) {
			p += lws_snprintf((char *)p, lws_ptr_diff_size_t(end, p),
				"%c{\"ac\": \"%s\",\"matches\": %d,"
				"\"agg\": %d, \"elided\": %d}",
				pss->first ? ' ' : ',', (char *)(pss->ac + 1),
				pss->ac->instances, pss->ac->agg_instances,
				pss->ac->elided);

			pss->first = 0;
			pss->ac = pss->ac->next;
		}

		if (!pss->ac_done && !pss->ac && pss->fp) {
			pss->ac_done = 1;

			p += lws_snprintf((char *)p, lws_ptr_diff_size_t(end, p),
					  "], \"fp\": [");
		}

		while (pss->fp && lws_ptr_diff_size_t(end, p) > 256) {
			if (!pss->fp_init_done) {
				p += lws_snprintf((char *)p,
						lws_ptr_diff_size_t(end, p),
					"%c{\"path\": \"%s\",\"matches\": %d,"
					"\"origlines\": %d,"
					"\"hits\": [", pss->first ? ' ' : ',',
					((char *)(pss->fp + 1)) +
						pss->fp->matches_length,
					pss->fp->matches,
					pss->fp->lines_in_file);

				pss->li = ((uint32_t *)(pss->fp + 1));
				pss->done = 0;
				pss->fp_init_done = 1;
				pss->first = 0;
			} else {
				while (pss->done < pss->fp->matches &&
				       lws_ptr_diff(end, p) > 256) {

					p += lws_snprintf((char *)p,
							lws_ptr_diff_size_t(end, p),
						"%c\n{\"l\":%d,\"o\":%d,"
						"\"s\":\"%s\"}",
						!pss->done ? ' ' : ',',
						pss->li[0], pss->li[1],
						*((const char **)&pss->li[2]));
					pss->li += 2 + (sizeof(const char *) /
							sizeof(uint32_t));
					pss->done++;
				}

				if (pss->done == pss->fp->matches) {
					*p++ = ']';
					pss->fp_init_done = 0;
					pss->fp = pss->fp->next;
					if (!pss->fp)
						*p++ = '}';
				}
			}
		}

		if (!pss->ac && !pss->fp) {
			n = LWS_WRITE_HTTP_FINAL;
			p += lws_snprintf((char *)p, lws_ptr_diff_size_t(end, p),
						"]}");
		}

		if (lws_write(wsi, (uint8_t *)start,
				lws_ptr_diff_size_t(p, start), (enum lws_write_protocol)n) !=
					      lws_ptr_diff(p, start))
			return 1;

		if (n == LWS_WRITE_HTTP_FINAL) {
			if (pss->result)
				lwsac_free(&pss->result);
			if (lws_http_transaction_completed(wsi))
				return -1;
		} else
			lws_callback_on_writable(wsi);

		return 0;

	default:
		break;
	}

	return lws_callback_http_dummy(wsi, reason, user, in, len);
}


#define LWS_PLUGIN_PROTOCOL_FULLTEXT_DEMO \
	{ \
		"lws-test-fts", \
		callback_fts, \
		sizeof(struct pss_fts_demo), \
		0, \
		0, NULL, 0 \
	}

#if !defined (LWS_PLUGIN_STATIC)

LWS_VISIBLE const struct lws_protocols fulltext_demo_protocols[] = {
	LWS_PLUGIN_PROTOCOL_FULLTEXT_DEMO
};

LWS_VISIBLE const lws_plugin_protocol_t fulltext_demo = {
	.hdr = {
		"fulltext demo",
		"lws_protocol_plugin",
		LWS_BUILD_HASH,
		LWS_PLUGIN_API_MAGIC
	},

	.protocols = fulltext_demo_protocols,
	.count_protocols = LWS_ARRAY_SIZE(fulltext_demo_protocols),
	.extensions = NULL,
	.count_extensions = 0,
};

#endif
