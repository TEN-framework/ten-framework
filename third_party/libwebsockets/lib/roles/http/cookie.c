
#include <libwebsockets.h>
#include "private-lib-core.h"

//#define LWS_COOKIE_DEBUG

#if defined(LWS_COOKIE_DEBUG)
	#define lwsl_cookie lwsl_notice
#else
	#define lwsl_cookie lwsl_debug
#endif

#define LWS_COOKIE_MAX_CACHE_NAME_LEN	128

#define lws_tolower(_c) (((_c) >= 'A' && (_c) <= 'Z') ? \
					    (char)((_c) + 'a' - 'A') : \
					    (char)(_c))

#define LWS_COOKIE_NSC_FORMAT		  "%.*s\t"\
					  "%s\t"\
					  "%.*s\t"\
					  "%s\t"\
					  "%llu\t"\
					  "%.*s\t"\
					  "%.*s"

static const char *const mon = "janfebmaraprnayjunjulaugsepoctnovdec";

enum lws_cookie_nsc_f {
	LWSC_NSC_DOMAIN,
	LWSC_NSC_HOSTONLY,
	LWSC_NSC_PATH,
	LWSC_NSC_SECURE,
	LWSC_NSC_EXPIRES,
	LWSC_NSC_NAME,
	LWSC_NSC_VALUE,

	LWSC_NSC_COUNT,
};

enum lws_cookie_elements {
	CE_DOMAIN,
	CE_PATH,
	CE_EXPIRES,
	CE_MAXAGE,
	CE_NAME,
	CE_VALUE,

	CE_HOSTONLY, /* these are bool, NULL = 0, non-NULL = 1 */
	CE_SECURE,

	CE_COUNT
};

struct lws_cookie {
	const char	*f[CE_COUNT];
	size_t		l[CE_COUNT];

	unsigned int httponly:1;
};

static int
lws_cookie_parse_date(const char *d, size_t len, time_t *t)
{
	struct tm date;
	int offset = 0, i;

	memset(&date, 0, sizeof(date));

	while (len) {
		if (isalnum((int)*d)) {
			offset++;
			goto next;
		}
		switch (offset) {
		case 2:
			if (*d == ':' && len >= 6) {
				date.tm_hour = atoi(d - 2);
				if (date.tm_hour < 0 || date.tm_hour > 23)
					return -1;
				date.tm_min = atoi(d + 1);
				if (date.tm_min < 0 || date.tm_min > 60)
					return -1;
				date.tm_sec = atoi(d + 4);
				if (date.tm_sec < 0 || date.tm_sec > 61)
					/* leap second */
					return -1;

				d += 6;
				len -= 6;
				offset = 0;
				continue;
			}

			if (!date.tm_mday) {
				date.tm_mday = atoi(d - 2);
				if (date.tm_mday < 1 || date.tm_mday > 31)
					return -1;
				goto next2;
			}

			if (!date.tm_year) {
				date.tm_year = atoi(d - 2);
				if (date.tm_year < 0 || date.tm_year > 99)
					return -1;
				if (date.tm_year < 70)
					date.tm_year += 100;
			}
			goto next2;

		case 3:
			for (i = 0; i < 36; i += 3) {
				if (lws_tolower(*(d - 3)) == mon[i] &&
				    lws_tolower(*(d - 2)) == mon[i + 1] &&
				    lws_tolower(*(d - 1)) == mon[i + 2]) {
					date.tm_mon = i / 3;
					break;
				}
			}
			goto next2;

		case 4:
			if (!date.tm_year) {
				date.tm_year = atoi(d - 4);
				if (date.tm_year < 1601)
					return -1;
				date.tm_year -= 1900;
			}
			goto next2;

		default:
			goto next2;
		}

next2:
		offset = 0;
next:
		d++;
		len--;
	}

	*t = mktime(&date);

	if (*t < 0)
		return -1;

	return 0;
}

static void
lws_cookie_rm_sws(const char **buf_p, size_t *len_p)
{
	const char *buf;
	size_t len;

	if (!buf_p || !*buf_p || !len_p || !*len_p) {
		lwsl_err("%s: false parameter\n", __func__);
		return;
	}

	buf = *buf_p;
	len = *len_p;
	while (buf[0] == ' ' && len > 0) {
		buf++;
		len--;
	}
	while (buf[len - 1] == ' ' && len > 0)
		len--;

	*buf_p = buf;
	*len_p = len;
}

static int
is_iprefix(const char *h, size_t hl, const char *n, size_t nl)
{
	if (!h || !n || nl > hl)
		return 0;

	while (nl) {
		nl--;
		if (lws_tolower(h[nl]) != lws_tolower(n[nl]))
			return 0;
	}
	return 1;
}

static int
lws_cookie_compile_cache_name(char *buf, size_t buf_len, struct lws_cookie *c)
{
	if (!buf || !c->f[CE_DOMAIN] || !c->f[CE_PATH] || !c->f[CE_NAME] ||
	    c->l[CE_DOMAIN] + c->l[CE_PATH] + c->l[CE_NAME] + 6 > buf_len)
		return -1;

	memcpy(buf, c->f[CE_DOMAIN], c->l[CE_DOMAIN]);
	buf += c->l[CE_DOMAIN];
	*buf++ = '|';

	memcpy(buf, c->f[CE_PATH], c->l[CE_PATH]);
	buf += c->l[CE_PATH];
	*buf++ = '|';

	memcpy(buf, c->f[CE_NAME], c->l[CE_NAME]);
	buf += c->l[CE_NAME];
	*buf = '\0';

	return 0;
}

static int
lws_cookie_parse_nsc(struct lws_cookie *c, const char *b, size_t l)
{
	enum lws_cookie_nsc_f state = LWSC_NSC_DOMAIN;
	size_t n = 0;

	if (!c || !b || l < 13)
		return -1;

	memset(c, 0, sizeof(*c));
	lwsl_cookie("%s: parsing (%.*s) \n", __func__, (int)l, b);

	while (l) {
		l--;
		if (b[n] != '\t' && l) {
			n++;
			continue;
		}
		switch (state) {
		case LWSC_NSC_DOMAIN:
			c->f[CE_DOMAIN] = b;
			c->l[CE_DOMAIN] = n;
			break;
		case LWSC_NSC_PATH:
			c->f[CE_PATH] = b;
			c->l[CE_PATH] = n;
			break;
		case LWSC_NSC_EXPIRES:
			c->f[CE_EXPIRES] = b;
			c->l[CE_EXPIRES] = n;
			break;
		case LWSC_NSC_NAME:
			c->f[CE_NAME] = b;
			c->l[CE_NAME] = n;
			break;

		case LWSC_NSC_HOSTONLY:
			if (b[0] == 'T') {
				c->f[CE_HOSTONLY] = b;
				c->l[CE_HOSTONLY] = 1;
			}
			break;
		case LWSC_NSC_SECURE:
			if (b[0] == 'T') {
				c->f[CE_SECURE] = b;
				c->l[CE_SECURE] = 1;
			}
			break;

		case LWSC_NSC_VALUE:
			c->f[CE_VALUE] = b;
			c->l[CE_VALUE] = n + 1;

			for (n = 0; n < LWS_ARRAY_SIZE(c->f); n++)
				lwsl_cookie("%s: %d: %.*s\n", __func__,
						(int)n, (int)c->l[n], c->f[n]);

			return 0;
		default:
			return -1;
		}

		b += n + 1;
		n = 0;
		state++;
	}

	return -1;
}

static int
lws_cookie_write_nsc(struct lws *wsi, struct lws_cookie *c)
{
	char cache_name[LWS_COOKIE_MAX_CACHE_NAME_LEN];
	const char *ads, *path;
	struct lws_cache_ttl_lru *l1;
	struct client_info_stash *stash;
	char *cookie_string = NULL, *dl;
	 /* 6 tabs + 20 for max time_t + 2 * TRUE/FALSE + null */
	size_t size = 6 + 20 + 10 + 1;
	time_t expires = 0;
	int ret = 0;

	if (!wsi || !c)
		return -1;

	l1 = wsi->a.context->l1;
	if (!l1 || !wsi->a.context->nsc)
		return -1;

	stash = wsi->stash ? wsi->stash : lws_get_network_wsi(wsi)->stash;
	if (stash) {
		ads = stash->cis[CIS_ADDRESS];
		path = stash->cis[CIS_PATH];
	} else {
		ads = lws_hdr_simple_ptr(wsi, _WSI_TOKEN_CLIENT_PEER_ADDRESS);
		path = lws_hdr_simple_ptr(wsi, _WSI_TOKEN_CLIENT_URI);
	}
	if (!ads || !path)
		return -1;

	if (!c->f[CE_NAME] || !c->f[CE_VALUE]) {
		lwsl_err("%s: malformed c\n", __func__);

		return -1;
	}

	if (!c->f[CE_EXPIRES]) {
		/*
		 * Currently we just take the approach to reject session cookies
		 */
		lwsl_warn("%s: reject session cookies\n", __func__);

		return 0;
	}

	if (!c->f[CE_DOMAIN]) {
		c->f[CE_HOSTONLY] = "T";
		c->l[CE_HOSTONLY] = 1;
		c->f[CE_DOMAIN] = ads;
		c->l[CE_DOMAIN] = strlen(ads);
	}

	if (!c->f[CE_PATH]) {
		c->f[CE_PATH] = path;
		c->l[CE_PATH] = strlen(path);
		dl = memchr(c->f[CE_PATH], '?', c->l[CE_PATH]);
		if (dl)
			c->l[CE_PATH] = (size_t)(dl - c->f[CE_PATH]);
	}

	if (lws_cookie_compile_cache_name(cache_name, sizeof(cache_name), c))
		return -1;

	if (c->f[CE_EXPIRES] &&
	    lws_cookie_parse_date(c->f[CE_EXPIRES], c->l[CE_EXPIRES], &expires)) {
		lwsl_err("%s: can't parse date %.*s\n", __func__,
			 (int)c->l[CE_EXPIRES], c->f[CE_EXPIRES]);
		return -1;
	}

	size += c->l[CE_NAME] + c->l[CE_VALUE] + c->l[CE_DOMAIN] + c->l[CE_PATH];
	cookie_string = (char *)lws_malloc(size, __func__);
	if (!cookie_string) {
		lwsl_err("%s: OOM\n",__func__);

		return -1;	
	}

	lws_snprintf(cookie_string, size, LWS_COOKIE_NSC_FORMAT,
			(int)c->l[CE_DOMAIN], c->f[CE_DOMAIN],
			c->f[CE_HOSTONLY] ? "TRUE" : "FALSE",
			(int)c->l[CE_PATH], c->f[CE_PATH],
			c->f[CE_SECURE] ? "TRUE" : "FALSE",
			(unsigned long long)expires,
			(int)c->l[CE_NAME], c->f[CE_NAME],
			(int)c->l[CE_VALUE], c->f[CE_VALUE]);

	lwsl_cookie("%s: name %s\n", __func__, cache_name);
	lwsl_cookie("%s: c %s\n", __func__, cookie_string);

	if (lws_cache_write_through(l1, cache_name,
				    (const uint8_t *)cookie_string,
				    strlen(cookie_string),
				    (lws_usec_t)((unsigned long long)expires *
					   (lws_usec_t)LWS_US_PER_SEC), NULL)) {
		ret = -1;
		goto exit;
	}

#if defined(LWS_COOKIE_DEBUG)
	char *po;
	if (lws_cache_item_get(l1, cache_name, (const void **)&po, &size) ||
	    size != strlen(cookie_string) || memcmp(po, cookie_string, size)) {
		lwsl_err("%s: L1 '%s' missing\n", __func__, cache_name);
	}

	if (lws_cache_item_get(wsi->a.context->nsc, cache_name,
			       (const void **)&po, &size) ||
			       size != strlen(cookie_string) ||
			       memcmp(po, cookie_string, size)) {
		lwsl_err("%s: NSC '%s' missing, size %llu, po %s\n", __func__,
			 cache_name, (unsigned long long)size, po);
	}
#endif

exit:
	lws_free(cookie_string);

	return ret;
}

static int
lws_cookie_attach_cookies(struct lws *wsi, char *buf, char *end)
{
	const char *domain, *path, *dl_domain, *dl_path, *po;
	char cache_name[LWS_COOKIE_MAX_CACHE_NAME_LEN];
	size_t domain_len, path_len, size, ret = 0;
	struct lws_cache_ttl_lru *l1;
	struct client_info_stash *stash;
	lws_cache_results_t cr;
	struct lws_cookie c;
	int hostdomain = 1;
	char *p, *p1;

	if (!wsi)
		return -1;

	stash = wsi->stash ? wsi->stash : lws_get_network_wsi(wsi)->stash;
	if (!stash || !stash->cis[CIS_ADDRESS] ||
			   !stash->cis[CIS_PATH])
		return -1;

	l1 = wsi->a.context->l1;
	if (!l1 || !wsi->a.context->nsc){
		lwsl_err("%s:no cookiejar\n", __func__);
		return -1;
	}

	memset(&c, 0, sizeof(c));

	domain = stash->cis[CIS_ADDRESS];
	path = stash->cis[CIS_PATH];

	if (!domain || !path)
		return -1;

	path_len = strlen(path);

	/* remove query string if exist */
	dl_path = memchr(path, '?', path_len);
	if (dl_path)
		path_len = lws_ptr_diff_size_t(dl_path,  path);

	/* remove last slash if exist */
	if (path_len != 1 && path[path_len - 1] == '/')
		path_len--;

	if (!path_len)
		return -1;

	lwsl_cookie("%s: path %.*s len %d\n", __func__, (int)path_len, path, (int)path_len);

	/* when dest buf is not provided, we only return size of cookie string */
	if (!buf || !end)
		p = NULL;
	else
		p = buf;

	/* iterate through domain and path levels to find matching cookies */
	dl_domain = domain;
	while (dl_domain) {
		domain_len = strlen(domain);
		dl_domain = memchr(domain, '.', domain_len);
		/* don't match top level domain */
		if (!dl_domain)
			break;

		if (domain_len + path_len + 6 > sizeof(cache_name))
			return -1;

		/* compile key string "[domain]|[path]|*"" */
		p1 = cache_name;
		memcpy(p1, domain, domain_len);
		p1 += domain_len;
		*p1 = '|';
		p1++;
		memcpy(p1, path, path_len);
		p1 += path_len;
		*p1 = '|';
		p1++;
		*p1 = '*';
		p1++;
		*p1 = '\0';

		lwsl_cookie("%s: looking for %s\n", __func__, cache_name);

		if (!lws_cache_lookup(l1, cache_name,
				      (const void **)&cr.ptr, &cr.size)) {

			while (!lws_cache_results_walk(&cr)) {
				lwsl_cookie(" %s (%d)\n", (const char *)cr.tag,
						(int)cr.payload_len);

				if (lws_cache_item_get(l1, (const char *)cr.tag,
						   (const void **)&po, &size) ||
					lws_cookie_parse_nsc(&c, po, size)) {
					lwsl_err("%s: failed to get c '%s'\n",
							__func__, cr.tag);
					break;
				}

				if (c.f[CE_HOSTONLY] && !hostdomain){
					lwsl_cookie("%s: not sending this\n",
							__func__);
					continue;
				}

				if (p) {
					if (ret) {
						*p = ';';
						p++;
						*p = ' ';
						p++;
					}

					memcpy(p, c.f[CE_NAME], c.l[CE_NAME]);
					p += c.l[CE_NAME];
					*p = '=';
					p++;
					memcpy(p, c.f[CE_VALUE], c.l[CE_VALUE]);
					p += c.l[CE_VALUE];
				}

				if (ret)
					ret += 2;
				ret += c.l[CE_NAME] + 1 + c.l[CE_VALUE];

			}
		}

		domain = dl_domain + 1;
		hostdomain = 0;
	}

	lwsl_notice("%s: c len (%d)\n", __func__, (int)ret);

	return (int)ret;
}

static struct {
	const char		*const name;
	uint8_t			len;
} cft[] = {
	{ "domain=",  7 },
	{ "path=",    5 },
	{ "expires=", 8 },
	{ "max-age=", 8 },
	{ "httponly", 8 },
	{ "secure",   6 }
};

int
lws_parse_set_cookie(struct lws *wsi)
{
	char *tk_head, *tk_end, *buf_head, *buf_end, *cookiep, *dl;
	struct lws_cache_ttl_lru *l1;
	struct lws_cookie c;
	size_t fl;
	int f, n;

	if (!wsi)
		return -1;

	l1 = wsi->a.context->l1;
	if (!l1)
		return -1;

	f = wsi->http.ah->frag_index[WSI_TOKEN_HTTP_SET_COOKIE];

	while (f) {
		cookiep = wsi->http.ah->data + wsi->http.ah->frags[f].offset;
		fl = wsi->http.ah->frags[f].len;
		f = wsi->http.ah->frags[f].nfrag;

		if (!cookiep || !fl)
			continue;

#if defined(LWS_COOKIE_DEBUG)
		lwsl_notice("%s:parsing: %.*s\n", __func__, (int)fl, cookiep);
#endif

		buf_head = cookiep;
		buf_end = cookiep + fl - 1;
		memset(&c, 0, sizeof(struct lws_cookie));

		do {
			tk_head = buf_head;
			tk_end = memchr(buf_head, ';',
					(size_t)(buf_end - buf_head + 1));
			if (!tk_end) {
				tk_end = buf_end;
				buf_head = buf_end;
			} else {
				buf_head = tk_end + 1;
				tk_end--;
			}

			if (c.f[CE_NAME])
				goto parse_av;

			/*
			 * find name value, remove leading trailing
			 * WS and DQ for value
			 */

			dl = memchr(tk_head, '=', lws_ptr_diff_size_t(tk_end,
							tk_head + 1));
			if (!dl || dl == tk_head)
				return -1;

			c.f[CE_NAME] = tk_head;
			c.l[CE_NAME] = lws_ptr_diff_size_t(dl, tk_head);
			lws_cookie_rm_sws(&c.f[CE_NAME], &c.l[CE_NAME]);

			if (!c.l[CE_NAME])
				return -1;

			lwsl_cookie("%s: c name l %d v:%.*s\n", __func__,
					(int)c.l[CE_NAME],
					(int)c.l[CE_NAME], c.f[CE_NAME]);
			c.f[CE_VALUE] = dl + 1;
			c.l[CE_VALUE] = lws_ptr_diff_size_t(tk_end,
						   c.f[CE_VALUE]) + 1;

			lws_cookie_rm_sws(&c.f[CE_VALUE], &c.l[CE_VALUE]);
			if (c.l[CE_VALUE] >= 2 && c.f[CE_VALUE][0] == '\"') {
				c.f[CE_VALUE]++;
				c.l[CE_VALUE] -= 2;
			}
			lwsl_cookie("%s: c value l %d v:%.*s\n", __func__,
				    (int)c.l[CE_VALUE], (int)c.l[CE_VALUE],
				    c.f[CE_VALUE]);
			continue;

parse_av:
			while (*tk_head == ' ') {
				if (tk_head == tk_end)
					return -1;

				tk_head++;
			}

			for (n = 0; n < (int)LWS_ARRAY_SIZE(cft); n++) {
				if (lws_tolower(*tk_head) != cft[n].name[0])
					continue;

				if (!is_iprefix(tk_head,
						lws_ptr_diff_size_t(tk_end,
								   tk_head) + 1,
						cft[n].name, cft[n].len))
					continue;

				if (n == 4 || n == 5) {
					c.f[n] = "T";
					c.l[n] = 1;
					break;
				}

				c.f[n] = tk_head + cft[n].len;
				c.l[n] = lws_ptr_diff_size_t(tk_end, c.f[n]) + 1;
				lws_cookie_rm_sws(&c.f[n], &c.l[n]);

				if (n == CE_DOMAIN && c.l[0] &&
				    c.f[n][0] == '.'){
					c.f[n]++;
					c.l[n]--;
				}

				lwsl_cookie("%s: %s l %d v:%.*s\n", __func__,
					    cft[n].name, (int)c.l[n],
					    (int)c.l[n], c.f[n]);
				break;
			}

		} while (tk_end != buf_end);

		if (lws_cookie_write_nsc(wsi, &c))
			lwsl_err("%s:failed to write nsc\n", __func__);
	}

	return 0;
}

int
lws_cookie_send_cookies(struct lws *wsi, char **pp, char *end)
{
	char *p;
	int size;

	if (!wsi || !pp || !(*pp) || !end)
		return -1;

	size = lws_cookie_attach_cookies(wsi, NULL, NULL);

	if (!size)
		return 0;
	if (size < 0) {
		lwsl_err("%s:failed to get cookie string size\n", __func__);
		return -1;
	}

	lwsl_notice("%s: size %d\n", __func__, size);

#if defined(LWS_COOKIE_DEBUG)
		char *p_dbg = *pp;
#endif

	if (lws_add_http_header_by_token(wsi, WSI_TOKEN_HTTP_COOKIE, NULL, size,
								(unsigned char **)pp, (unsigned char *)end))
		return -1;

#if defined(LWS_COOKIE_DEBUG)
		lwsl_notice("%s: dummy copy (%.*s) \n", __func__, (int)(*pp - p_dbg), p_dbg);
#endif


#ifdef LWS_WITH_HTTP2
	if (lws_wsi_is_h2(wsi))
		p = *pp - size;
	else
#endif
		p = *pp - size - 2;

	if (lws_cookie_attach_cookies(wsi, p, p + size) <= 0) {
		lwsl_err("%s:failed to attach cookies\n", __func__);
		return -1;
	}

#if defined(LWS_COOKIE_DEBUG)
		lwsl_notice("%s: real copy (%.*s) total len %d\n", __func__, (int)(*pp - p_dbg), p_dbg, (int)(*pp - p_dbg));
		lwsl_hexdump_notice(p_dbg, (size_t)(*pp - p_dbg));
#endif

	return 0;
}
