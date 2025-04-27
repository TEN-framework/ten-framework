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

#if !defined(_GNU_SOURCE)
#define _GNU_SOURCE
#endif

#include "private-lib-core.h"

#if defined(WIN32) || defined(_WIN32)
#else
#include <sys/wait.h>
#endif

static const char *hex = "0123456789ABCDEF";

void
lws_cgi_sul_cb(lws_sorted_usec_list_t *sul);

static int
urlencode(const char *in, int inlen, char *out, int outlen)
{
	char *start = out, *end = out + outlen;

	while (inlen-- && out < end - 4) {
		if ((*in >= 'A' && *in <= 'Z') ||
		    (*in >= 'a' && *in <= 'z') ||
		    (*in >= '0' && *in <= '9') ||
		    *in == '-' ||
		    *in == '_' ||
		    *in == '.' ||
		    *in == '~') {
			*out++ = *in++;
			continue;
		}
		if (*in == ' ') {
			*out++ = '+';
			in++;
			continue;
		}
		*out++ = '%';
		*out++ = hex[(*in) >> 4];
		*out++ = hex[(*in++) & 15];
	}
	*out = '\0';

	if (out >= end - 4)
		return -1;

	return lws_ptr_diff(out, start);
}

static void
lws_cgi_grace(lws_sorted_usec_list_t *sul)
{
	struct lws_cgi *cgi = lws_container_of(sul, struct lws_cgi, sul_grace);

	/* act on the reap cb from earlier */

	if (!cgi->wsi->http.cgi->post_in_expected)
		cgi->wsi->http.cgi->cgi_transaction_over = 1;

	lws_callback_on_writable(cgi->wsi);
}


static void
lws_cgi_reap_cb(void *opaque, lws_usec_t *accounting, siginfo_t *si,
		 int we_killed_him)
{
	struct lws *wsi = (struct lws *)opaque;

	/*
	 * The cgi has come to an end, by itself or with a signal...
	 */

	if (wsi->http.cgi)
		lwsl_wsi_info(wsi, "post_in_expected %d",
			   (int)wsi->http.cgi->post_in_expected);

	/*
	 * Grace period to handle the incoming stdout
	 */

	if (wsi->http.cgi)
		lws_sul_schedule(wsi->a.context, wsi->tsi, &wsi->http.cgi->sul_grace,
			 lws_cgi_grace, 1 * LWS_US_PER_SEC);
}

int
lws_cgi(struct lws *wsi, const char * const *exec_array,
	int script_uri_path_len, int timeout_secs,
	const struct lws_protocol_vhost_options *mp_cgienv)
{
	struct lws_context_per_thread *pt = &wsi->a.context->pt[(int)wsi->tsi];
	struct lws_spawn_piped_info info;
	char *env_array[30], cgi_path[500], e[1024], *p = e,
	     *end = p + sizeof(e) - 1, tok[256], *t, *sum, *sumend;
	struct lws_cgi *cgi;
	int n, m = 0, i, uritok = -1, c;

	/*
	 * give the cgi stream wsi a cgi struct
	 */

	wsi->http.cgi = lws_zalloc(sizeof(*wsi->http.cgi), "new cgi");
	if (!wsi->http.cgi) {
		lwsl_wsi_err(wsi, "OOM");
		return -1;
	}

	wsi->http.cgi->response_code = HTTP_STATUS_OK;

	cgi = wsi->http.cgi;
	cgi->wsi = wsi; /* set cgi's owning wsi */
	sum = cgi->summary;
	sumend = sum + strlen(cgi->summary) - 1;

	if (timeout_secs)
		lws_set_timeout(wsi, PENDING_TIMEOUT_CGI, timeout_secs);

	/* the cgi stdout is always sending us http1.x header data first */
	wsi->hdr_state = LCHS_HEADER;

	/* add us to the pt list of active cgis */
	lwsl_wsi_debug(wsi, "adding cgi %p to list", wsi->http.cgi);
	cgi->cgi_list = pt->http.cgi_list;
	pt->http.cgi_list = cgi;

	/* if it's not already running, start the cleanup timer */
	if (!pt->sul_cgi.list.owner)
		lws_sul_schedule(pt->context, (int)(pt - pt->context->pt), &pt->sul_cgi,
				 lws_cgi_sul_cb, 3 * LWS_US_PER_SEC);

	sum += lws_snprintf(sum, lws_ptr_diff_size_t(sumend, sum), "%s ", exec_array[0]);

	if (0) {
		char *pct = lws_hdr_simple_ptr(wsi,
				WSI_TOKEN_HTTP_CONTENT_ENCODING);

		if (pct && !strcmp(pct, "gzip"))
			wsi->http.cgi->gzip_inflate = 1;
	}

	/* prepare his CGI env */

	n = 0;

	if (lws_is_ssl(wsi)) {
		env_array[n++] = p;
		p += lws_snprintf(p, lws_ptr_diff_size_t(end, p), "HTTPS=ON");
		p++;
	}

	if (wsi->http.ah) {
		static const unsigned char meths[] = {
			WSI_TOKEN_GET_URI,
			WSI_TOKEN_POST_URI,
#if defined(LWS_WITH_HTTP_UNCOMMON_HEADERS)
			WSI_TOKEN_OPTIONS_URI,
			WSI_TOKEN_PUT_URI,
			WSI_TOKEN_PATCH_URI,
			WSI_TOKEN_DELETE_URI,
#endif
			WSI_TOKEN_CONNECT,
			WSI_TOKEN_HEAD_URI,
		#ifdef LWS_WITH_HTTP2
			WSI_TOKEN_HTTP_COLON_PATH,
		#endif
		};
		static const char * const meth_names[] = {
			"GET", "POST",
#if defined(LWS_WITH_HTTP_UNCOMMON_HEADERS)
			"OPTIONS", "PUT", "PATCH", "DELETE",
#endif
			"CONNECT", "HEAD", ":path"
		};

		if (script_uri_path_len >= 0)
			for (m = 0; m < (int)LWS_ARRAY_SIZE(meths); m++)
				if (lws_hdr_total_length(wsi, meths[m]) >=
						script_uri_path_len) {
					uritok = meths[m];
					break;
				}

		if (script_uri_path_len < 0 && uritok < 0)
			goto bail;
//		if (script_uri_path_len < 0)
//			uritok = 0;

		if (m >= 0) {
			env_array[n++] = p;
			if (m < (int)LWS_ARRAY_SIZE(meths) - 1) {
				p += lws_snprintf(p, lws_ptr_diff_size_t(end, p),
						  "REQUEST_METHOD=%s",
						  meth_names[m]);
				sum += lws_snprintf(sum, lws_ptr_diff_size_t(sumend, sum), "%s ",
						    meth_names[m]);
#if defined(LWS_ROLE_H2)
			} else {
				p += lws_snprintf(p, lws_ptr_diff_size_t(end, p),
						  "REQUEST_METHOD=%s",
			  lws_hdr_simple_ptr(wsi, WSI_TOKEN_HTTP_COLON_METHOD));
				sum += lws_snprintf(sum, lws_ptr_diff_size_t(sumend, sum), "%s ",
					lws_hdr_simple_ptr(wsi,
						  WSI_TOKEN_HTTP_COLON_METHOD));
#endif
			}
			p++;
		}

		if (uritok >= 0)
			sum += lws_snprintf(sum, lws_ptr_diff_size_t(sumend, sum), "%s ",
					    lws_hdr_simple_ptr(wsi, (enum lws_token_indexes)uritok));

		env_array[n++] = p;
		p += lws_snprintf(p, lws_ptr_diff_size_t(end, p), "QUERY_STRING=");
		/* dump the individual URI Arg parameters */
		m = 0;
		while (script_uri_path_len >= 0) {
			i = lws_hdr_copy_fragment(wsi, tok, sizeof(tok),
					     WSI_TOKEN_HTTP_URI_ARGS, m);
			if (i < 0)
				break;
			t = tok;
			while (*t && *t != '=' && p < end - 4)
				*p++ = *t++;
			if (*t == '=')
				*p++ = *t++;
			i = urlencode(t, i - lws_ptr_diff(t, tok), p, lws_ptr_diff(end, p));
			if (i > 0) {
				p += i;
				*p++ = '&';
			}
			m++;
		}
		if (m)
			p--;
		*p++ = '\0';

		if (uritok >= 0) {
			strcpy(cgi_path, "REQUEST_URI=");
			c = lws_hdr_copy(wsi, cgi_path + 12,
					 sizeof(cgi_path) - 12, (enum lws_token_indexes)uritok);
			if (c < 0)
				goto bail;

			cgi_path[sizeof(cgi_path) - 1] = '\0';
			env_array[n++] = cgi_path;
		}

		sum += lws_snprintf(sum, lws_ptr_diff_size_t(sumend, sum), "%s", env_array[n - 1]);

		if (script_uri_path_len >= 0) {
			env_array[n++] = p;
			p += lws_snprintf(p, lws_ptr_diff_size_t(end, p), "PATH_INFO=%s",
				      cgi_path + 12 + script_uri_path_len);
			p++;
		}
	}
#if defined(LWS_WITH_HTTP_UNCOMMON_HEADERS)
	if (script_uri_path_len >= 0 &&
	    lws_hdr_total_length(wsi, WSI_TOKEN_HTTP_REFERER)) {
		env_array[n++] = p;
		p += lws_snprintf(p, lws_ptr_diff_size_t(end, p), "HTTP_REFERER=%s",
			      lws_hdr_simple_ptr(wsi, WSI_TOKEN_HTTP_REFERER));
		p++;
	}
#endif
	if (script_uri_path_len >= 0 &&
	    lws_hdr_total_length(wsi, WSI_TOKEN_HOST)) {
		env_array[n++] = p;
		p += lws_snprintf(p, lws_ptr_diff_size_t(end, p), "HTTP_HOST=%s",
			      lws_hdr_simple_ptr(wsi, WSI_TOKEN_HOST));
		p++;
	}
	if (script_uri_path_len >= 0 &&
	    lws_hdr_total_length(wsi, WSI_TOKEN_HTTP_COOKIE)) {
		env_array[n++] = p;
		p += lws_snprintf(p, lws_ptr_diff_size_t(end, p), "HTTP_COOKIE=");
		m = lws_hdr_copy(wsi, p, lws_ptr_diff(end, p), WSI_TOKEN_HTTP_COOKIE);
		if (m > 0)
			p += lws_hdr_total_length(wsi, WSI_TOKEN_HTTP_COOKIE);
		*p++ = '\0';
	}
#if defined(LWS_WITH_HTTP_UNCOMMON_HEADERS)
	if (script_uri_path_len >= 0 &&
	    lws_hdr_total_length(wsi, WSI_TOKEN_HTTP_USER_AGENT)) {
		env_array[n++] = p;
		p += lws_snprintf(p, lws_ptr_diff_size_t(end, p), "HTTP_USER_AGENT=%s",
			    lws_hdr_simple_ptr(wsi, WSI_TOKEN_HTTP_USER_AGENT));
		p++;
	}
#endif
	if (script_uri_path_len >= 0 &&
	    lws_hdr_total_length(wsi, WSI_TOKEN_HTTP_CONTENT_ENCODING)) {
		env_array[n++] = p;
		p += lws_snprintf(p, lws_ptr_diff_size_t(end, p), "HTTP_CONTENT_ENCODING=%s",
		      lws_hdr_simple_ptr(wsi, WSI_TOKEN_HTTP_CONTENT_ENCODING));
		p++;
	}
	if (script_uri_path_len >= 0 &&
	    lws_hdr_total_length(wsi, WSI_TOKEN_HTTP_ACCEPT)) {
		env_array[n++] = p;
		p += lws_snprintf(p, lws_ptr_diff_size_t(end, p), "HTTP_ACCEPT=%s",
			      lws_hdr_simple_ptr(wsi, WSI_TOKEN_HTTP_ACCEPT));
		p++;
	}
	if (script_uri_path_len >= 0 &&
	    lws_hdr_total_length(wsi, WSI_TOKEN_HTTP_ACCEPT_ENCODING)) {
		env_array[n++] = p;
		p += lws_snprintf(p, lws_ptr_diff_size_t(end, p), "HTTP_ACCEPT_ENCODING=%s",
		      lws_hdr_simple_ptr(wsi, WSI_TOKEN_HTTP_ACCEPT_ENCODING));
		p++;
	}
	if (script_uri_path_len >= 0 &&
	    uritok == WSI_TOKEN_POST_URI) {
		if (lws_hdr_total_length(wsi, WSI_TOKEN_HTTP_CONTENT_TYPE)) {
			env_array[n++] = p;
			p += lws_snprintf(p, lws_ptr_diff_size_t(end, p), "CONTENT_TYPE=%s",
			  lws_hdr_simple_ptr(wsi, WSI_TOKEN_HTTP_CONTENT_TYPE));
			p++;
		}
		if (!wsi->http.cgi->gzip_inflate &&
		    lws_hdr_total_length(wsi, WSI_TOKEN_HTTP_CONTENT_LENGTH)) {
			env_array[n++] = p;
			p += lws_snprintf(p, lws_ptr_diff_size_t(end, p), "CONTENT_LENGTH=%s",
					  lws_hdr_simple_ptr(wsi,
					  WSI_TOKEN_HTTP_CONTENT_LENGTH));
			p++;
		}

		if (lws_hdr_total_length(wsi, WSI_TOKEN_HTTP_CONTENT_LENGTH))
			wsi->http.cgi->post_in_expected = (lws_filepos_t)
				atoll(lws_hdr_simple_ptr(wsi,
						WSI_TOKEN_HTTP_CONTENT_LENGTH));
	}


	env_array[n++] = p;
	p += lws_snprintf(p, lws_ptr_diff_size_t(end, p), "PATH=/bin:/usr/bin:/usr/local/bin:/var/www/cgi-bin");
	p++;

	env_array[n++] = p;
	p += lws_snprintf(p, lws_ptr_diff_size_t(end, p), "SCRIPT_PATH=%s", exec_array[0]);
	p++;

	while (mp_cgienv) {
		env_array[n++] = p;
		p += lws_snprintf(p, lws_ptr_diff_size_t(end, p), "%s=%s", mp_cgienv->name,
			      mp_cgienv->value);
		if (!strcmp(mp_cgienv->name, "GIT_PROJECT_ROOT")) {
			wsi->http.cgi->implied_chunked = 1;
			wsi->http.cgi->explicitly_chunked = 1;
		}
		lwsl_info("   Applying mount-specific cgi env '%s'\n",
			   env_array[n - 1]);
		p++;
		mp_cgienv = mp_cgienv->next;
	}

	env_array[n++] = p;
	p += lws_snprintf(p, lws_ptr_diff_size_t(end, p), "SERVER_SOFTWARE=lws");
	p++;

	env_array[n] = NULL;

#if 0
	for (m = 0; m < n; m++)
		lwsl_notice("    %s\n", env_array[m]);
#endif

	memset(&info, 0, sizeof(info));
	info.env_array = (const char **)env_array;
	info.exec_array = exec_array;
	info.max_log_lines = 20000;
	info.opt_parent = wsi;
	info.timeout_us = 5 * 60 * LWS_US_PER_SEC;
	info.tsi = wsi->tsi;
	info.vh = wsi->a.vhost;
	info.ops = &role_ops_cgi;
	info.plsp = &wsi->http.cgi->lsp;
	info.opaque = wsi;
	info.reap_cb = lws_cgi_reap_cb;

	/*
	 * Actually having made the env, as a cgi we don't need the ah
	 * any more
	 */
	if (script_uri_path_len >= 0) {
		lws_header_table_detach(wsi, 0);
		info.disable_ctrlc = 1;
	}

	wsi->http.cgi->lsp = lws_spawn_piped(&info);
	if (!wsi->http.cgi->lsp) {
		lwsl_err("%s: spawn failed\n", __func__);
		goto bail;
	}

	/* we are the parent process */

	wsi->a.context->count_cgi_spawned++;

	/* inform cgi owner of the child PID */
	n = user_callback_handle_rxflow(wsi->a.protocol->callback, wsi,
				    LWS_CALLBACK_CGI_PROCESS_ATTACH,
				    wsi->user_space, NULL, (unsigned int)cgi->lsp->child_pid);
	(void)n;

	return 0;

bail:
	lws_sul_cancel(&wsi->http.cgi->sul_grace);
	lws_free_set_NULL(wsi->http.cgi);

	lwsl_err("%s: failed\n", __func__);

	return -1;
}

/* we have to parse out these headers in the CGI output */

static const char * const significant_hdr[SIGNIFICANT_HDR_COUNT] = {
	"content-length: ",
	"location: ",
	"status: ",
	"transfer-encoding: chunked",
	"content-encoding: gzip",
};

enum header_recode {
	HR_NAME,
	HR_WHITESPACE,
	HR_ARG,
	HR_CRLF,
};

int
lws_cgi_write_split_stdout_headers(struct lws *wsi)
{
	int n, m, cmd;
	unsigned char buf[LWS_PRE + 4096], *start = &buf[LWS_PRE], *p = start,
			*end = &buf[sizeof(buf) - 1 - LWS_PRE], *name,
			*value = NULL;
	char c, hrs;

	if (!wsi->http.cgi)
		return -1;

	while (wsi->hdr_state != LHCS_PAYLOAD) {
		/*
		 * We have to separate header / finalize and payload chunks,
		 * since they need to be handled separately
		 */
		switch (wsi->hdr_state) {
		case LHCS_RESPONSE:
			lwsl_wsi_debug(wsi, "LHCS_RESPONSE: iss response %d",
					    wsi->http.cgi->response_code);
			if (lws_add_http_header_status(wsi,
						   (unsigned int)wsi->http.cgi->response_code,
						       &p, end))
				return 1;
			if (!wsi->http.cgi->explicitly_chunked &&
			    !wsi->http.cgi->content_length &&
				lws_add_http_header_by_token(wsi,
					WSI_TOKEN_HTTP_TRANSFER_ENCODING,
					(unsigned char *)"chunked", 7, &p, end))
				return 1;
			if (!(wsi->mux_substream))
				if (lws_add_http_header_by_token(wsi,
						WSI_TOKEN_CONNECTION,
						(unsigned char *)"close", 5,
						&p, end))
					return 1;
			n = lws_write(wsi, start, lws_ptr_diff_size_t(p, start),
				      LWS_WRITE_HTTP_HEADERS | LWS_WRITE_NO_FIN);

			/*
			 * so we have a bunch of http/1 style ascii headers
			 * starting from wsi->http.cgi->headers_buf through
			 * wsi->http.cgi->headers_pos.  These are OK for http/1
			 * connections, but they're no good for http/2 conns.
			 *
			 * Let's redo them at headers_pos forward using the
			 * correct coding for http/1 or http/2
			 */
			if (!wsi->mux_substream)
				goto post_hpack_recode;

			p = wsi->http.cgi->headers_start;
			wsi->http.cgi->headers_start =
					wsi->http.cgi->headers_pos;
			wsi->http.cgi->headers_dumped =
					wsi->http.cgi->headers_start;
			hrs = HR_NAME;
			name = buf;

			while (p < wsi->http.cgi->headers_start) {
				switch (hrs) {
				case HR_NAME:
					/*
					 * in http/2 upper-case header names
					 * are illegal.  So convert to lower-
					 * case.
					 */
					if (name - buf > 64)
						return -1;
					if (*p != ':') {
						if (*p >= 'A' && *p <= 'Z')
							*name++ = (unsigned char)((*p++) +
								  ('a' - 'A'));
						else
							*name++ = *p++;
					} else {
						p++;
						*name++ = '\0';
						value = name;
						hrs = HR_WHITESPACE;
					}
					break;
				case HR_WHITESPACE:
					if (*p == ' ') {
						p++;
						break;
					}
					hrs = HR_ARG;
					/* fallthru */
				case HR_ARG:
					if (name > end - 64)
						return -1;

					if (*p != '\x0a' && *p != '\x0d') {
						*name++ = *p++;
						break;
					}
					hrs = HR_CRLF;
					/* fallthru */
				case HR_CRLF:
					if ((*p != '\x0a' && *p != '\x0d') ||
					    p + 1 == wsi->http.cgi->headers_start) {
						*name = '\0';
						if ((strcmp((const char *)buf,
							    "transfer-encoding")
						)) {
							lwsl_debug("+ %s: %s\n",
								   buf, value);
							if (
					lws_add_http_header_by_name(wsi, buf,
					(unsigned char *)value, lws_ptr_diff(name, value),
					(unsigned char **)&wsi->http.cgi->headers_pos,
					(unsigned char *)wsi->http.cgi->headers_end))
								return 1;
							hrs = HR_NAME;
							name = buf;
							break;
						}
					}
					p++;
					break;
				}
			}
post_hpack_recode:
			/* finalize cached headers before dumping them */
			if (lws_finalize_http_header(wsi,
			      (unsigned char **)&wsi->http.cgi->headers_pos,
			      (unsigned char *)wsi->http.cgi->headers_end)) {

				lwsl_notice("finalize failed\n");
				return -1;
			}

			wsi->hdr_state = LHCS_DUMP_HEADERS;
			wsi->reason_bf |= LWS_CB_REASON_AUX_BF__CGI_HEADERS;
			lws_callback_on_writable(wsi);
			/* back to the loop for writeability again */
			return 0;

		case LHCS_DUMP_HEADERS:

			n = (int)(wsi->http.cgi->headers_pos -
			    wsi->http.cgi->headers_dumped);
			if (n > 512)
				n = 512;

			lwsl_wsi_debug(wsi, "LHCS_DUMP_HEADERS: %d", n);

			cmd = LWS_WRITE_HTTP_HEADERS_CONTINUATION;
			if (wsi->http.cgi->headers_dumped + n !=
						wsi->http.cgi->headers_pos) {
				lwsl_notice("adding no fin flag\n");
				cmd |= LWS_WRITE_NO_FIN;
			}

			m = lws_write(wsi,
				 (unsigned char *)wsi->http.cgi->headers_dumped,
				      (unsigned int)n, (enum lws_write_protocol)cmd);
			if (m < 0) {
				lwsl_wsi_debug(wsi, "write says %d", m);
				return -1;
			}
			wsi->http.cgi->headers_dumped += n;
			if (wsi->http.cgi->headers_dumped ==
			    wsi->http.cgi->headers_pos) {
				wsi->hdr_state = LHCS_PAYLOAD;
				lws_free_set_NULL(wsi->http.cgi->headers_buf);
				lwsl_wsi_debug(wsi, "freed cgi headers");

				if (wsi->http.cgi->post_in_expected) {
					lwsl_wsi_info(wsi, "post data still "
							   "expected, asking "
							   "for writeable");
					lws_callback_on_writable(wsi);
				}

			} else {
				wsi->reason_bf |=
					LWS_CB_REASON_AUX_BF__CGI_HEADERS;
				lws_callback_on_writable(wsi);
			}

			/*
			 * writeability becomes uncertain now we wrote
			 * something, we must return to the event loop
			 */
			return 0;
		}

		if (!wsi->http.cgi->headers_buf) {
			/* if we don't already have a headers buf, cook one */
			n = 2048;
			if (wsi->mux_substream)
				n = 4096;
			wsi->http.cgi->headers_buf = lws_malloc((unsigned int)n + LWS_PRE,
							   "cgi hdr buf");
			if (!wsi->http.cgi->headers_buf) {
				lwsl_wsi_err(wsi, "OOM");
				return -1;
			}

			lwsl_wsi_debug(wsi, "allocated cgi hdrs");
			wsi->http.cgi->headers_start =
					wsi->http.cgi->headers_buf + LWS_PRE;
			wsi->http.cgi->headers_pos = wsi->http.cgi->headers_start;
			wsi->http.cgi->headers_dumped = wsi->http.cgi->headers_pos;
			wsi->http.cgi->headers_end =
					wsi->http.cgi->headers_buf + n - 1;

			for (n = 0; n < SIGNIFICANT_HDR_COUNT; n++) {
				wsi->http.cgi->match[n] = 0;
				wsi->http.cgi->lp = 0;
			}
		}

		n = lws_get_socket_fd(wsi->http.cgi->lsp->stdwsi[LWS_STDOUT]);
		if (n < 0)
			return -1;
		n = (int)read(n, &c, 1);
		if (n < 0) {
			if (errno != EAGAIN) {
				lwsl_wsi_debug(wsi, "read says %d", n);
				return -1;
			}
			else
				n = 0;

			if (wsi->http.cgi->headers_pos >=
					wsi->http.cgi->headers_end - 4) {
				lwsl_wsi_notice(wsi, "CGI hdrs > buf size");

				return -1;
			}
		}
		if (!n)
			goto agin;

		lwsl_wsi_debug(wsi, "-- 0x%02X %c %d %d", (unsigned char)c, c,
				    wsi->http.cgi->match[1], wsi->hdr_state);
		if (!c)
			return -1;
		switch (wsi->hdr_state) {
		case LCHS_HEADER:
			hdr:
			for (n = 0; n < SIGNIFICANT_HDR_COUNT; n++) {
				/*
				 * significant headers with
				 * numeric decimal payloads
				 */
				if (!significant_hdr[n][wsi->http.cgi->match[n]] &&
				    (c >= '0' && c <= '9') &&
				    wsi->http.cgi->lp < (int)sizeof(wsi->http.cgi->l) - 1) {
					wsi->http.cgi->l[wsi->http.cgi->lp++] = c;
					wsi->http.cgi->l[wsi->http.cgi->lp] = '\0';
					switch (n) {
					case SIGNIFICANT_HDR_CONTENT_LENGTH:
						wsi->http.cgi->content_length =
							(lws_filepos_t)atoll(wsi->http.cgi->l);
						break;
					case SIGNIFICANT_HDR_STATUS:
						wsi->http.cgi->response_code =
							atoi(wsi->http.cgi->l);
						lwsl_wsi_debug(wsi, "Status set to %d",
								wsi->http.cgi->response_code);
						break;
					default:
						break;
					}
				}
				/* hits up to the NUL are sticky until next hdr */
				if (significant_hdr[n][wsi->http.cgi->match[n]]) {
					if (tolower(c) ==
					    significant_hdr[n][wsi->http.cgi->match[n]])
						wsi->http.cgi->match[n]++;
					else
						wsi->http.cgi->match[n] = 0;
				}
			}

			/* some cgi only send us \x0a for EOL */
			if (c == '\x0a') {
				wsi->hdr_state = LCHS_SINGLE_0A;
				*wsi->http.cgi->headers_pos++ = '\x0d';
			}
			*wsi->http.cgi->headers_pos++ = (unsigned char)c;
			if (c == '\x0d')
				wsi->hdr_state = LCHS_LF1;

			if (wsi->hdr_state != LCHS_HEADER &&
			    !significant_hdr[SIGNIFICANT_HDR_TRANSFER_ENCODING]
				    [wsi->http.cgi->match[
					 SIGNIFICANT_HDR_TRANSFER_ENCODING]]) {
				lwsl_wsi_info(wsi, "cgi produced chunked");
				wsi->http.cgi->explicitly_chunked = 1;
			}

			/* presence of Location: mandates 302 retcode */
			if (wsi->hdr_state != LCHS_HEADER &&
			    !significant_hdr[SIGNIFICANT_HDR_LOCATION][
			      wsi->http.cgi->match[SIGNIFICANT_HDR_LOCATION]]) {
				lwsl_wsi_debug(wsi, "CGI: Location hdr seen");
				wsi->http.cgi->response_code = 302;
			}
			break;
		case LCHS_LF1:
			*wsi->http.cgi->headers_pos++ = (unsigned char)c;
			if (c == '\x0a') {
				wsi->hdr_state = LCHS_CR2;
				break;
			}
			/* we got \r[^\n]... it's unreasonable */
			lwsl_wsi_debug(wsi, "funny CRLF 0x%02X",
					    (unsigned char)c);
			return -1;

		case LCHS_CR2:
			if (c == '\x0d') {
				/* drop the \x0d */
				wsi->hdr_state = LCHS_LF2;
				break;
			}
			wsi->hdr_state = LCHS_HEADER;
			for (n = 0; n < SIGNIFICANT_HDR_COUNT; n++)
				wsi->http.cgi->match[n] = 0;
			wsi->http.cgi->lp = 0;
			goto hdr;

		case LCHS_LF2:
		case LCHS_SINGLE_0A:
			m = wsi->hdr_state;
			if (c == '\x0a') {
				lwsl_wsi_debug(wsi, "Content-Length: %lld",
					(unsigned long long)
					wsi->http.cgi->content_length);
				wsi->hdr_state = LHCS_RESPONSE;
				/*
				 * drop the \0xa ... finalize
				 * will add it if needed (HTTP/1)
				 */
				break;
			}
			if (m == LCHS_LF2)
				/* we got \r\n\r[^\n]... unreasonable */
				return -1;
			/* we got \x0anext header, it's reasonable */
			*wsi->http.cgi->headers_pos++ = (unsigned char)c;
			wsi->hdr_state = LCHS_HEADER;
			for (n = 0; n < SIGNIFICANT_HDR_COUNT; n++)
				wsi->http.cgi->match[n] = 0;
			wsi->http.cgi->lp = 0;
			break;
		case LHCS_PAYLOAD:
			break;
		}

agin:
		/* ran out of input, ended the hdrs, or filled up the hdrs buf */
		if (!n || wsi->hdr_state == LHCS_PAYLOAD)
			return 0;
	}

	/* payload processing */

	m = !wsi->http.cgi->implied_chunked && !wsi->mux_substream &&
	//    !wsi->http.cgi->explicitly_chunked &&
	    !wsi->http.cgi->content_length;
	n = lws_get_socket_fd(wsi->http.cgi->lsp->stdwsi[LWS_STDOUT]);
	if (n < 0)
		return -1;
	n = (int)read(n, start, sizeof(buf) - LWS_PRE);

	if (n < 0 && errno != EAGAIN) {
		lwsl_wsi_debug(wsi, "stdout read says %d", n);
		return -1;
	}
	if (n > 0) {
		// lwsl_hexdump_notice(buf, n);

		if (!wsi->mux_substream && m) {
			char chdr[LWS_HTTP_CHUNK_HDR_SIZE];
			m = lws_snprintf(chdr, LWS_HTTP_CHUNK_HDR_SIZE - 3,
					 "%X\x0d\x0a", n);
			memmove(start + m, start, (unsigned int)n);
			memcpy(start, chdr, (unsigned int)m);
			memcpy(start + m + n, "\x0d\x0a", 2);
			n += m + 2;
		}


#if defined(LWS_WITH_HTTP2)
		if (wsi->mux_substream) {
			struct lws *nwsi = lws_get_network_wsi(wsi);

			__lws_set_timeout(wsi,
				PENDING_TIMEOUT_HTTP_KEEPALIVE_IDLE, 31);

			if (!nwsi->immortal_substream_count)
				__lws_set_timeout(nwsi,
					PENDING_TIMEOUT_HTTP_KEEPALIVE_IDLE, 31);
		}
#endif

		cmd = LWS_WRITE_HTTP;
		if (wsi->http.cgi->content_length_seen + (unsigned int)n ==
						wsi->http.cgi->content_length)
			cmd = LWS_WRITE_HTTP_FINAL;

		m = lws_write(wsi, (unsigned char *)start, (unsigned int)n, (enum lws_write_protocol)cmd);
		//lwsl_notice("write %d\n", m);
		if (m < 0) {
			lwsl_wsi_debug(wsi, "stdout write says %d\n", m);
			return -1;
		}
		wsi->http.cgi->content_length_seen += (unsigned int)n;
	} else {

		if (!wsi->mux_substream && m) {
			uint8_t term[LWS_PRE + 6];

			lwsl_wsi_info(wsi, "sent trailer");
			memcpy(term + LWS_PRE, (uint8_t *)"0\x0d\x0a\x0d\x0a", 5);

			if (lws_write(wsi, term + LWS_PRE, 5,
				      LWS_WRITE_HTTP_FINAL) != 5)
				return -1;

			wsi->http.cgi->cgi_transaction_over = 1;

			return 0;
		}

		if (wsi->cgi_stdout_zero_length) {
			lwsl_wsi_debug(wsi, "stdout is POLLHUP'd");
			if (wsi->mux_substream)
				m = lws_write(wsi, (unsigned char *)start, 0,
					      LWS_WRITE_HTTP_FINAL);
			else
				return -1;
			return 1;
		}
		wsi->cgi_stdout_zero_length = 1;
	}
	return 0;
}

int
lws_cgi_kill(struct lws *wsi)
{
	struct lws_cgi_args args;
	pid_t pid;
	int n, m = 0;

	if (!wsi->http.cgi || !wsi->http.cgi->lsp)
		return 0;

	pid = wsi->http.cgi->lsp->child_pid;

	args.stdwsi = &wsi->http.cgi->lsp->stdwsi[0];
	lws_spawn_piped_kill_child_process(wsi->http.cgi->lsp);
	/* that has invalidated and NULL'd wsi->http.cgi->lsp */

	if (pid != -1) {
		if (wsi->http.cgi)
			m = wsi->http.cgi->being_closed;
		n = user_callback_handle_rxflow(wsi->a.protocol->callback, wsi,
						LWS_CALLBACK_CGI_TERMINATED,
						wsi->user_space, (void *)&args,
						(unsigned int)pid);
		if (n && !m)
			lws_close_free_wsi(wsi, 0, "lws_cgi_kill");
	}

	return 0;
}

int
lws_cgi_kill_terminated(struct lws_context_per_thread *pt)
{
	struct lws_cgi **pcgi, *cgi = NULL;
	int status, n = 1;

	while (n > 0) {
		/* find finished guys but don't reap yet */
		n = waitpid(-1, &status, WNOHANG);
		if (n <= 0)
			continue;
		lwsl_cx_debug(pt->context, "observed PID %d terminated", n);

		pcgi = &pt->http.cgi_list;

		/* check all the subprocesses on the cgi list */
		while (*pcgi) {
			/* get the next one first as list may change */
			cgi = *pcgi;
			pcgi = &(*pcgi)->cgi_list;

			if (cgi->lsp->child_pid <= 0)
				continue;

			/* finish sending cached headers */
			if (cgi->headers_buf)
				continue;

			/* wait for stdout to be drained */
			if (cgi->content_length > cgi->content_length_seen)
				continue;

			if (cgi->content_length) {
				lwsl_cx_debug(pt->context, "expected content "
							   "length seen: %lld",
				(unsigned long long)cgi->content_length_seen);
			}

			/* reap it */
			waitpid(n, &status, WNOHANG);
			/*
			 * he's already terminated so no need for kill()
			 * but we should do the terminated cgi callback
			 * and close him if he's not already closing
			 */
			if (n == cgi->lsp->child_pid) {

				if (!cgi->content_length) {
					/*
					 * well, if he sends chunked...
					 * give him 2s after the
					 * cgi terminated to send buffered
					 */
					cgi->chunked_grace++;
					continue;
				}

				/* defeat kill() */
				cgi->lsp->child_pid = 0;
				lws_cgi_kill(cgi->wsi);

				break;
			}
			cgi = NULL;
		}
		/* if not found on the cgi list, as he's one of ours, reap */
		if (!cgi)
			waitpid(n, &status, WNOHANG);

	}

	pcgi = &pt->http.cgi_list;

	/* check all the subprocesses on the cgi list */
	while (*pcgi) {
		/* get the next one first as list may change */
		cgi = *pcgi;
		pcgi = &(*pcgi)->cgi_list;

		if (!cgi || !cgi->lsp || cgi->lsp->child_pid <= 0)
			continue;

		/* we deferred killing him after reaping his PID */
		if (cgi->chunked_grace) {
			cgi->chunked_grace++;
			if (cgi->chunked_grace < 2)
				continue;
			goto finish_him;
		}

		/* finish sending cached headers */
		if (cgi->headers_buf)
			continue;

		/* wait for stdout to be drained */
		if (cgi->content_length > cgi->content_length_seen)
			continue;

		if (cgi->content_length)
			lwsl_wsi_debug(cgi->wsi, "expected cont len seen: %lld",
				  (unsigned long long)cgi->content_length_seen);

		/* reap it */
		if (waitpid(cgi->lsp->child_pid, &status, WNOHANG) > 0) {

			if (!cgi->content_length) {
				/*
				 * well, if he sends chunked...
				 * give him 2s after the
				 * cgi terminated to send buffered
				 */
				cgi->chunked_grace++;
				continue;
			}
finish_him:
			lwsl_cx_debug(pt->context, "found PID %d on cgi list",
						   cgi->lsp->child_pid);

			/* defeat kill() */
			cgi->lsp->child_pid = 0;
			lws_cgi_kill(cgi->wsi);

			break;
		}
	}

	return 0;
}

struct lws *
lws_cgi_get_stdwsi(struct lws *wsi, enum lws_enum_stdinouterr ch)
{
	if (!wsi->http.cgi)
		return NULL;

	return wsi->http.cgi->lsp->stdwsi[ch];
}

void
lws_cgi_remove_and_kill(struct lws *wsi)
{
	struct lws_context_per_thread *pt = &wsi->a.context->pt[(int)wsi->tsi];
	struct lws_cgi **pcgi = &pt->http.cgi_list;

	/* remove us from the cgi list */

	while (*pcgi) {
		if (*pcgi == wsi->http.cgi) {
			/* drop us from the pt cgi list */
			*pcgi = (*pcgi)->cgi_list;
			break;
		}
		pcgi = &(*pcgi)->cgi_list;
	}
	if (wsi->http.cgi->headers_buf)
		lws_free_set_NULL(wsi->http.cgi->headers_buf);

	/* we have a cgi going, we must kill it */
	wsi->http.cgi->being_closed = 1;
	lws_cgi_kill(wsi);

	if (!pt->http.cgi_list)
		lws_sul_cancel(&pt->sul_cgi);
}
