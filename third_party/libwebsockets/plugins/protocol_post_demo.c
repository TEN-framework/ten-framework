/*
 * ws protocol handler plugin for "POST demo"
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

struct per_session_data__post_demo {
	struct lws_spa *spa;
	char result[LWS_PRE + LWS_RECOMMENDED_MIN_HEADER_SPACE];
	char filename[64];
	long file_length;
#if !defined(LWS_WITH_ESP32)
	lws_filefd_type fd;
#endif
	uint8_t completed:1;
	uint8_t sent_headers:1;
	uint8_t sent_body:1;
};

static const char * const param_names[] = {
	"text",
	"send",
	"file",
	"upload",
};

enum enum_param_names {
	EPN_TEXT,
	EPN_SEND,
	EPN_FILE,
	EPN_UPLOAD,
};

static int
file_upload_cb(void *data, const char *name, const char *filename,
	       char *buf, int len, enum lws_spa_fileupload_states state)
{
	struct per_session_data__post_demo *pss =
			(struct per_session_data__post_demo *)data;
#if !defined(LWS_WITH_ESP32)
	int n;

	(void)n;
#endif

	switch (state) {
	case LWS_UFS_OPEN:
		lws_strncpy(pss->filename, filename, sizeof(pss->filename));
		/* we get the original filename in @filename arg, but for
		 * simple demo use a fixed name so we don't have to deal with
		 * attacks  */
#if !defined(LWS_WITH_ESP32)
		pss->fd = (lws_filefd_type)(lws_intptr_t)lws_open("/tmp/post-file",
			       O_CREAT | O_TRUNC | O_RDWR, 0600);
#endif
		break;
	case LWS_UFS_FINAL_CONTENT:
	case LWS_UFS_CONTENT:
		if (len) {
			pss->file_length += len;

			/* if the file length is too big, drop it */
			if (pss->file_length > 100000)
				return 1;

#if !defined(LWS_WITH_ESP32)
			n = (int)write((int)(lws_intptr_t)pss->fd, buf, (unsigned int)len);
			lwsl_info("%s: write %d says %d\n", __func__, len, n);
#else
			lwsl_notice("%s: Received chunk size %d\n", __func__, len);
#endif
		}
		if (state == LWS_UFS_CONTENT)
			break;
#if !defined(LWS_WITH_ESP32)
		close((int)(lws_intptr_t)pss->fd);
		pss->fd = LWS_INVALID_FILE;
#endif
		break;
	case LWS_UFS_CLOSE:
		break;
	}

	return 0;
}

/*
 * returns length in bytes
 */

static int
format_result(struct per_session_data__post_demo *pss)
{
	unsigned char *p, *start, *end;
	int n;

	p = (unsigned char *)pss->result + LWS_PRE;
	start = p;
	end = p + sizeof(pss->result) - LWS_PRE - 1;

	if (!pss->spa) {
		p += lws_snprintf((char *)p, lws_ptr_diff_size_t(end, p),
				  "pss->spa already NULL");
		goto bail;
	}

	p += lws_snprintf((char *)p, lws_ptr_diff_size_t(end, p),
			"<!DOCTYPE html><html lang=\"en\"><head>"
			"<meta charset=utf-8 http-equiv=\"Content-Language\" "
			"content=\"en\"/>"
	  "<title>LWS Server Status</title>"
	  "</head><body><h1>Form results (after urldecoding)</h1>"
	  "<table><tr><td>Name</td><td>Length</td><td>Value</td></tr>");

	for (n = 0; n < (int)LWS_ARRAY_SIZE(param_names); n++) {
		if (!lws_spa_get_string(pss->spa, n))
			p += lws_snprintf((char *)p, lws_ptr_diff_size_t(end, p),
			    "<tr><td><b>%s</b></td><td>0"
			    "</td><td>NULL</td></tr>",
			    param_names[n]);
		else
			p += lws_snprintf((char *)p, lws_ptr_diff_size_t(end, p),
			    "<tr><td><b>%s</b></td><td>%d"
			    "</td><td>%s</td></tr>",
			    param_names[n],
			    lws_spa_get_length(pss->spa, n),
			    lws_spa_get_string(pss->spa, n));
	}

	p += lws_snprintf((char *)p, lws_ptr_diff_size_t(end, p),
			"</table><br><b>filename:</b> %s, "
			"<b>length</b> %ld",
			pss->filename, pss->file_length);

	p += lws_snprintf((char *)p, lws_ptr_diff_size_t(end, p), "</body></html>");

bail:
	return (int)lws_ptr_diff(p, start);
}

static int
callback_post_demo(struct lws *wsi, enum lws_callback_reasons reason,
		   void *user, void *in, size_t len)
{
	struct per_session_data__post_demo *pss =
			(struct per_session_data__post_demo *)user;
	unsigned char *p, *start, *end;
	int n;

	switch (reason) {
	case LWS_CALLBACK_HTTP_BODY:
		/* create the POST argument parser if not already existing */
		if (!pss->spa) {
			pss->spa = lws_spa_create(wsi, param_names,
					LWS_ARRAY_SIZE(param_names), 1024,
					file_upload_cb, pss);
			if (!pss->spa)
				return -1;

			pss->filename[0] = '\0';
			pss->file_length = 0;
		}

		/* let it parse the POST data */
		if (lws_spa_process(pss->spa, in, (int)len))
			return -1;
		break;

	case LWS_CALLBACK_HTTP_BODY_COMPLETION:
		lwsl_debug("LWS_CALLBACK_HTTP_BODY_COMPLETION: %s\n", lws_wsi_tag(wsi));
		/* call to inform no more payload data coming */
		lws_spa_finalize(pss->spa);

		pss->completed = 1;
		lws_callback_on_writable(wsi);
		break;

	case LWS_CALLBACK_HTTP_WRITEABLE:
		if (!pss->completed)
			break;

		p = (unsigned char *)pss->result + LWS_PRE;
		start = p;
		end = p + sizeof(pss->result) - LWS_PRE - 1;

		if (!pss->sent_headers) {
			n = format_result(pss);

			if (lws_add_http_header_status(wsi, HTTP_STATUS_OK,
						       &p, end))
				goto bail;

			if (lws_add_http_header_by_token(wsi,
					WSI_TOKEN_HTTP_CONTENT_TYPE,
					(unsigned char *)"text/html", 9,
					&p, end))
				goto bail;
			if (lws_add_http_header_content_length(wsi, (unsigned int)n, &p, end))
				goto bail;
			if (lws_finalize_http_header(wsi, &p, end))
				goto bail;

			/* first send the headers ... */
			n = lws_write(wsi, start, lws_ptr_diff_size_t(p, start),
				      LWS_WRITE_HTTP_HEADERS);
			if (n < 0)
				goto bail;

			pss->sent_headers = 1;
			lws_callback_on_writable(wsi);
			break;
		}

		if (!pss->sent_body) {
			n = format_result(pss);

			n = lws_write(wsi, (unsigned char *)start, (unsigned int)n,
				      LWS_WRITE_HTTP_FINAL);

			pss->sent_body = 1;
			if (n < 0)
				return 1;
			goto try_to_reuse;
		}
		break;

	case LWS_CALLBACK_HTTP_DROP_PROTOCOL:
		/* called when our wsi user_space is going to be destroyed */
		if (pss->spa) {
			lws_spa_destroy(pss->spa);
			pss->spa = NULL;
		}
		break;

	default:
		break;
	}

	return 0;

bail:

	return 1;

try_to_reuse:
	if (lws_http_transaction_completed(wsi))
		return -1;

	return 0;
}

#define LWS_PLUGIN_PROTOCOL_POST_DEMO \
	{ \
		"protocol-post-demo", \
		callback_post_demo, \
		sizeof(struct per_session_data__post_demo), \
		1024, \
		0, NULL, 0 \
	}

#if !defined (LWS_PLUGIN_STATIC)

LWS_VISIBLE const struct lws_protocols post_demo_protocols[] = {
	LWS_PLUGIN_PROTOCOL_POST_DEMO
};

LWS_VISIBLE const lws_plugin_protocol_t post_demo = {
	.hdr = {
		"post demo",
		"lws_protocol_plugin",
		LWS_BUILD_HASH,
		LWS_PLUGIN_API_MAGIC
	},

	.protocols = post_demo_protocols,
	.count_protocols = LWS_ARRAY_SIZE(post_demo_protocols),
	.extensions = NULL,
	.count_extensions = 0,
};

#endif
