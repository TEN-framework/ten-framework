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

/*
 * Official static header table for HPACK
 *        +-------+-----------------------------+---------------+
          | 1     | :authority                  |               |
          | 2     | :method                     | GET           |
          | 3     | :method                     | POST          |
          | 4     | :path                       | /             |
          | 5     | :path                       | /index.html   |
          | 6     | :scheme                     | http          |
          | 7     | :scheme                     | https         |
          | 8     | :status                     | 200           |
          | 9     | :status                     | 204           |
          | 10    | :status                     | 206           |
          | 11    | :status                     | 304           |
          | 12    | :status                     | 400           |
          | 13    | :status                     | 404           |
          | 14    | :status                     | 500           |
          | 15    | accept-charset              |               |
          | 16    | accept-encoding             | gzip, deflate |
          | 17    | accept-language             |               |
          | 18    | accept-ranges               |               |
          | 19    | accept                      |               |
          | 20    | access-control-allow-origin |               |
          | 21    | age                         |               |
          | 22    | allow                       |               |
          | 23    | authorization               |               |
          | 24    | cache-control               |               |
          | 25    | content-disposition         |               |
          | 26    | content-encoding            |               |
          | 27    | content-language            |               |
          | 28    | content-length              |               |
          | 29    | content-location            |               |
          | 30    | content-range               |               |
          | 31    | content-type                |               |
          | 32    | cookie                      |               |
          | 33    | date                        |               |
          | 34    | etag                        |               |
          | 35    | expect                      |               |
          | 36    | expires                     |               |
          | 37    | from                        |               |
          | 38    | host                        |               |
          | 39    | if-match                    |               |
          | 40    | if-modified-since           |               |
          | 41    | if-none-match               |               |
          | 42    | if-range                    |               |
          | 43    | if-unmodified-since         |               |
          | 44    | last-modified               |               |
          | 45    | link                        |               |
          | 46    | location                    |               |
          | 47    | max-forwards                |               |
          | 48    | proxy-authenticate          |               |
          | 49    | proxy-authorization         |               |
          | 50    | range                       |               |
          | 51    | referer                     |               |
          | 52    | refresh                     |               |
          | 53    | retry-after                 |               |
          | 54    | server                      |               |
          | 55    | set-cookie                  |               |
          | 56    | strict-transport-security   |               |
          | 57    | transfer-encoding           |               |
          | 58    | user-agent                  |               |
          | 59    | vary                        |               |
          | 60    | via                         |               |
          | 61    | www-authenticate            |               |
          +-------+-----------------------------+---------------+
*/

static const uint8_t static_hdr_len[62] = {
		0, /* starts at 1 */
		10,  7,  7,  5,  5,    7,  7,  7,  7,  7,
		 7,  7,  7,  7, 14,   15, 15, 13,  6, 27,
		 3,  5, 13, 13, 19,   16, 16, 14, 16, 13,
		12,  6,  4,  4,  6,    7,  4,  4,  8, 17,
		13,  8, 19, 13,  4,    8, 12, 18, 19,  5,
		 7,  7, 11,  6, 10,   25, 17, 10,  4,  3,
		16
};

static const unsigned char static_token[] = {
	0,
	WSI_TOKEN_HTTP_COLON_AUTHORITY,
	WSI_TOKEN_HTTP_COLON_METHOD,
	WSI_TOKEN_HTTP_COLON_METHOD,
	WSI_TOKEN_HTTP_COLON_PATH,
	WSI_TOKEN_HTTP_COLON_PATH,
	WSI_TOKEN_HTTP_COLON_SCHEME,
	WSI_TOKEN_HTTP_COLON_SCHEME,
	WSI_TOKEN_HTTP_COLON_STATUS,
	WSI_TOKEN_HTTP_COLON_STATUS,
	WSI_TOKEN_HTTP_COLON_STATUS,
	WSI_TOKEN_HTTP_COLON_STATUS,
	WSI_TOKEN_HTTP_COLON_STATUS,
	WSI_TOKEN_HTTP_COLON_STATUS,
	WSI_TOKEN_HTTP_COLON_STATUS,
	WSI_TOKEN_HTTP_ACCEPT_CHARSET,
	WSI_TOKEN_HTTP_ACCEPT_ENCODING,
	WSI_TOKEN_HTTP_ACCEPT_LANGUAGE,
	WSI_TOKEN_HTTP_ACCEPT_RANGES,
	WSI_TOKEN_HTTP_ACCEPT,
	WSI_TOKEN_HTTP_ACCESS_CONTROL_ALLOW_ORIGIN,
	WSI_TOKEN_HTTP_AGE,
	WSI_TOKEN_HTTP_ALLOW,
	WSI_TOKEN_HTTP_AUTHORIZATION,
	WSI_TOKEN_HTTP_CACHE_CONTROL,
	WSI_TOKEN_HTTP_CONTENT_DISPOSITION,
	WSI_TOKEN_HTTP_CONTENT_ENCODING,
	WSI_TOKEN_HTTP_CONTENT_LANGUAGE,
	WSI_TOKEN_HTTP_CONTENT_LENGTH,
	WSI_TOKEN_HTTP_CONTENT_LOCATION,
	WSI_TOKEN_HTTP_CONTENT_RANGE,
	WSI_TOKEN_HTTP_CONTENT_TYPE,
	WSI_TOKEN_HTTP_COOKIE,
	WSI_TOKEN_HTTP_DATE,
	WSI_TOKEN_HTTP_ETAG,
	WSI_TOKEN_HTTP_EXPECT,
	WSI_TOKEN_HTTP_EXPIRES,
	WSI_TOKEN_HTTP_FROM,
	WSI_TOKEN_HOST,
	WSI_TOKEN_HTTP_IF_MATCH,
	WSI_TOKEN_HTTP_IF_MODIFIED_SINCE,
	WSI_TOKEN_HTTP_IF_NONE_MATCH,
	WSI_TOKEN_HTTP_IF_RANGE,
	WSI_TOKEN_HTTP_IF_UNMODIFIED_SINCE,
	WSI_TOKEN_HTTP_LAST_MODIFIED,
	WSI_TOKEN_HTTP_LINK,
	WSI_TOKEN_HTTP_LOCATION,
	WSI_TOKEN_HTTP_MAX_FORWARDS,
	WSI_TOKEN_HTTP_PROXY_AUTHENTICATE,
	WSI_TOKEN_HTTP_PROXY_AUTHORIZATION,
	WSI_TOKEN_HTTP_RANGE,
	WSI_TOKEN_HTTP_REFERER,
	WSI_TOKEN_HTTP_REFRESH,
	WSI_TOKEN_HTTP_RETRY_AFTER,
	WSI_TOKEN_HTTP_SERVER,
	WSI_TOKEN_HTTP_SET_COOKIE,
	WSI_TOKEN_HTTP_STRICT_TRANSPORT_SECURITY,
	WSI_TOKEN_HTTP_TRANSFER_ENCODING,
	WSI_TOKEN_HTTP_USER_AGENT,
	WSI_TOKEN_HTTP_VARY,
	WSI_TOKEN_HTTP_VIA,
	WSI_TOKEN_HTTP_WWW_AUTHENTICATE,
};

/* some of the entries imply values as well as header names */

static const char * const http2_canned[] = {
	"",
	"",
	"GET",
	"POST",
	"/",
	"/index.html",
	"http",
	"https",
	"200",
	"204",
	"206",
	"304",
	"400",
	"404",
	"500",
	"",
	"gzip, deflate"
};

/* see minihuf.c */

#include "huftable.h"

static int huftable_decode(int pos, char c)
{
	int q = pos + !!c;

	if (lextable_terms[q >> 3] & (1 << (q & 7))) /* terminal */
		return lextable[q] | 0x8000;

	return pos + (lextable[q] << 1);
}

static int lws_frag_start(struct lws *wsi, int hdr_token_idx)
{
	struct allocated_headers *ah = wsi->http.ah;

	if (!ah) {
		lwsl_notice("%s: no ah\n", __func__);
		return 1;
	}

	ah->hdr_token_idx = -1;

	lwsl_header("%s: token %d ah->pos = %d, ah->nfrag = %d\n",
		   __func__, hdr_token_idx, ah->pos, ah->nfrag);

	if (!hdr_token_idx) {
		lwsl_err("%s: zero hdr_token_idx\n", __func__);
		return 1;
	}

	if (ah->nfrag >= LWS_ARRAY_SIZE(ah->frag_index)) {
		lwsl_err("%s: frag index %d too big\n", __func__, ah->nfrag);
		return 1;
	}

	if ((hdr_token_idx == WSI_TOKEN_HTTP_COLON_AUTHORITY ||
	     hdr_token_idx == WSI_TOKEN_HTTP_COLON_METHOD ||
	     hdr_token_idx == WSI_TOKEN_HTTP_COLON_PATH ||
	     hdr_token_idx == WSI_TOKEN_COLON_PROTOCOL ||
	     hdr_token_idx == WSI_TOKEN_HTTP_COLON_SCHEME) &&
	     ah->frag_index[hdr_token_idx]) {
		if (!(ah->frags[ah->frag_index[hdr_token_idx]].flags & 1)) {
			lws_h2_goaway(lws_get_network_wsi(wsi),
				      H2_ERR_PROTOCOL_ERROR,
				      "Duplicated pseudoheader");
			return 1;
		}
	}

	if (ah->nfrag == 0)
		ah->nfrag = 1;

	ah->frags[ah->nfrag].offset = ah->pos;
	ah->frags[ah->nfrag].len = 0;
	ah->frags[ah->nfrag].nfrag = 0;
	ah->frags[ah->nfrag].flags = 2; /* we had reason to set it */

	ah->hdr_token_idx = hdr_token_idx;

	/*
	 * Okay, but we could be, eg, the second or subsequent cookie: header
	 */

	if (ah->frag_index[hdr_token_idx]) {
		int n;

		/* find the last fragment for this header... */
		n = ah->frag_index[hdr_token_idx];
		while (ah->frags[n].nfrag)
			n = ah->frags[n].nfrag;
		/* and point it to continue in our continuation fragment */
		ah->frags[n].nfrag = ah->nfrag;
	} else
		ah->frag_index[hdr_token_idx] = ah->nfrag;

	return 0;
}

static int lws_frag_append(struct lws *wsi, unsigned char c)
{
	struct allocated_headers *ah = wsi->http.ah;

	ah->data[ah->pos++] = (char)c;
	ah->frags[ah->nfrag].len++;

	return (unsigned int)ah->pos >= wsi->a.context->max_http_header_data;
}

static int lws_frag_end(struct lws *wsi)
{
	lwsl_header("%s\n", __func__);
	if (lws_frag_append(wsi, 0))
		return 1;

	/* don't account for the terminating NUL in the logical length */
	wsi->http.ah->frags[wsi->http.ah->nfrag].len--;

	wsi->http.ah->nfrag++;
	return 0;
}

int
lws_hdr_extant(struct lws *wsi, enum lws_token_indexes h)
{
	struct allocated_headers *ah = wsi->http.ah;
	int n;

	if (!ah)
		return 0;

	n = ah->frag_index[h];
	if (!n)
		return 0;

	return !!(ah->frags[n].flags & 2);
}

static void lws_dump_header(struct lws *wsi, int hdr)
{
	char s[200];
	const unsigned char *p;
	int len;

	if (hdr == LWS_HPACK_IGNORE_ENTRY) {
		lwsl_notice("hdr tok ignored\n");
		return;
	}

	(void)p;

	len = lws_hdr_copy(wsi, s, sizeof(s) - 1, (enum lws_token_indexes)hdr);
	if (len < 0)
		strcpy(s, "(too big to show)");
	else
		s[len] = '\0';
#if defined(_DEBUG)
	p = lws_token_to_string((enum lws_token_indexes)hdr);
	lwsl_header("  hdr tok %d (%s) = '%s' (len %d)\n", hdr,
		   p ? (char *)p : (char *)"null", s, len);
#endif
}

/*
 * dynamic table
 *
 *  [ 0 ....   num_entries - 1]
 *
 *  Starts filling at 0+
 *
 *  #62 is *most recently entered*
 *
 *  Number of entries is not restricted, but aggregated size of the entry
 *  payloads is.  Unfortunately the way HPACK does this is specific to an
 *  imagined implementation, and lws implementation is much more efficient
 *  (ignoring unknown headers and using the lws token index for the header
 *  name part).
 */

/*
 * returns 0 if dynamic entry (arg and len are filled)
 * returns -1 if failure
 * returns nonzero token index if actually static token
 */
static int
lws_token_from_index(struct lws *wsi, int index, const char **arg, int *len,
		     uint32_t *hdr_len)
{
	struct hpack_dynamic_table *dyn;

	if (index == LWS_HPACK_IGNORE_ENTRY)
		return LWS_HPACK_IGNORE_ENTRY;

	/* dynamic table only belongs to network wsi */
	wsi = lws_get_network_wsi(wsi);
	if (!wsi->h2.h2n)
		return -1;

	dyn = &wsi->h2.h2n->hpack_dyn_table;

	if (index < 0)
		return -1;

	if (index < (int)LWS_ARRAY_SIZE(static_token)) {
		if (arg && index < (int)LWS_ARRAY_SIZE(http2_canned)) {
			*arg = http2_canned[index];
			*len = (int)strlen(http2_canned[index]);
		}
		if (hdr_len)
			*hdr_len = static_hdr_len[index];

		return static_token[index];
	}

	if (!dyn) {
		lwsl_notice("no dynamic table\n");
		return -1;
	}

	if (index >= (int)LWS_ARRAY_SIZE(static_token) + dyn->used_entries) {
		lwsl_info("  %s: adjusted index %d >= %d\n", __func__, index,
				(int)LWS_ARRAY_SIZE(static_token) + dyn->used_entries);
		lws_h2_goaway(wsi, H2_ERR_COMPRESSION_ERROR,
			      "index out of range");
		return -1;
	}

	index -= (int)LWS_ARRAY_SIZE(static_token);
	index = lws_safe_modulo(dyn->pos - 1 - index, dyn->num_entries);
	if (index < 0)
		index += dyn->num_entries;

	lwsl_header("%s: dyn index %d, tok %d\n", __func__, index,
		    dyn->entries[index].lws_hdr_idx);

	if (arg && len) {
		*arg = dyn->entries[index].value;
		*len = dyn->entries[index].value_len;
	}

	if (hdr_len)
		*hdr_len = dyn->entries[index].hdr_len;

	return dyn->entries[index].lws_hdr_idx;
}

static int
lws_h2_dynamic_table_dump(struct lws *wsi)
{
#if 0
	struct lws *nwsi = lws_get_network_wsi(wsi);
	struct hpack_dynamic_table *dyn;
	int n, m;
	const char *p;

	if (!nwsi->h2.h2n)
		return 1;
	dyn = &nwsi->h2.h2n->hpack_dyn_table;

	lwsl_header("Dump dyn table for nwsi %s (%d / %d members, pos = %d, "
		    "start index %d, virt used %d / %d)\n", lws_wsi_tag(nwsi),
		    dyn->used_entries, dyn->num_entries, dyn->pos,
		    (uint32_t)LWS_ARRAY_SIZE(static_token),
		    dyn->virtual_payload_usage, dyn->virtual_payload_max);

	for (n = 0; n < dyn->used_entries; n++) {
		m = lws_safe_modulo(dyn->pos - 1 - n, dyn->num_entries);
		if (m < 0)
			m += dyn->num_entries;
		if (dyn->entries[m].lws_hdr_idx != LWS_HPACK_IGNORE_ENTRY)
			p = (const char *)lws_token_to_string(
					dyn->entries[m].lws_hdr_idx);
		else
			p = "(ignored)";
		lwsl_header("   %3d: tok %s: (len %d) val '%s'\n",
			    (int)(n + LWS_ARRAY_SIZE(static_token)), p,
			    dyn->entries[m].hdr_len, dyn->entries[m].value ?
			    dyn->entries[m].value : "null");
	}
#endif
	return 0;
}

static void
lws_dynamic_free(struct hpack_dynamic_table *dyn, int idx)
{
	lwsl_header("freeing %d for reuse\n", idx);
	dyn->virtual_payload_usage = (uint32_t)((unsigned int)dyn->virtual_payload_usage - (unsigned int)(dyn->entries[idx].value_len +
				dyn->entries[idx].hdr_len));
	lws_free_set_NULL(dyn->entries[idx].value);
	dyn->entries[idx].value = NULL;
	dyn->entries[idx].value_len = 0;
	dyn->entries[idx].hdr_len = 0;
	dyn->entries[idx].lws_hdr_idx = LWS_HPACK_IGNORE_ENTRY;
	dyn->used_entries--;
}

/*
 * There are two address spaces, 1) internal ringbuffer and 2) HPACK indexes.
 *
 * Internal ringbuffer:
 *
 * The internal ringbuffer wraps as we keep filling it, dyn->pos points to
 * the next index to be written.
 *
 * HPACK indexes:
 *
 * The last-written entry becomes entry 0, the previously-last-written entry
 * becomes entry 1 etc.
 */

static int
lws_dynamic_token_insert(struct lws *wsi, int hdr_len,
			 int lws_hdr_index, char *arg, size_t len)
{
	struct hpack_dynamic_table *dyn;
	int new_index;

	/* dynamic table only belongs to network wsi */
	wsi = lws_get_network_wsi(wsi);
	if (!wsi->h2.h2n)
		return 1;
	dyn = &wsi->h2.h2n->hpack_dyn_table;

	if (!dyn->entries) {
		lwsl_err("%s: unsized dyn table\n", __func__);

		return 1;
	}
	lws_h2_dynamic_table_dump(wsi);

	new_index = lws_safe_modulo(dyn->pos, dyn->num_entries);
	if (dyn->num_entries && dyn->used_entries == dyn->num_entries) {
		if (dyn->virtual_payload_usage < dyn->virtual_payload_max)
			lwsl_err("Dropping header content before limit!\n");
		/* we have to drop the oldest to make space */
		lws_dynamic_free(dyn, new_index);
	}

	/*
	 * evict guys to make room, allowing for some overage.  We have to
	 * take care about getting a single huge header, and evicting
	 * everything
	 */

	while (dyn->virtual_payload_usage &&
	       dyn->used_entries &&
	       dyn->virtual_payload_usage + (unsigned int)hdr_len + len >
				dyn->virtual_payload_max + 1024) {
		int n = lws_safe_modulo(dyn->pos - dyn->used_entries,
						dyn->num_entries);
		if (n < 0)
			n += dyn->num_entries;
		lws_dynamic_free(dyn, n);
	}

	if (dyn->used_entries < dyn->num_entries)
		dyn->used_entries++;

	dyn->entries[new_index].value_len = 0;

	if (lws_hdr_index != LWS_HPACK_IGNORE_ENTRY) {
		if (dyn->entries[new_index].value)
			lws_free_set_NULL(dyn->entries[new_index].value);
		dyn->entries[new_index].value =
				lws_malloc(len + 1, "hpack dyn");
		if (!dyn->entries[new_index].value)
			return 1;

		memcpy(dyn->entries[new_index].value, arg, len);
		dyn->entries[new_index].value[len] = '\0';
		dyn->entries[new_index].value_len = (uint16_t)len;
	} else
		dyn->entries[new_index].value = NULL;

	dyn->entries[new_index].lws_hdr_idx = (uint16_t)lws_hdr_index;
	dyn->entries[new_index].hdr_len = (uint16_t)hdr_len;

	dyn->virtual_payload_usage = (uint32_t)(dyn->virtual_payload_usage +
					(unsigned int)hdr_len + len);

	lwsl_info("%s: index %ld: lws_hdr_index 0x%x, hdr len %d, '%s' len %d\n",
		  __func__, (long)LWS_ARRAY_SIZE(static_token),
		  lws_hdr_index, hdr_len, dyn->entries[new_index].value ?
				 dyn->entries[new_index].value : "null", (int)len);

	dyn->pos = (uint16_t)lws_safe_modulo(dyn->pos + 1, dyn->num_entries);

	lws_h2_dynamic_table_dump(wsi);

	return 0;
}

int
lws_hpack_dynamic_size(struct lws *wsi, int size)
{
	struct hpack_dynamic_table *dyn;
	struct hpack_dt_entry *dte;
	struct lws *nwsi;
	int min, n = 0, m;

	/*
	 * "size" here is coming from the http/2 SETTING
	 * SETTINGS_HEADER_TABLE_SIZE.  This is a (virtual, in our case)
	 * linear buffer containing dynamic header names and values... when it
	 * is full, old entries are evicted.
	 *
	 * We encode the header as an lws_hdr_idx, which is all the rest of
	 * lws cares about; if there is no matching header we store an empty
	 * entry in the dyn table as a placeholder.
	 *
	 * So to make the two systems work together we keep an accounting of
	 * what we are using to decide when to evict... we must only evict
	 * things when the remote peer's accounting also makes him feel he
	 * should evict something.
	 */

	nwsi = lws_get_network_wsi(wsi);
	if (!nwsi->h2.h2n)
		goto bail;

	dyn = &nwsi->h2.h2n->hpack_dyn_table;
	lwsl_info("%s: from %d to %d, lim %u\n", __func__,
		  (int)dyn->num_entries, size,
		  (unsigned int)nwsi->a.vhost->h2.set.s[H2SET_HEADER_TABLE_SIZE]);

	if (!size) {
		size = dyn->num_entries * 8;
		lws_hpack_destroy_dynamic_header(wsi);
	}

	if (size > (int)nwsi->a.vhost->h2.set.s[H2SET_HEADER_TABLE_SIZE]) {
		lwsl_info("rejecting hpack dyn size %u vs %u\n", size,
			  (unsigned int)nwsi->a.vhost->h2.set.s[H2SET_HEADER_TABLE_SIZE]);

		// this seems necessary to work with some browsers

		if (nwsi->a.vhost->h2.set.s[H2SET_HEADER_TABLE_SIZE] == 65536 &&
				size == 65537) { /* h2spec */
			lws_h2_goaway(nwsi, H2_ERR_COMPRESSION_ERROR,
				  "Asked for header table bigger than we told");
			goto bail;
		}

		size = (int)nwsi->a.vhost->h2.set.s[H2SET_HEADER_TABLE_SIZE];
	}

	dyn->virtual_payload_max = (uint32_t)size;

	size = size / 8;
	min = size;
	if (min > dyn->used_entries)
		min = dyn->used_entries;

	if (size == dyn->num_entries)
		return 0;

	if (dyn->num_entries < min)
		min = dyn->num_entries;

	// lwsl_notice("dte requested size %d\n", size);

	dte = lws_zalloc(sizeof(*dte) * (unsigned int)(size + 1), "dynamic table entries");
	if (!dte)
		goto bail;

	while (dyn->virtual_payload_usage && dyn->used_entries &&
	       dyn->virtual_payload_usage > dyn->virtual_payload_max) {
		n = lws_safe_modulo(dyn->pos - dyn->used_entries, dyn->num_entries);
		if (n < 0)
			n += dyn->num_entries;
		lws_dynamic_free(dyn, n);
	}

	if (min > dyn->used_entries)
		min = dyn->used_entries;

	if (dyn->entries) {
		for (n = 0; n < min; n++) {
			m = (dyn->pos - dyn->used_entries + n) %
						dyn->num_entries;
			if (m < 0)
				m += dyn->num_entries;
			dte[n] = dyn->entries[m];
		}

		lws_free(dyn->entries);
	}

	dyn->entries = dte;
	dyn->num_entries = (uint16_t)size;
	dyn->used_entries = (uint16_t)min;
	if (size)
		dyn->pos = (uint16_t)lws_safe_modulo(min, size);
	else
		dyn->pos = 0;

	lws_h2_dynamic_table_dump(wsi);

	return 0;

bail:
	lwsl_info("%s: failed to resize to %d\n", __func__, size);

	return 1;
}

void
lws_hpack_destroy_dynamic_header(struct lws *wsi)
{
	struct hpack_dynamic_table *dyn;
	int n;

	if (!wsi->h2.h2n)
		return;

	dyn = &wsi->h2.h2n->hpack_dyn_table;

	if (!dyn->entries)
		return;

	for (n = 0; n < dyn->num_entries; n++)
		if (dyn->entries[n].value)
			lws_free_set_NULL(dyn->entries[n].value);

	lws_free_set_NULL(dyn->entries);
}

static int
lws_hpack_use_idx_hdr(struct lws *wsi, int idx, int known_token)
{
	const char *arg = NULL;
	int len = 0;
	const char *p = NULL;
	int tok = lws_token_from_index(wsi, idx, &arg, &len, NULL);

	if (tok == LWS_HPACK_IGNORE_ENTRY) {
		lwsl_header("%s: lws_token says ignore, returning\n", __func__);
		return 0;
	}

	if (tok == -1) {
		lwsl_info("%s: idx %d mapped to tok %d\n", __func__, idx, tok);
		return 1;
	}

	if (arg) {
		/* dynamic result */
		if (known_token > 0)
			tok = known_token;
		lwsl_header("%s: dyn: idx %d '%s' tok %d\n", __func__, idx, arg,
			   tok);
	} else
		lwsl_header("writing indexed hdr %d (tok %d '%s')\n", idx, tok,
				lws_token_to_string((enum lws_token_indexes)tok));

	if (tok == LWS_HPACK_IGNORE_ENTRY)
		return 0;

	if (arg)
		p = arg;

	if (idx < (int)LWS_ARRAY_SIZE(http2_canned))
		p = http2_canned[idx];

	if (lws_frag_start(wsi, tok))
		return 1;

	if (p)
		while (*p && len--)
			if (lws_frag_append(wsi, (unsigned char)*p++))
				return 1;

	if (lws_frag_end(wsi))
		return 1;

	lws_dump_header(wsi, tok);

	return 0;
}

#if !defined(LWS_HTTP_HEADERS_ALL) && !defined(LWS_WITH_HTTP_UNCOMMON_HEADERS) && !defined(LWS_ROLE_WS) && !defined(LWS_ROLE_H2)
static uint8_t lws_header_implies_psuedoheader_map[] = {
	0x03,0x00,0x00,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
};
#endif
#if !defined(LWS_HTTP_HEADERS_ALL) &&  defined(LWS_WITH_HTTP_UNCOMMON_HEADERS) && !defined(LWS_ROLE_WS) && !defined(LWS_ROLE_H2)
static uint8_t lws_header_implies_psuedoheader_map[] = {
	0x07,0x00,0x00,0x00,0x00,0x00,0x00,0x0e,0x04,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
};
#endif
#if !defined(LWS_HTTP_HEADERS_ALL) && !defined(LWS_WITH_HTTP_UNCOMMON_HEADERS) &&  defined(LWS_ROLE_WS) && !defined(LWS_ROLE_H2)
static uint8_t lws_header_implies_psuedoheader_map[] = {
	0x03,0x00,0x00,0x00,0x00,0x00,0x00,0x04,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
};
#endif
#if !defined(LWS_HTTP_HEADERS_ALL) &&  defined(LWS_WITH_HTTP_UNCOMMON_HEADERS) &&  defined(LWS_ROLE_WS) && !defined(LWS_ROLE_H2)
static uint8_t lws_header_implies_psuedoheader_map[] = {
	0x07,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x38,0x10,0x00,0x00,0x00,0x00,0x00,0x00,
};
#endif
#if !defined(LWS_HTTP_HEADERS_ALL) && !defined(LWS_WITH_HTTP_UNCOMMON_HEADERS) && !defined(LWS_ROLE_WS) &&  defined(LWS_ROLE_H2)
static uint8_t lws_header_implies_psuedoheader_map[] = {
	0x03,0x00,0x80,0x0f,0x00,0x00,0x00,0x00,0x12,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
};
#endif
#if !defined(LWS_HTTP_HEADERS_ALL) &&  defined(LWS_WITH_HTTP_UNCOMMON_HEADERS) && !defined(LWS_ROLE_WS) &&  defined(LWS_ROLE_H2)
static uint8_t lws_header_implies_psuedoheader_map[] = {
	0x07,0x00,0x00,0x3e,0x00,0x00,0x00,0x80,0x03,0x09,0x00,0x00,0x00,0x00,0x00,0x00,
};
#endif
#if !defined(LWS_HTTP_HEADERS_ALL) && !defined(LWS_WITH_HTTP_UNCOMMON_HEADERS) &&  defined(LWS_ROLE_WS) &&  defined(LWS_ROLE_H2)
static uint8_t lws_header_implies_psuedoheader_map[] = {
	0x03,0x00,0x00,0x00,0x3e,0x00,0x00,0x00,0x00,0x48,0x00,0x00,0x00,0x00,0x00,0x00,
};
#endif
#if defined(LWS_HTTP_HEADERS_ALL) || ( defined(LWS_WITH_HTTP_UNCOMMON_HEADERS) &&  defined(LWS_ROLE_WS) &&  defined(LWS_ROLE_H2))
static uint8_t lws_header_implies_psuedoheader_map[] = {
	0x07,0x00,0x00,0x00,0xf8,0x00,0x00,0x00,0x00,0x0e,0x24,0x00,0x00,0x00,0x00,0x00,
};
#endif


static int
lws_hpack_handle_pseudo_rules(struct lws *nwsi, struct lws *wsi, int m)
{
	if (m == LWS_HPACK_IGNORE_ENTRY || m == -1)
		return 0;

	if (wsi->seen_nonpseudoheader &&
	    (lws_header_implies_psuedoheader_map[m >> 3] & (1 << (m & 7)))) {

		lwsl_info("lws tok %d seems to be a pseudoheader\n", m);

		/*
		 * it's not legal to see a
		 * pseudoheader after normal
		 * headers
		 */
		lws_h2_goaway(nwsi, H2_ERR_PROTOCOL_ERROR,
			"Pseudoheader after normal hdrs");
		return 1;
	}

	if (!(lws_header_implies_psuedoheader_map[m >> 3] & (1 << (m & 7))))
		wsi->seen_nonpseudoheader = 1;

	return 0;
}

int lws_hpack_interpret(struct lws *wsi, unsigned char c)
{
	struct lws *nwsi = lws_get_network_wsi(wsi);
	struct lws_h2_netconn *h2n = nwsi->h2.h2n;
	struct allocated_headers *ah = wsi->http.ah;
	unsigned int prev;
	unsigned char c1;
	int n, m, plen;

	if (!h2n)
		return -1;

	/*
	 * HPKT_INDEXED_HDR_7		  1xxxxxxx: just "header field"
	 * HPKT_INDEXED_HDR_6_VALUE_INCR  01xxxxxx: NEW indexed hdr + val
	 * HPKT_LITERAL_HDR_VALUE_INCR	  01000000: NEW literal hdr + val
	 * HPKT_INDEXED_HDR_4_VALUE	  0000xxxx: indexed hdr + val
	 * HPKT_INDEXED_HDR_4_VALUE_NEVER 0001xxxx: NEVER NEW indexed hdr + val
	 * HPKT_LITERAL_HDR_VALUE	  00000000: literal hdr + val
	 * HPKT_LITERAL_HDR_VALUE_NEVER	  00010000: NEVER NEW literal hdr + val
	 */
	switch (h2n->hpack) {

	case HPKS_TYPE:
		h2n->is_first_header_char = 1;
		h2n->huff_pad = 0;
		h2n->zero_huff_padding = 0;
		h2n->last_action_dyntable_resize = 0;
		h2n->ext_count = 0;
		h2n->hpack_hdr_len = 0;
		h2n->unknown_header = 0;
		ah->parser_state = 255;

		if (c & 0x80) { /* 1....  indexed header field only */
			/* just a possibly-extended integer */
			h2n->hpack_type = HPKT_INDEXED_HDR_7;
			lwsl_header("HPKT_INDEXED_HDR_7 hdr %d\n", c & 0x7f);
			lws_h2_dynamic_table_dump(wsi);

			h2n->hdr_idx = c & 0x7f;
			if ((c & 0x7f) == 0x7f) {
				h2n->hpack_len = 0;
				h2n->hpack_m = 0x7f;
				h2n->hpack = HPKS_IDX_EXT;
				break;
			}
			if (!h2n->hdr_idx) {
				lws_h2_goaway(nwsi, H2_ERR_COMPRESSION_ERROR,
					      "hdr index 0 seen");
					return 1;
			}

			m = lws_token_from_index(wsi, (int)h2n->hdr_idx,
						 NULL, NULL, NULL);
			if (lws_hpack_handle_pseudo_rules(nwsi, wsi, m))
				return 1;

			lwsl_header("HPKT_INDEXED_HDR_7: hdr %d\n", c & 0x7f);
			if (lws_hpack_use_idx_hdr(wsi, c & 0x7f, -1)) {
				lwsl_header("%s: idx hdr wr fail\n", __func__);
				return 1;
			}
			/* stay at same state */
			break;
		}
		if (c & 0x40) { /* 01.... indexed or literal header incr idx */
			/*
			 * [possibly-ext hdr idx (6) | new literal hdr name]
			 * H + possibly-ext value length
			 * literal value
			 */
			h2n->hdr_idx = 0;
			if (c == 0x40) { /* literal header */
				lwsl_header("   HPKT_LITERAL_HDR_VALUE_INCR\n");
				h2n->hpack_type = HPKT_LITERAL_HDR_VALUE_INCR;
				h2n->value = 0;
				h2n->hpack_len = 0;
				h2n->hpack = HPKS_HLEN;
				break;
			}
			/* indexed header */
			h2n->hpack_type = HPKT_INDEXED_HDR_6_VALUE_INCR;
			lwsl_header(" HPKT_INDEXED_HDR_6_VALUE_INCR (hdr %d)\n",
				   c & 0x3f);
			h2n->hdr_idx = c & 0x3f;
			if ((c & 0x3f) == 0x3f) {
				h2n->hpack_m = 0x3f;
				h2n->hpack_len = 0;
				h2n->hpack = HPKS_IDX_EXT;
				break;
			}

			h2n->value = 1;
			h2n->hpack = HPKS_HLEN;
			if (!h2n->hdr_idx) {
				lws_h2_goaway(nwsi, H2_ERR_COMPRESSION_ERROR,
					      "hdr index 0 seen");
					return 1;
			}
			break;
		}
		switch(c & 0xf0) {
		case 0x10: /* literal header never index */
		case 0:    /* literal header without indexing */
			/*
			 * follows 0x40 except 4-bit hdr idx
			 * and don't add to index
			 */
			if (c == 0) { /* literal name */
				h2n->hpack_type = HPKT_LITERAL_HDR_VALUE;
				lwsl_header("   HPKT_LITERAL_HDR_VALUE\n");
				h2n->hpack = HPKS_HLEN;
				h2n->value = 0;
				break;
			}
			if (c == 0x10) { /* literal name NEVER */
				h2n->hpack_type = HPKT_LITERAL_HDR_VALUE_NEVER;
				lwsl_header("  HPKT_LITERAL_HDR_VALUE_NEVER\n");
				h2n->hpack = HPKS_HLEN;
				h2n->value = 0;
				break;
			}
			lwsl_header("indexed\n");
			/* indexed name */
			if (c & 0x10) {
				h2n->hpack_type = HPKT_INDEXED_HDR_4_VALUE_NEVER;
				lwsl_header("HPKT_LITERAL_HDR_4_VALUE_NEVER\n");
			} else {
				h2n->hpack_type = HPKT_INDEXED_HDR_4_VALUE;
				lwsl_header("   HPKT_INDEXED_HDR_4_VALUE\n");
			}
			h2n->hdr_idx = 0;
			if ((c & 0xf) == 0xf) {
				h2n->hpack_len = c & 0xf;
				h2n->hpack_m = 0xf;
				h2n->hpack_len = 0;
				h2n->hpack = HPKS_IDX_EXT;
				break;
			}
			h2n->hdr_idx = c & 0xf;
			h2n->value = 1;
			h2n->hpack = HPKS_HLEN;
			break;

		case 0x20:
		case 0x30: /* header table size update */
			/* possibly-extended size value (5) */
			lwsl_header("HPKT_SIZE_5 %x\n", c &0x1f);
			h2n->hpack_type = HPKT_SIZE_5;
			h2n->hpack_len = c & 0x1f;
			if (h2n->hpack_len == 0x1f) {
				h2n->hpack_m = 0x1f;
				h2n->hpack_len = 0;
				h2n->hpack = HPKS_IDX_EXT;
				break;
			}
			h2n->last_action_dyntable_resize = 1;
			if (lws_hpack_dynamic_size(wsi, (int)h2n->hpack_len))
				return 1;
			break;
		}
		break;

	case HPKS_IDX_EXT:
		h2n->hpack_len = (uint32_t)((unsigned int)h2n->hpack_len |
				(unsigned int)((c & 0x7f) << h2n->ext_count));
		h2n->ext_count = (uint8_t)(h2n->ext_count + 7);
		if (c & 0x80) /* extended int not complete yet */
			break;

		/* extended integer done */
		h2n->hpack_len += h2n->hpack_m;
		lwsl_header("HPKS_IDX_EXT: hpack_len %u\n", (unsigned int)h2n->hpack_len);

		switch (h2n->hpack_type) {
		case HPKT_INDEXED_HDR_7:
			if (lws_hpack_use_idx_hdr(wsi, (int)h2n->hpack_len,
						  (int)h2n->hdr_idx)) {
				lwsl_notice("%s: hd7 use fail\n", __func__);
				return 1;
			}
			h2n->hpack = HPKS_TYPE;
			break;

		case HPKT_SIZE_5:
			h2n->last_action_dyntable_resize = 1;
			if (lws_hpack_dynamic_size(wsi, (int)h2n->hpack_len))
				return 1;
			h2n->hpack = HPKS_TYPE;
			break;

		default:
			h2n->hdr_idx = h2n->hpack_len;
			if (!h2n->hdr_idx) {
				lws_h2_goaway(nwsi, H2_ERR_COMPRESSION_ERROR,
					      "extended header index was 0");
				return 1;
			}
			h2n->value = 1;
			h2n->hpack = HPKS_HLEN;
			break;
		}
		break;

	case HPKS_HLEN: /* [ H | 7+ ] */
		h2n->huff = !!(c & 0x80);
		h2n->hpack_pos = 0;
		h2n->hpack_len = c & 0x7f;

		if (h2n->hpack_len == 0x7f) {
			h2n->hpack_m = 0x7f;
			h2n->hpack_len = 0;
			h2n->ext_count = 0;
			h2n->hpack = HPKS_HLEN_EXT;
			break;
		}

		if (h2n->value && !h2n->hpack_len) {
			lwsl_debug("%s: zero-length header data\n", __func__);
			h2n->hpack = HPKS_TYPE;
			goto fin;
		}

pre_data:
		h2n->hpack = HPKS_DATA;
		if (!h2n->value || !h2n->hdr_idx) {
			ah->parser_state = WSI_TOKEN_NAME_PART;
			ah->lextable_pos = 0;
			h2n->unknown_header = 0;
			break;
		}

		if (h2n->hpack_type == HPKT_LITERAL_HDR_VALUE ||
		    h2n->hpack_type == HPKT_LITERAL_HDR_VALUE_INCR ||
		    h2n->hpack_type == HPKT_LITERAL_HDR_VALUE_NEVER) {
			n = ah->parser_state;
			if (n == 255) {
				n = -1;
				h2n->hdr_idx = (uint32_t)-1;
			} else
				h2n->hdr_idx = 1;
		} else {
			n = lws_token_from_index(wsi, (int)h2n->hdr_idx, NULL,
						 NULL, NULL);
			lwsl_header("  lws_tok_from_idx(%u) says %d\n",
				   (unsigned int)h2n->hdr_idx, n);
		}

		if (n == LWS_HPACK_IGNORE_ENTRY || n == -1)
			h2n->hdr_idx = LWS_HPACK_IGNORE_ENTRY;

		switch (h2n->hpack_type) {
		/*
		 * hpack types with literal headers were parsed by the lws
		 * header SM... on recognition of a known lws header, it does
		 * the correct lws_frag_start() for us already.  Other types
		 * (ie, indexed header) need us to do it here.
		 */
		case HPKT_LITERAL_HDR_VALUE_INCR:
		case HPKT_LITERAL_HDR_VALUE:
		case HPKT_LITERAL_HDR_VALUE_NEVER:
			break;
		default:
			if (n != -1 && n != LWS_HPACK_IGNORE_ENTRY &&
			    lws_frag_start(wsi, n)) {
				lwsl_header("%s: frag start failed\n",
					    __func__);
				return 1;
			}
			break;
		}
		break;

	case HPKS_HLEN_EXT:
		h2n->hpack_len = (uint32_t)((unsigned int)h2n->hpack_len |
				(unsigned int)((c & 0x7f) << h2n->ext_count));
		h2n->ext_count = (uint8_t)(h2n->ext_count + 7);
		if (c & 0x80) /* extended integer not complete yet */
			break;

		h2n->hpack_len += h2n->hpack_m;
		goto pre_data;

	case HPKS_DATA:
		//lwsl_header(" 0x%02X huff %d\n", c, h2n->huff);
			c1 = c;

		for (n = 0; n < 8; n++) {
			if (h2n->huff) {
				char b = (c >> 7) & 1;
				prev = h2n->hpack_pos;
				h2n->hpack_pos = (uint16_t)huftable_decode(
						(int)h2n->hpack_pos, b);
				c = (unsigned char)(c << 1);
				if (h2n->hpack_pos == 0xffff) {
					lwsl_notice("Huffman err\n");
					return 1;
				}
				if (!(h2n->hpack_pos & 0x8000)) {
					if (!b)
						h2n->zero_huff_padding = 1;
					h2n->huff_pad++;
					continue;
				}
				c1 = (uint8_t)(h2n->hpack_pos & 0x7fff);
				h2n->hpack_pos = 0;
				h2n->huff_pad = 0;
				h2n->zero_huff_padding = 0;

				/* EOS |11111111|11111111|11111111|111111 */
				if (!c1 && prev == HUFTABLE_0x100_PREV) {
					lws_h2_goaway(nwsi,
						H2_ERR_COMPRESSION_ERROR,
						"Huffman EOT seen");
					return 1;
				}
			} else
				n = 8;

			if (h2n->value) { /* value */

				if (h2n->hdr_idx &&
				    h2n->hdr_idx != LWS_HPACK_IGNORE_ENTRY) {

					if (ah->hdr_token_idx ==
					    WSI_TOKEN_HTTP_COLON_PATH) {

						switch (lws_parse_urldecode(
								    wsi, &c1)) {
						case LPUR_CONTINUE:
							break;
						case LPUR_SWALLOW:
							goto swallow;
						case LPUR_EXCESSIVE:
						case LPUR_FORBID:
							lws_h2_goaway(nwsi,
							  H2_ERR_PROTOCOL_ERROR,
							  "Evil URI");
							return 1;

						default:
							return -1;
						}
					}
					if (lws_frag_append(wsi, c1)) {
						lwsl_notice(
							"%s: frag app fail\n",
							    __func__);
						return 1;
					}
				} //else
					//lwsl_header("ignoring %c\n", c1);
			} else {
				/*
				 * Convert name using existing parser,
			 	 * If h2n->unknown_header == 0, result is
				 * in wsi->parser_state
			 	 * using WSI_TOKEN_GET_URI.
			 	 *
			 	 * If unknown header h2n->unknown_header
			 	 * will be set.
			 	 */
				h2n->hpack_hdr_len++;
				if (h2n->is_first_header_char) {
					h2n->is_first_header_char = 0;
					h2n->first_hdr_char = (char)c1;
				}
				lwsl_header("parser: %c\n", c1);
				/* uppercase header names illegal */
				if (c1 >= 'A' && c1 <= 'Z') {
					lws_h2_goaway(nwsi,
						H2_ERR_COMPRESSION_ERROR,
						"Uppercase literal hpack hdr");
					return 1;
				}
				plen = 1;
				if (!h2n->unknown_header &&
				    lws_parse(wsi, &c1, &plen))
					h2n->unknown_header = 1;
			}
swallow:
			(void)n;
		} // for n

		if (--h2n->hpack_len)
			break;

		/*
		 * The header (h2n->value = 0) or the payload (h2n->value = 1)
		 * is complete.
		 */

		if (h2n->huff && (h2n->huff_pad > 7 ||
		    (h2n->zero_huff_padding && h2n->huff_pad))) {
			lwsl_info("zero_huff_padding: %d huff_pad: %d\n",
				    h2n->zero_huff_padding, h2n->huff_pad);
			lws_h2_goaway(nwsi, H2_ERR_COMPRESSION_ERROR,
				      "Huffman padding excessive or wrong");
			return 1;
		}
fin:
		if (!h2n->value && (
		    h2n->hpack_type == HPKT_LITERAL_HDR_VALUE ||
		    h2n->hpack_type == HPKT_LITERAL_HDR_VALUE_INCR ||
		    h2n->hpack_type == HPKT_LITERAL_HDR_VALUE_NEVER)) {
			h2n->hdr_idx = LWS_HPACK_IGNORE_ENTRY;
			lwsl_header("wsi->parser_state: %d\n",
					ah->parser_state);

			if (ah->parser_state == WSI_TOKEN_NAME_PART) {
				/* h2 headers come without the colon */
				c1 = ':';
				plen = 1;
				n = lws_parse(wsi, &c1, &plen);
				(void)n;
			}

			if (ah->parser_state == WSI_TOKEN_NAME_PART ||
#if defined(LWS_WITH_CUSTOM_HEADERS)
			    ah->parser_state == WSI_TOKEN_UNKNOWN_VALUE_PART ||
#endif
			    ah->parser_state == WSI_TOKEN_SKIPPING) {
				h2n->unknown_header = 1;
				ah->parser_state = 0xff;
				wsi->seen_nonpseudoheader = 1;
			}
		}

		/* we have the header */
		if (!h2n->value) {
			h2n->value = 1;
			h2n->hpack = HPKS_HLEN;
			h2n->huff_pad = 0;
			h2n->zero_huff_padding = 0;
			h2n->ext_count = 0;
			break;
		}

		/*
		 * we have got both the header and value
		 */

		m = -1;
		switch (h2n->hpack_type) {
		/*
		 * These are the only two that insert to the dyntable
		 */
		/* NEW indexed hdr with value */
		case HPKT_INDEXED_HDR_6_VALUE_INCR:
			/* header length is determined by known index */
			m = lws_token_from_index(wsi, (int)h2n->hdr_idx, NULL, NULL,
					&h2n->hpack_hdr_len);
			if (m < 0)
				/*
				 * The peer may only send known 6-bit indexes,
				 * there's still the possibility it sends an unset
				 * dynamic index that we can't succeed to look up
				 */
				return 1;
			goto add_it;
		/* NEW literal hdr with value */
		case HPKT_LITERAL_HDR_VALUE_INCR:
			/*
			 * hdr is a new literal, so length is already in
			 * h2n->hpack_hdr_len
			 */
			m = ah->parser_state;
			if (h2n->unknown_header ||
			    ah->parser_state == WSI_TOKEN_NAME_PART ||
			    ah->parser_state == WSI_TOKEN_SKIPPING) {
				if (h2n->first_hdr_char == ':') {
					lwsl_info("HPKT_LITERAL_HDR_VALUE_INCR:"
						  " end state %d unk hdr %d\n",
						  ah->parser_state,
						h2n->unknown_header);
					/* unknown pseudoheaders are illegal */
					lws_h2_goaway(nwsi,
						      H2_ERR_PROTOCOL_ERROR,
						      "Unknown pseudoheader");
					return 1;
				}
				m = LWS_HPACK_IGNORE_ENTRY;
			}
add_it:
			/*
			 * mark us as having been set at the time of dynamic
			 * token insertion.
			 */
			ah->frags[ah->nfrag].flags |= 1;

			if (lws_dynamic_token_insert(wsi, (int)h2n->hpack_hdr_len, m,
					&ah->data[ah->frags[ah->nfrag].offset],
					ah->frags[ah->nfrag].len)) {
				lwsl_notice("%s: tok_insert fail\n", __func__);
				return 1;
			}
			break;

		default:
			break;
		}

		if (h2n->hdr_idx != LWS_HPACK_IGNORE_ENTRY && lws_frag_end(wsi))
			return 1;

		if (h2n->hpack_type != HPKT_INDEXED_HDR_6_VALUE_INCR) {

			if (h2n->hpack_type == HPKT_LITERAL_HDR_VALUE ||
			    h2n->hpack_type == HPKT_LITERAL_HDR_VALUE_INCR ||
			    h2n->hpack_type == HPKT_LITERAL_HDR_VALUE_NEVER) {
				m = ah->parser_state;
				if (m == 255)
					m = -1;
			} else
				m = lws_token_from_index(wsi, (int)h2n->hdr_idx,
							 NULL, NULL, NULL);
		}

		if (m != -1 && m != LWS_HPACK_IGNORE_ENTRY)
			lws_dump_header(wsi, m);

		if (lws_hpack_handle_pseudo_rules(nwsi, wsi, m))
			return 1;

		h2n->is_first_header_char = 1;
		h2n->hpack = HPKS_TYPE;
		break;
	}

	return 0;
}



static unsigned int
lws_h2_num_start(int starting_bits, unsigned long num)
{
	unsigned int mask = (unsigned int)((1 << starting_bits) - 1);

	if (num < mask)
		return (unsigned int)num;

	return mask;
}

static int
lws_h2_num(int starting_bits, unsigned long num,
			 unsigned char **p, unsigned char *end)
{
	unsigned int mask = (unsigned int)((1 << starting_bits) - 1);

	if (num < mask)
		return 0;

	num -= mask;
	do {
		if (num > 127)
			*((*p)++) = (uint8_t)(0x80 | (num & 0x7f));
		else
			*((*p)++) = (uint8_t)(0x00 | (num & 0x7f));
		if (*p >= end)
			return 1;
		num >>= 7;
	} while (num);

	return 0;
}

int lws_add_http2_header_by_name(struct lws *wsi, const unsigned char *name,
				 const unsigned char *value, int length,
				 unsigned char **p, unsigned char *end)
{
	int len;

#if defined(_DEBUG)
	/* value does not have to be NUL-terminated... %.*s not available on
	 * all platforms */
	if (value) {
		lws_strnncpy((char *)*p, (const char *)value, length,
				lws_ptr_diff(end, (*p)));

		lwsl_header("%s: %p  %s:%s (len %d)\n", __func__, *p, name,
				(const char *)*p, length);
	} else {
		lwsl_err("%s: %p dummy copy %s (len %d)\n", __func__, *p, name, length);
	}
#endif

	len = (int)strlen((char *)name);
	if (len)
		if (name[len - 1] == ':')
			len--;

	if (wsi->mux_substream && !strncmp((const char *)name,
					     "transfer-encoding", (unsigned int)len)) {
		lwsl_header("rejecting %s\n", name);

		return 0;
	}

	if (end - *p < len + length + 8)
		return 1;

	*((*p)++) = 0; /* literal hdr, literal name,  */

	*((*p)++) = (uint8_t)(0 | (uint8_t)lws_h2_num_start(7, (unsigned long)len)); /* non-HUF */
	if (lws_h2_num(7, (unsigned long)len, p, end))
		return 1;

	/* upper-case header names are verboten in h2, but OK on h1, so
	 * they're not illegal per se.  Silently convert them for h2... */

	while(len--)
		*((*p)++) = (uint8_t)tolower((int)*name++);

	*((*p)++) = (uint8_t)(0 | (uint8_t)lws_h2_num_start(7, (unsigned long)length)); /* non-HUF */
	if (lws_h2_num(7, (unsigned long)length, p, end))
		return 1;

	if (value)
		memcpy(*p, value, (unsigned int)length);
	*p += length;

	return 0;
}

int lws_add_http2_header_by_token(struct lws *wsi, enum lws_token_indexes token,
				  const unsigned char *value, int length,
				  unsigned char **p, unsigned char *end)
{
	const unsigned char *name;

	name = lws_token_to_string(token);
	if (!name)
		return 1;

	return lws_add_http2_header_by_name(wsi, name, value, length, p, end);
}

int lws_add_http2_header_status(struct lws *wsi, unsigned int code,
				unsigned char **p, unsigned char *end)
{
	unsigned char status[10];
	int n;

	wsi->h2.send_END_STREAM = 0; // !!(code >= 400);

	n = sprintf((char *)status, "%u", code);
	if (lws_add_http2_header_by_token(wsi, WSI_TOKEN_HTTP_COLON_STATUS,
					  status, n, p, end))

		return 1;

	return 0;
}
