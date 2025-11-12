/*
 * libwebsockets - small server side websockets and web server implementation
 *
 * Copyright (C) 2010 - 2021 Andy Green <andy@warmcat.com>
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

#ifdef LWS_HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#if defined(LWS_PLAT_OPTEE)
void lwsl_emit_optee(int level, const char *line);
#endif

lws_log_cx_t log_cx = {
#if !defined(LWS_PLAT_OPTEE)
	.u.emit				= lwsl_emit_stderr,
#else
	.u.emit				= lwsl_emit_optee,
#endif
	.lll_flags			= LLL_ERR | LLL_WARN | LLL_NOTICE,
};

#if !defined(LWS_PLAT_OPTEE) && !defined(LWS_WITH_NO_LOGS)
static const char * log_level_names ="EWNIDPHXCLUT??";
#endif

/*
 * Name an instance tag and attach to a group
 */

void
__lws_lc_tag(struct lws_context *context, lws_lifecycle_group_t *grp,
	     lws_lifecycle_t *lc, const char *format, ...)
{
	va_list ap;
	int n = 1;

	if (*lc->gutag == '[') {
		/* appending inside [] */

		char *cp = strchr(lc->gutag, ']');
		char rend[96];
		size_t ll, k;
		int n;

		if (!cp)
			return;

		/* length of closing brace and anything else after it */
		k = strlen(cp);

		/* compute the remaining gutag unused */
		ll = sizeof(lc->gutag) - lws_ptr_diff_size_t(cp, lc->gutag) - k - 1;
		if (ll > sizeof(rend) - 1)
			ll = sizeof(rend) - 1;
		va_start(ap, format);
		n = vsnprintf(rend, ll, format, ap);
		va_end(ap);

		if ((unsigned int)n > ll)
			n = (int)ll;

		/* shove the trailer up by what we added */
		memmove(cp + n, cp, k);
		assert(k + (unsigned int)n < sizeof(lc->gutag));
		cp[k + (unsigned int)n] = '\0';
		/* copy what we added into place */
		memcpy(cp, rend, (unsigned int)n);

		return;
	}

	assert(grp);
	assert(grp->tag_prefix); /* lc group must have a tag prefix string */

	lc->gutag[0] = '[';

#if defined(LWS_WITH_SECURE_STREAMS_PROXY_API) /* ie, will have getpid if set */
	n += lws_snprintf(&lc->gutag[n], sizeof(lc->gutag) -
					 (unsigned int)n - 1u, "%u|", getpid());
#endif
	n += lws_snprintf(&lc->gutag[n], sizeof(lc->gutag) -
					 (unsigned int)n - 1u, "%s|%lx|",
					 grp->tag_prefix,
					 (unsigned long)grp->ordinal++);

	va_start(ap, format);
	n += vsnprintf(&lc->gutag[n], sizeof(lc->gutag) - (unsigned int)n -
			1u, format, ap);
	va_end(ap);

	if (n < (int)sizeof(lc->gutag) - 2) {
		lc->gutag[n++] = ']';
		lc->gutag[n++] = '\0';
	} else {
		lc->gutag[sizeof(lc->gutag) - 2] = ']';
		lc->gutag[sizeof(lc->gutag) - 1] = '\0';
	}

	lc->us_creation = (uint64_t)lws_now_usecs();
	lws_dll2_add_tail(&lc->list, &grp->owner);

	lwsl_refcount_cx(lc->log_cx, 1);

#if defined(LWS_LOG_TAG_LIFECYCLE)
	lwsl_cx_notice(context, " ++ %s (%d)", lc->gutag, (int)grp->owner.count);
#endif
}

/*
 * Normally we want to set the tag one time at creation.  But sometimes we
 * don't have enough information at that point to give it a meaningful tag, eg,
 * it's an accepted, served connection but we haven't read data from it yet
 * to find out what it wants to be.
 *
 * This allows you to append some extra info to the tag in those cases, the
 * initial tag remains the same on the lhs so it can be tracked correctly.
 */

void
__lws_lc_tag_append(lws_lifecycle_t *lc, const char *app)
{
	int n = (int)strlen(lc->gutag);

	if (n && lc->gutag[n - 1] == ']')
		n--;

	n += lws_snprintf(&lc->gutag[n], sizeof(lc->gutag) - 2u -
					 (unsigned int)n, "|%s]", app);

	if ((unsigned int)n >= sizeof(lc->gutag) - 2u) {
		lc->gutag[sizeof(lc->gutag) - 2] = ']';
		lc->gutag[sizeof(lc->gutag) - 1] = '\0';
	}
}

/*
 * Remove instance from group
 */

void
__lws_lc_untag(struct lws_context *context, lws_lifecycle_t *lc)
{
	//lws_lifecycle_group_t *grp;
	char buf[24];

	if (!lc->gutag[0]) { /* we never tagged this object... */
		lwsl_cx_err(context, "%s never tagged", lc->gutag);
		assert(0);
		return;
	}

	if (!lc->list.owner) { /* we already untagged this object... */
		lwsl_cx_err(context, "%s untagged twice", lc->gutag);
		assert(0);
		return;
	}

	//grp = lws_container_of(lc->list.owner, lws_lifecycle_group_t, owner);

	lws_humanize(buf, sizeof(buf),
		     (uint64_t)lws_now_usecs() - lc->us_creation,
		     humanize_schema_us);

#if defined(LWS_LOG_TAG_LIFECYCLE)
	lwsl_cx_notice(context, " -- %s (%d) %s", lc->gutag,
		    (int)lc->list.owner->count - 1, buf);
#endif

	lws_dll2_remove(&lc->list);

	lwsl_refcount_cx(lc->log_cx, -1);
}

const char *
lws_lc_tag(lws_lifecycle_t *lc)
{
	return lc->gutag;
}


int
lwsl_timestamp(int level, char *p, size_t len)
{
#if !defined(LWS_PLAT_OPTEE) && !defined(LWS_WITH_NO_LOGS)
	time_t o_now;
	unsigned long long now;
	struct timeval tv;
	struct tm *ptm = NULL;
#if defined(LWS_HAVE_LOCALTIME_R)
	struct tm tm;
#endif
	int n;

	gettimeofday(&tv, NULL);
	o_now = tv.tv_sec;
	now = ((unsigned long long)tv.tv_sec * 10000) +
				(unsigned int)(tv.tv_usec / 100);

#if defined(LWS_HAVE_LOCALTIME_R)
	ptm = localtime_r(&o_now, &tm);
#else
	ptm = localtime(&o_now);
#endif
	p[0] = '\0';
	for (n = 0; n < LLL_COUNT; n++) {
		if (level != (1 << n))
			continue;

		if (ptm)
			n = lws_snprintf(p, len,
				"[%04d/%02d/%02d %02d:%02d:%02d:%04d] %c: ",
				ptm->tm_year + 1900,
				ptm->tm_mon + 1,
				ptm->tm_mday,
				ptm->tm_hour,
				ptm->tm_min,
				ptm->tm_sec,
				(int)(now % 10000), log_level_names[n]);
		else
			n = lws_snprintf(p, len, "[%llu:%04d] %c: ",
					(unsigned long long) now / 10000,
					(int)(now % 10000), log_level_names[n]);

#if defined(LWS_PLAT_FREERTOS)
		n += lws_snprintf(p + n, len - n, "%6u: ",
#if defined(LWS_AMAZON_RTOS)
				  (unsigned int)xPortGetFreeHeapSize());
#else
				  (unsigned int)esp_get_free_heap_size());
#endif
#endif

		return n;
	}
#else
	p[0] = '\0';
#endif

	return 0;
}

#ifndef LWS_PLAT_OPTEE
static const char * const colours[] = {
	"[31;1m", /* LLL_ERR */
	"[36;1m", /* LLL_WARN */
	"[35;1m", /* LLL_NOTICE */
	"[32;1m", /* LLL_INFO */
	"[34;1m", /* LLL_DEBUG */
	"[33;1m", /* LLL_PARSER */
	"[33m", /* LLL_HEADER */
	"[33m", /* LLL_EXT */
	"[33m", /* LLL_CLIENT */
	"[33;1m", /* LLL_LATENCY */
        "[0;1m", /* LLL_USER */
	"[31m", /* LLL_THREAD */
};

static char tty;

static void
_lwsl_emit_stderr(int level, const char *line)
{
	int n, m = LWS_ARRAY_SIZE(colours) - 1;

	if (!tty)
		tty = (char)(isatty(2) | 2);

	if (tty == 3) {
		n = 1 << (LWS_ARRAY_SIZE(colours) - 1);
		while (n) {
			if (level & n)
				break;
			m--;
			n >>= 1;
		}
		fprintf(stderr, "%c%s%s%c[0m", 27, colours[m], line, 27);
	} else
		fprintf(stderr, "%s", line);
}

void
lwsl_emit_stderr(int level, const char *line)
{
	_lwsl_emit_stderr(level, line);
}

void
lwsl_emit_stderr_notimestamp(int level, const char *line)
{
	_lwsl_emit_stderr(level, line);
}

#if !defined(LWS_PLAT_FREERTOS) && !defined(LWS_PLAT_OPTEE)

/*
 * Helper to emit to a file
 */

void
lws_log_emit_cx_file(struct lws_log_cx *cx, int level, const char *line,
			size_t len)
{
	int fd = (int)(intptr_t)cx->stg;

	if (fd >= 0)
		if (write(fd, line, (unsigned int)len) != (ssize_t)len)
			fprintf(stderr, "Unable to write log to file\n");
}

/*
 * Helper to use a .refcount_cb to store logs in a file
 */

void
lws_log_use_cx_file(struct lws_log_cx *cx, int _new)
{
	int fd;

	if (_new > 0 && cx->refcount == 1) {
		fd = open((const char *)cx->opaque,
				LWS_O_CREAT | LWS_O_TRUNC | LWS_O_WRONLY, 0600);
		if (fd < 0)
			fprintf(stderr, "Unable to open log %s: errno %d\n",
				(const char *)cx->opaque, errno);
		cx->stg = (void *)(intptr_t)fd;

		return;
	}

	fd = (int)(intptr_t)cx->stg;

	if (_new <= 0 && cx->refcount == 0 && fd >= 0) {
		close(fd);
		cx->stg = (void *)(intptr_t)-1;
	}
}

#endif

#endif

#if !(defined(LWS_PLAT_OPTEE) && !defined(LWS_WITH_NETWORK))
void
__lws_logv(lws_log_cx_t *cx, lws_log_prepend_cx_t prep, void *obj,
	   int filter, const char *_fun, const char *format, va_list vl)
{
#if LWS_MAX_SMP == 1 && !defined(LWS_WITH_THREADPOOL)
	/* this is incompatible with multithreaded logging */
	static char buf[256];
#else
	char buf[1024];
#endif
	char *p = buf, *end = p + sizeof(buf) - 1;
	lws_log_cx_t *cxp;
	int n, back = 0;

	/*
	 * We need to handle NULL wsi etc at the wrappers as gracefully as
	 * possible
	 */

	if (!cx) {
		lws_strncpy(p, "NULL log cx: ", sizeof(buf) - 1);
		p += 13;
		/* use the processwide one for lack of anything better */
		cx = &log_cx;
	}

	cxp = cx;

	if (!(cx->lll_flags & (uint32_t)filter))
		/*
		 * logs may be produced and built in to the code but disabled
		 * at runtime
		 */
		return;

#if !defined(LWS_LOGS_TIMESTAMP)
	if (cx->lll_flags & LLLF_LOG_TIMESTAMP)
#endif
	{
		buf[0] = '\0';
		lwsl_timestamp(filter, buf, sizeof(buf));
		p += strlen(buf);
	}

	/*
	 * prepend parent log ctx content first
	 * top level cx also gets an opportunity to prepend
	 */

	while (cxp->parent) {
		cxp = cxp->parent;
		back++;
	}

	do {
		int b = back;

		cxp = cx;
		while (b--)
			cxp = cxp->parent;
		if (cxp->prepend)
			cxp->prepend(cxp, NULL, &p, end);

		back--;
	} while (back > 0);

	if (prep)
		prep(cxp, obj, &p, end);

	if (_fun)
		p += lws_snprintf(p, lws_ptr_diff_size_t(end, p), "%s: ", _fun);

	/*
	 * The actual log content
	 */

	n = vsnprintf(p, lws_ptr_diff_size_t(end, p), format, vl);

	/* vnsprintf returns what it would have written, even if truncated */
	if (p + n > end - 2) {
		p = end - 5;
		*p++ = '.';
		*p++ = '.';
		*p++ = '.';
		*p++ = '\n';
		*p++ = '\0';
	} else
		if (n > 0) {
			p += n;
			if (p[-1] != '\n')
				*p++ = '\n';
			*p = '\0';
		}

	/*
	 * The actual emit
	 */

	if (cx->lll_flags & LLLF_LOG_CONTEXT_AWARE)
		cx->u.emit_cx(cx, filter, buf, lws_ptr_diff_size_t(p, buf));
	else
		cx->u.emit(filter, buf);
}

void _lws_logv(int filter, const char *format, va_list vl)
{
	__lws_logv(&log_cx, NULL, NULL, filter, NULL, format, vl);
}

void _lws_log(int filter, const char *format, ...)
{
	va_list ap;

	va_start(ap, format);
	__lws_logv(&log_cx, NULL, NULL, filter, NULL, format, ap);
	va_end(ap);
}

void _lws_log_cx(lws_log_cx_t *cx, lws_log_prepend_cx_t prep, void *obj,
		 int filter, const char *_fun, const char *format, ...)
{
	va_list ap;

	if (!cx)
		cx = &log_cx;

	va_start(ap, format);
	__lws_logv(cx, prep, obj, filter, _fun, format, ap);
	va_end(ap);
}
#endif

void
lws_set_log_level(int flags, lws_log_emit_t func)
{
	log_cx.lll_flags = (uint32_t)(flags & (~LLLF_LOG_CONTEXT_AWARE));

	if (func)
		log_cx.u.emit = func;
}

int lwsl_visible(int level)
{
	return !!(log_cx.lll_flags & (uint32_t)level);
}

int lwsl_visible_cx(lws_log_cx_t *cx, int level)
{
	return !!(cx->lll_flags & (uint32_t)level);
}

void
lwsl_refcount_cx(lws_log_cx_t *cx, int _new)
{
#if LWS_MAX_SMP > 1
	volatile lws_log_cx_t *vcx = (volatile lws_log_cx_t *)cx;
#endif

	if (!cx)
		return;

#if LWS_MAX_SMP > 1
	if (!vcx->inited) {
		vcx->inited = 1;
		lws_pthread_mutex_init(&cx->refcount_lock);
		vcx->inited = 2;
	}
	while (vcx->inited != 2)
		;
	lws_pthread_mutex_lock(&cx->refcount_lock);
#endif

	if (_new > 0)
		cx->refcount++;
	else {
		assert(cx->refcount);
		cx->refcount--;
	}

	if (cx->refcount_cb)
		cx->refcount_cb(cx, _new);

#if LWS_MAX_SMP > 1
	lws_pthread_mutex_unlock(&cx->refcount_lock);
#endif
}

void
lwsl_hexdump_level_cx(lws_log_cx_t *cx, lws_log_prepend_cx_t prep, void *obj,
		      int hexdump_level, const void *vbuf, size_t len)
{
	unsigned char *buf = (unsigned char *)vbuf;
	unsigned int n;

	if (!lwsl_visible_cx(cx, hexdump_level))
		return;

	if (!len) {
		_lws_log_cx(cx, prep, obj, hexdump_level, NULL,
					"(hexdump: zero length)\n");
		return;
	}

	if (!vbuf) {
		_lws_log_cx(cx, prep, obj, hexdump_level, NULL,
					"(hexdump: NULL ptr)\n");
		return;
	}

	_lws_log_cx(cx, prep, obj, hexdump_level, NULL, "\n");

	for (n = 0; n < len;) {
		unsigned int start = n, m;
		char line[80], *p = line;

		p += lws_snprintf(p, 10, "%04X: ", start);

		for (m = 0; m < 16 && n < len; m++)
			p += lws_snprintf(p, 5, "%02X ", buf[n++]);
		while (m++ < 16)
			p += lws_snprintf(p, 5, "   ");

		p += lws_snprintf(p, 6, "   ");

		for (m = 0; m < 16 && (start + m) < len; m++) {
			if (buf[start + m] >= ' ' && buf[start + m] < 127)
				*p++ = (char)buf[start + m];
			else
				*p++ = '.';
		}
		while (m++ < 16)
			*p++ = ' ';

		*p++ = '\n';
		*p = '\0';
		_lws_log_cx(cx, prep, obj, hexdump_level, NULL, "%s", line);
		(void)line;
	}

	_lws_log_cx(cx, prep, obj, hexdump_level, NULL, "\n");
}

void
lwsl_hexdump_level(int hexdump_level, const void *vbuf, size_t len)
{
	lwsl_hexdump_level_cx(&log_cx, NULL, NULL, hexdump_level, vbuf, len);
}

void
lwsl_hexdump(const void *vbuf, size_t len)
{
#if defined(_DEBUG)
	lwsl_hexdump_level(LLL_DEBUG, vbuf, len);
#endif
}
