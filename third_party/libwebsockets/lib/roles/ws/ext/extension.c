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

#include "extension-permessage-deflate.h"

void
lws_context_init_extensions(const struct lws_context_creation_info *info,
			    struct lws_context *context)
{
	lwsl_cx_info(context, " LWS_MAX_EXTENSIONS_ACTIVE: %u", LWS_MAX_EXTENSIONS_ACTIVE);
}

enum lws_ext_option_parser_states {
	LEAPS_SEEK_NAME,
	LEAPS_EAT_NAME,
	LEAPS_SEEK_VAL,
	LEAPS_EAT_DEC,
	LEAPS_SEEK_ARG_TERM
};

int
lws_ext_parse_options(const struct lws_extension *ext, struct lws *wsi,
		      void *ext_user, const struct lws_ext_options *opts,
		      const char *in, int len)
{
	enum lws_ext_option_parser_states leap = LEAPS_SEEK_NAME;
	unsigned int match_map = 0, n, m, w = 0, count_options = 0,
		     pending_close_quote = 0;
	struct lws_ext_option_arg oa;

	oa.option_name = NULL;

	while (opts[count_options].name)
		count_options++;
	while (len) {
		lwsl_wsi_ext(wsi, "'%c' %d", *in, leap);
		switch (leap) {
		case LEAPS_SEEK_NAME:
			if (*in == ' ')
				break;
			if (*in == ',') {
				len = 1;
				break;
			}
			match_map = (unsigned int)(1 << count_options) - 1;
			leap = LEAPS_EAT_NAME;
			w = 0;

		/* fallthru */

		case LEAPS_EAT_NAME:
			oa.start = NULL;
			oa.len = 0;
			m = match_map;
			n = 0;
			pending_close_quote = 0;
			while (m) {
				if (!(m & 1)) {
					m >>= 1;
					n++;
					continue;
				}
				lwsl_wsi_ext(wsi, "    m=%d, n=%d, w=%d", m, n, w);

				if (*in == opts[n].name[w]) {
					if (!opts[n].name[w + 1]) {
						oa.option_index = (int)n;
						lwsl_wsi_ext(wsi, "hit %d",
							 oa.option_index);
						leap = LEAPS_SEEK_VAL;
						if (len == 1)
							goto set_arg;
						break;
					}
				} else {
					match_map &= (unsigned int)~(1 << n);
					if (!match_map) {
						lwsl_wsi_ext(wsi, "empty match map");
						return -1;
					}
				}

				m >>= 1;
				n++;
			}
			w++;
			break;
		case LEAPS_SEEK_VAL:
			if (*in == ' ')
				break;
			if (*in == ',') {
				len = 1;
				break;
			}
			if (*in == ';' || len == 1) { /* ie,nonoptional */
				if (opts[oa.option_index].type == EXTARG_DEC)
					return -1;
				leap = LEAPS_SEEK_NAME;
				goto set_arg;
			}
			if (*in == '=') {
				w = 0;
				pending_close_quote = 0;
				if (opts[oa.option_index].type == EXTARG_NONE)
					return -1;

				leap = LEAPS_EAT_DEC;
				break;
			}
			return -1;

		case LEAPS_EAT_DEC:
			if (*in >= '0' && *in <= '9') {
				if (!w)
					oa.start = in;
				w++;
				if (len != 1)
					break;
			}
			if (!w && *in =='"') {
				pending_close_quote = 1;
				break;
			}
			if (!w)
				return -1;
			if (pending_close_quote && *in != '"' && len != 1)
				return -1;
			leap = LEAPS_SEEK_ARG_TERM;
			if (oa.start)
				oa.len = lws_ptr_diff(in, oa.start);
			if (len == 1)
				oa.len++;

set_arg:
			ext->callback(lws_get_context(wsi),
				ext, wsi, LWS_EXT_CB_OPTION_SET,
				ext_user, (char *)&oa, 0);
			if (len == 1)
				break;
			if (pending_close_quote && *in == '"')
				break;

			/* fallthru */

		case LEAPS_SEEK_ARG_TERM:
			if (*in == ' ')
				break;
			if (*in == ';') {
				leap = LEAPS_SEEK_NAME;
				break;
			}
			if (*in == ',') {
				len = 1;
				break;
			}
			return -1;
		}
		len--;
		in++;
	}

	return 0;
}


/* 0 = nobody had nonzero return, 1 = somebody had positive return, -1 = fail */

int lws_ext_cb_active(struct lws *wsi, int reason, void *arg, int len)
{
	int n, m, handled = 0;

	if (!wsi->ws)
		return 0;

	for (n = 0; n < wsi->ws->count_act_ext; n++) {
		m = wsi->ws->active_extensions[n]->callback(
			lws_get_context(wsi), wsi->ws->active_extensions[n],
			wsi, (enum lws_extension_callback_reasons)reason, wsi->ws->act_ext_user[n], arg, (size_t)len);
		if (m < 0) {
			lwsl_wsi_ext(wsi, "Ext '%s' failed to handle callback %d!",
				 wsi->ws->active_extensions[n]->name, reason);
			return -1;
		}
		/* valgrind... */
		if (reason == LWS_EXT_CB_DESTROY)
			wsi->ws->act_ext_user[n] = NULL;
		if (m > handled)
			handled = m;
	}

	return handled;
}

int lws_ext_cb_all_exts(struct lws_context *context, struct lws *wsi,
			int reason, void *arg, int len)
{
	int n = 0, m, handled = 0;
	const struct lws_extension *ext;

	if (!wsi || !wsi->a.vhost || !wsi->ws)
		return 0;

	ext = wsi->a.vhost->ws.extensions;

	while (ext && ext->callback && !handled) {
		m = ext->callback(context, ext, wsi, (enum lws_extension_callback_reasons)reason,
				  (void *)(lws_intptr_t)n, arg, (size_t)len);
		if (m < 0) {
			lwsl_wsi_ext(wsi, "Ext '%s' failed to handle callback %d!",
				 wsi->ws->active_extensions[n]->name, reason);
			return -1;
		}
		if (m)
			handled = 1;

		ext++;
		n++;
	}

	return 0;
}

int
lws_issue_raw_ext_access(struct lws *wsi, unsigned char *buf, size_t len)
{
	struct lws_tokens ebuf;
	int ret, m, n = 0;

	ebuf.token = buf;
	ebuf.len = (int)len;

	/*
	 * while we have original buf to spill ourselves, or extensions report
	 * more in their pipeline
	 */

	ret = 1;
	while (ret == 1) {

		/* default to nobody has more to spill */

		ret = 0;

		/* show every extension the new incoming data */
		m = lws_ext_cb_active(wsi, LWS_EXT_CB_PACKET_TX_PRESEND,
				      &ebuf, 0);
		if (m < 0)
			return -1;
		if (m) /* handled */
			ret = 1;

		if (buf != ebuf.token)
			/*
			 * extension recreated it:
			 * need to buffer this if not all sent
			 */
			wsi->ws->clean_buffer = 0;

		/* assuming they left us something to send, send it */

		if (ebuf.len) {
			n = lws_issue_raw(wsi, ebuf.token, (size_t)ebuf.len);
			if (n < 0) {
				lwsl_wsi_info(wsi, "closing from ext access");
				return -1;
			}

			/* always either sent it all or privately buffered */
			if (wsi->ws->clean_buffer)
				len = (size_t)n;

			lwsl_wsi_ext(wsi, "written %d bytes to client", n);
		}

		/* no extension has more to spill?  Then we can go */

		if (!ret)
			break;

		/* we used up what we had */

		ebuf.token = NULL;
		ebuf.len = 0;

		/*
		 * Did that leave the pipe choked?
		 * Or we had to hold on to some of it?
		 */

		if (!lws_send_pipe_choked(wsi) && !lws_has_buffered_out(wsi))
			/* no we could add more, lets's do that */
			continue;

		lwsl_wsi_debug(wsi, "choked");

		/*
		 * Yes, he's choked.  Don't spill the rest now get a callback
		 * when he is ready to send and take care of it there
		 */
		lws_callback_on_writable(wsi);
		wsi->ws->extension_data_pending = 1;
		ret = 0;
	}

	return (int)len;
}

int
lws_any_extension_handled(struct lws *wsi, enum lws_extension_callback_reasons r,
			  void *v, size_t len)
{
	struct lws_context *context = wsi->a.context;
	int n, handled = 0;

	if (!wsi->ws)
		return 0;

	/* maybe an extension will take care of it for us */

	for (n = 0; n < wsi->ws->count_act_ext && !handled; n++) {
		if (!wsi->ws->active_extensions[n]->callback)
			continue;

		handled |= wsi->ws->active_extensions[n]->callback(context,
			wsi->ws->active_extensions[n], wsi,
			r, wsi->ws->act_ext_user[n], v, len);
	}

	return handled;
}

int
lws_set_extension_option(struct lws *wsi, const char *ext_name,
			 const char *opt_name, const char *opt_val)
{
	struct lws_ext_option_arg oa;
	int idx = 0;

	if (!wsi->ws)
		return 0;

	/* first identify if the ext is active on this wsi */
	while (idx < wsi->ws->count_act_ext &&
	       strcmp(wsi->ws->active_extensions[idx]->name, ext_name))
		idx++;

	if (idx == wsi->ws->count_act_ext)
		return -1; /* request ext not active on this wsi */

	oa.option_name = opt_name;
	oa.option_index = 0;
	oa.start = opt_val;
	oa.len = 0;

	return wsi->ws->active_extensions[idx]->callback(wsi->a.context,
			wsi->ws->active_extensions[idx], wsi,
			LWS_EXT_CB_NAMED_OPTION_SET, wsi->ws->act_ext_user[idx],
			&oa, 0);
}
