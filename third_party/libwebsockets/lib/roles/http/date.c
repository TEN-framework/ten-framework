/*
 * libwebsockets - small server side websockets and web server implementation
 *
 * Copyright (C) 2010 - 2020 Andy Green <andy@warmcat.com>
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
 *
 * RFC7231 date string generation and parsing
 */

#include "private-lib-core.h"

/*
 * To avoid needless pointers, we encode these in one string using the fact
 * they're 3 chars each to index it
 */

static const char *const s =
		"JanFebMarAprMayJunJulAugSepOctNovDecMonTueWedThuFriSatSun";

static int
lws_http_date_render(char *buf, size_t len, const struct tm *tm)
{
	const char *w = s + 36 + (3 * tm->tm_wday), *m = s + (3 * tm->tm_mon);

	if (len < 29)
		return -1;

	lws_snprintf(buf, len, "%c%c%c, %02d %c%c%c %d %02d:%02d:%02d GMT",
		     w[0], w[1], w[2], tm->tm_mday, m[0], m[1], m[2],
		     1900 + tm->tm_year, tm->tm_hour, tm->tm_min, tm->tm_sec);

	return 0;
}


int
lws_http_date_render_from_unix(char *buf, size_t len, const time_t *t)
{
#if defined(LWS_HAVE_GMTIME_R)
	struct tm tmp;
	struct tm *tm = gmtime_r(t, &tmp);
#else
	struct tm *tm = gmtime(t);
#endif
	if (!tm)
		return -1;

	if (lws_http_date_render(buf, len, tm))
		return -1;

	return 0;
}

static int
lws_http_date_parse(const char *b, size_t len, struct tm *tm)
{
	int n;

	if (len < 29)
		return -1;

	/*
	 * We reject anything that isn't a properly-formatted RFC7231 date, eg
	 *
	 *     Tue, 15 Nov 1994 08:12:31 GMT
	 */

	if (b[3] != ','  || b[4] != ' '  || b[7] != ' '  || b[11] != ' ' ||
	    b[16] != ' ' || b[19] != ':' || b[22] != ':' || b[25] != ' ' ||
	    b[26] != 'G' || b[27] != 'M' || b[28] != 'T')
		return -1;

	memset(tm, 0, sizeof(*tm));

	for (n = 36; n < 57; n += 3)
		if (b[0] == s[n] && b[1] == s[n + 1] && b[2] == s[n + 2])
			break;
		else
			tm->tm_wday++;

	if (n == 57)
		return -1;

	for (n = 0; n < 36; n += 3)
		if (b[8] == s[n] && b[9] == s[n + 1] && b[10] == s[n + 2])
			break;
		else
			tm->tm_mon++;

	if (n == 36)
		return -1;

	tm->tm_mday = atoi(b + 5);
	n = atoi(b + 12);
	if (n < 1900)
		return -1;
	tm->tm_year = n - 1900;

	n = atoi(b + 17);
	if (n < 0 || n > 23)
		return -1;
	tm->tm_hour = n;

	n = atoi(b + 20);
	if (n < 0 || n > 60)
		return -1;
	tm->tm_min = n;

	n = atoi(b + 23);
	if (n < 0 || n > 61) /* leap second */
		return -1;
	tm->tm_sec = n;

	return 0;
}

int
lws_http_date_parse_unix(const char *b, size_t len, time_t *t)
{
	struct tm tm;

	if (lws_http_date_parse(b, len, &tm))
		return -1;

#if defined(WIN32)
	*t = _mkgmtime(&tm);
#else
#if defined(LWS_HAVE_TIMEGM)
	*t = timegm(&tm);
#else
	/* this is a poor fallback since it uses localtime zone */
	*t = mktime(&tm);
#endif
#endif

	return (int)*t == -1 ? -1 : 0;
}

#if defined(LWS_WITH_CLIENT)

int
lws_http_check_retry_after(struct lws *wsi, lws_usec_t *us_interval_in_out)
{
	size_t len = (unsigned int)lws_hdr_total_length(wsi, WSI_TOKEN_HTTP_RETRY_AFTER);
	char *p = lws_hdr_simple_ptr(wsi, WSI_TOKEN_HTTP_RETRY_AFTER);
	lws_usec_t u;
	time_t t, td;

	if (!p)
		return 1;

	/*
	 * There are two arg styles for RETRY_AFTER specified in RFC7231 7.1.3,
	 * either a full absolute second-resolution date/time, or an integer
	 * interval
	 *
	 *      Retry-After: Fri, 31 Dec 1999 23:59:59 GMT
         *      Retry-After: 120
	 */

	if (len < 9)
		u = ((lws_usec_t)(time_t)atoi(p)) * LWS_USEC_PER_SEC;
	else {

		if (lws_http_date_parse_unix(p, len, &t))
			return 1;

		/*
		 * If possible, look for DATE from the server as well, so we
		 * can calculate the interval it thinks it is giving us,
		 * eliminating problems from server - client clock skew
		 */

		time(&td);
		len = (unsigned int)lws_hdr_total_length(wsi, WSI_TOKEN_HTTP_DATE);
		if (len) {
			p = lws_hdr_simple_ptr(wsi, WSI_TOKEN_HTTP_DATE);
			/* if this fails, it leaves td as client time */
			(void)lws_http_date_parse_unix(p, len, &td);
		}

		if (td >= t)
			/*
			 * if he's effectively giving us a 0 or negative
			 * interval, just ignore the whole thing and keep the
			 * incoming interval
			 */
			return 1;

		u = ((lws_usec_t)(t - td)) * LWS_USEC_PER_SEC;
	}

	/*
	 * We are only willing to increase the incoming interval, not
	 * decrease it
	 */

	if (u < *us_interval_in_out)
		/* keep the incoming interval */
		return 1;

	/* use the computed interval */
	*us_interval_in_out = u;

	return 0;
}

#endif
