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
 *
 * Implements a cache backing store compatible with netscape cookies.txt format
 * There is one entry per "line", and fields are tab-delimited
 *
 * We need to know the format here, because while the unique cookie tag consists
 * of "hostname|urlpath|cookiename", that does not appear like that in the file;
 * we have to go parse the fields and synthesize the corresponding tag.
 *
 * We rely on all the fields except the cookie value fitting in a 256 byte
 * buffer, and allow eating multiple buffers to get a huge cookie values.
 *
 * Because the cookie file is a device-wide asset, although lws will change it
 * from the lws thread without conflict, there may be other processes that will
 * change it by removal and regenerating the file asynchronously.  For that
 * reason, file handles are opened fresh each time we want to use the file, so
 * we always get the latest version.
 *
 * When updating the file ourselves, we use a lockfile to ensure our process
 * has exclusive access.
 *
 *
 * Tag Matching rules
 *
 * There are three kinds of tag matching rules
 *
 * 1) specific - tag strigs must be the same
 * 2) wilcard - tags matched using optional wildcards
 * 3) wildcard + lookup - wildcard, but path part matches using cookie scope rules
 *
 */

#include <private-lib-core.h>
#include "private-lib-misc-cache-ttl.h"

typedef enum nsc_iterator_ret {
	NIR_CONTINUE		= 0,
	NIR_FINISH_OK		= 1,
	NIR_FINISH_ERROR	= -1
} nsc_iterator_ret_t;

typedef enum cbreason {
	LCN_SOL			= (1 << 0),
	LCN_EOL			= (1 << 1)
} cbreason_t;

typedef int (*nsc_cb_t)(lws_cache_nscookiejar_t *cache, void *opaque, int flags,
			const char *buf, size_t size);

static void
expiry_cb(lws_sorted_usec_list_t *sul);

static int
nsc_backing_open_lock(lws_cache_nscookiejar_t *cache, int mode, const char *par)
{
	int sanity = 50;
	char lock[128];
	int fd_lock, fd;

	lwsl_debug("%s: %s\n", __func__, par);

	lws_snprintf(lock, sizeof(lock), "%s.LCK",
			cache->cache.info.u.nscookiejar.filepath);

	do {
		fd_lock = open(lock, LWS_O_CREAT | O_EXCL, 0600);
		if (fd_lock >= 0) {
			close(fd_lock);
			break;
		}

		if (!sanity--) {
			lwsl_warn("%s: unable to lock %s: errno %d\n", __func__,
					lock, errno);
			return -1;
		}

#if defined(WIN32)
		Sleep(100);
#else
		usleep(100000);
#endif
	} while (1);

	fd = open(cache->cache.info.u.nscookiejar.filepath,
		      LWS_O_CREAT | mode, 0600);

	if (fd == -1) {
		lwsl_warn("%s: unable to open or create %s\n", __func__,
				cache->cache.info.u.nscookiejar.filepath);
		unlink(lock);
	}

	return fd;
}

static void
nsc_backing_close_unlock(lws_cache_nscookiejar_t *cache, int fd)
{
	char lock[128];

	lwsl_debug("%s\n", __func__);

	lws_snprintf(lock, sizeof(lock), "%s.LCK",
			cache->cache.info.u.nscookiejar.filepath);
	if (fd >= 0)
		close(fd);
	unlink(lock);
}

/*
 * We're going to call the callback with chunks of the file with flags
 * indicating we're giving it the start of a line and / or giving it the end
 * of a line.
 *
 * It's like this because the cookie value may be huge (and to a lesser extent
 * the path may also be big).
 *
 * If it's the start of a line (flags on the cb has LCN_SOL), then the buffer
 * contains up to the first 256 chars of the line, it's enough to match with.
 *
 * We cannot hold the file open inbetweentimes, since other processes may
 * regenerate it, so we need to bind to a new inode.  We open it with an
 * exclusive flock() so other processes can't replace conflicting changes
 * while we also write changes, without having to wait and see our changes.
 */

static int
nscookiejar_iterate(lws_cache_nscookiejar_t *cache, int fd,
		    nsc_cb_t cb, void *opaque)
{
	int m = 0, n = 0, e, r = LCN_SOL, ignore = 0, ret = 0;
	char temp[256], eof = 0;

	if (lseek(fd, 0, SEEK_SET) == (off_t)-1)
		return -1;

	do { /* for as many buffers in the file */

		int n1;

		lwsl_debug("%s: n %d, m %d\n", __func__, n, m);

read:
		n1 = (int)read(fd, temp + n, sizeof(temp) - (size_t)n);

		lwsl_debug("%s: n1 %d\n", __func__, n1);

		if (n1 <= 0) {
			eof = 1;
			if (m == n)
				continue;
		} else
			n += n1;

		while (m < n) {

			m++;

			if (temp[m - 1] != '\n')
				continue;

			/* ie, we hit EOL */

			if (temp[0] == '#')
				/* lines starting with # are comments */
				e = 0;
			else
				e = cb(cache, opaque, r | LCN_EOL, temp,
				       (size_t)m - 1);
			r = LCN_SOL;
			ignore = 0;
			/*
			 * Move back remainder and prefill the gap that opened
			 * up: we want to pass enough in the start chunk so the
			 * cb can classify it even if it can't get all the
			 * value part in one go
			 */
			memmove(temp, temp + m, (size_t)(n - m));
			n -= m;
			m = 0;

			if (e) {
				ret = e;
				goto bail;
			}

			goto read;
		}

		if (m) {
			/* we ran out of buffer */
			if (ignore || (r == LCN_SOL && n && temp[0] == '#')) {
				e = 0;
				ignore = 1;
			} else {
				e = cb(cache, opaque,
				       r | (n == m && eof ? LCN_EOL : 0),
				       temp, (size_t)m);

				m = 0;
				n = 0;
			}

			if (e) {
				/*
				 * We have to call off the whole thing if any
				 * step, eg, OOMs
				 */
				ret = e;
				goto bail;
			}
			r = 0;
		}

	} while (!eof || n != m);

	ret = 0;

bail:

	return ret;
}

/*
 * lookup() just handles wildcard resolution, it doesn't deal with moving the
 * hits to L1.  That has to be done individually by non-wildcard names.
 */

enum {
	NSC_COL_HOST		= 0, /* wc idx 0 */
	NSC_COL_PATH		= 2, /* wc idx 1 */
	NSC_COL_EXPIRY		= 4,
	NSC_COL_NAME		= 5, /* wc idx 2 */

	NSC_COL_COUNT		= 6
};

/*
 * This performs the specialized wildcard that knows about cookie path match
 * rules.
 *
 * To defeat the lookup path matching, lie to it about idx being NSC_COL_PATH
 */

static int
nsc_match(const char *wc, size_t wc_len, const char *col, size_t col_len,
	  int idx)
{
	size_t n = 0;

	if (idx != NSC_COL_PATH)
		return lws_strcmp_wildcard(wc, wc_len, col, col_len);

	/*
	 * Cookie path match is special, if we lookup on a path like /my/path,
	 * we must match on cookie paths for every dir level including /, so
	 * match on /, /my, and /my/path.  But we must not match on /m or
	 * /my/pa etc.  If we lookup on /, we must not match /my/path
	 *
	 * Let's go through wc checking at / and for every complete subpath if
	 * it is an explicit match
	 */

	if (!strcmp(col, wc))
		return 0; /* exact hit */

	while (n <= wc_len) {
		if (n == wc_len || wc[n] == '/') {
			if (n && col_len <= n && !strncmp(wc, col, n))
				return 0; /* hit */

			if (n != wc_len && col_len <= n + 1 &&
			    !strncmp(wc, col, n + 1)) /* check for trailing / */
				return 0; /* hit */
		}
		n++;
	}

	return 1; /* fail */
}

static const uint8_t nsc_cols[] = { NSC_COL_HOST, NSC_COL_PATH, NSC_COL_NAME };

static int
lws_cache_nscookiejar_tag_match(struct lws_cache_ttl_lru *cache,
				const char *wc, const char *tag, char lookup)
{
	const char *wc_end = wc + strlen(wc), *tag_end = tag + strlen(tag),
			*start_wc, *start_tag;
	int n = 0;

	lwsl_cache("%s: '%s' vs '%s'\n", __func__, wc, tag);

	/*
	 * Given a well-formed host|path|name tag and a wildcard term,
	 * make the determination if the tag matches the wildcard or not,
	 * using lookup rules that apply at this cache level.
	 */

	while (n < 3) {
		start_wc = wc;
		while (wc < wc_end && *wc != LWSCTAG_SEP)
			wc++;

		start_tag = tag;
		while (tag < tag_end && *tag != LWSCTAG_SEP)
			tag++;

		lwsl_cache("%s:   '%.*s' vs '%.*s'\n", __func__,
				lws_ptr_diff(wc, start_wc), start_wc,
				lws_ptr_diff(tag, start_tag), start_tag);
		if (nsc_match(start_wc, lws_ptr_diff_size_t(wc, start_wc),
			      start_tag, lws_ptr_diff_size_t(tag, start_tag),
			      lookup ? nsc_cols[n] : NSC_COL_HOST)) {
			lwsl_cache("%s: fail\n", __func__);
			return 1;
		}

		if (wc < wc_end)
			wc++;
		if (tag < tag_end)
			tag++;

		n++;
	}

	lwsl_cache("%s: hit\n", __func__);

	return 0; /* match */
}

/*
 * Converts the start of a cookie file line into a tag
 */

static int
nsc_line_to_tag(const char *buf, size_t size, char *tag, size_t max_tag,
		lws_usec_t *pexpiry)
{
	int n, idx = 0, tl = 0;
	lws_usec_t expiry = 0;
	size_t bn = 0;
	char col[64];

	if (size < 3)
		return 1;

	while (bn < size && idx <= NSC_COL_NAME) {

		n = 0;
		while (bn < size && n < (int)sizeof(col) - 1 &&
		       buf[bn] != '\t')
			col[n++] = buf[bn++];
		col[n] = '\0';
		if (buf[bn] == '\t')
			bn++;

		switch (idx) {
		case NSC_COL_EXPIRY:
			expiry = (lws_usec_t)((unsigned long long)atoll(col) *
					(lws_usec_t)LWS_US_PER_SEC);
			break;

		case NSC_COL_HOST:
		case NSC_COL_PATH:
		case NSC_COL_NAME:

			/*
			 * As we match the pieces of the wildcard,
			 * compose the matches into a specific tag
			 */

			if (tl + n + 2 > (int)max_tag)
				return 1;
			if (tl)
				tag[tl++] = LWSCTAG_SEP;
			memcpy(tag + tl, col, (size_t)n);
			tl += n;
			tag[tl] = '\0';
			break;
		default:
			break;
		}

		idx++;
	}

	if (pexpiry)
		*pexpiry = expiry;

	lwsl_info("%s: %.*s: tag '%s'\n", __func__, (int)size, buf, tag);

	return 0;
}

struct nsc_lookup_ctx {
	const char		*wildcard_key;
	lws_dll2_owner_t	*results_owner;
	lws_cache_match_t	*match; /* current match if any */
	size_t			wklen;
};


static int
nsc_lookup_cb(lws_cache_nscookiejar_t *cache, void *opaque, int flags,
	      const char *buf, size_t size)
{
	struct nsc_lookup_ctx *ctx = (struct nsc_lookup_ctx *)opaque;
	lws_usec_t expiry;
	char tag[200];
	int tl;

	if (!(flags & LCN_SOL)) {
		if (ctx->match)
			ctx->match->payload_size += size;

		return NIR_CONTINUE;
	}

	/*
	 * There should be enough in buf to match or reject it... let's
	 * synthesize a tag from the text "line" and then check the tags for
	 * a match
	 */

	ctx->match = NULL; /* new SOL means stop tracking payload len */

	if (nsc_line_to_tag(buf, size, tag, sizeof(tag), &expiry))
		return NIR_CONTINUE;

	if (lws_cache_nscookiejar_tag_match(&cache->cache,
					    ctx->wildcard_key, tag, 1))
		return NIR_CONTINUE;

	tl = (int)strlen(tag);

	/*
	 * ... it looks like a match then... create new match
	 * object with the specific tag, and add it to the owner list
	 */

	ctx->match = lws_fi(&cache->cache.info.cx->fic, "cache_lookup_oom") ? NULL :
			lws_malloc(sizeof(*ctx->match) + (unsigned int)tl + 1u,
				__func__);
	if (!ctx->match)
		/* caller of lookup will clean results list on fail */
		return NIR_FINISH_ERROR;

	ctx->match->payload_size = size;
	ctx->match->tag_size = (size_t)tl;
	ctx->match->expiry = expiry;

	memset(&ctx->match->list, 0, sizeof(ctx->match->list));
	memcpy(&ctx->match[1], tag, (size_t)tl + 1u);
	lws_dll2_add_tail(&ctx->match->list, ctx->results_owner);

	return NIR_CONTINUE;
}

static int
lws_cache_nscookiejar_lookup(struct lws_cache_ttl_lru *_c,
			     const char *wildcard_key,
			     lws_dll2_owner_t *results_owner)
{
	lws_cache_nscookiejar_t *cache = (lws_cache_nscookiejar_t *)_c;
	struct nsc_lookup_ctx ctx;
	int ret, fd;

	fd = nsc_backing_open_lock(cache, LWS_O_RDONLY, __func__);
	if (fd < 0)
		return 1;

	ctx.wildcard_key = wildcard_key;
	ctx.results_owner = results_owner;
	ctx.wklen = strlen(wildcard_key);
	ctx.match = 0;

	ret = nscookiejar_iterate(cache, fd, nsc_lookup_cb, &ctx);
		/*
		 * The cb can fail, eg, with OOM, making the whole lookup
		 * invalid and returning fail.  Caller will clean
		 * results_owner on fail.
		 */
	nsc_backing_close_unlock(cache, fd);

	return ret == NIR_FINISH_ERROR;
}

/*
 * It's pretty horrible having to implement add or remove individual items by
 * file regeneration, but if we don't want to keep it all in heap, and we want
 * this cookie jar format, that is what we are into.
 *
 * Allow to optionally add a "line", optionally wildcard delete tags, and always
 * delete expired entries.
 *
 * Although we can rely on the lws thread to be doing this, multiple processes
 * may be using the cookie jar and can tread on each other.  So we use flock()
 * (linux only) to get exclusive access while we are processing this.
 *
 * We leave the existing file alone and generate a new one alongside it, with a
 * fixed name.tmp format so it can't leak, if that went OK then we unlink the
 * old and rename the new.
 */

struct nsc_regen_ctx {
	const char		*wildcard_key_delete;
	const void		*add_data;
	lws_usec_t		curr;
	size_t			add_size;
	int			fdt;
	char			drop;
};

/* only used by nsc_regen() */

static int
nsc_regen_cb(lws_cache_nscookiejar_t *cache, void *opaque, int flags,
	      const char *buf, size_t size)
{
	struct nsc_regen_ctx *ctx = (struct nsc_regen_ctx *)opaque;
	char tag[256];
	lws_usec_t expiry;

	if (flags & LCN_SOL) {

		ctx->drop = 0;

		if (nsc_line_to_tag(buf, size, tag, sizeof(tag), &expiry))
			/* filter it out if it is unparseable */
			goto drop;

		/* routinely track the earliest expiry */

		if (!cache->earliest_expiry ||
		    (expiry && cache->earliest_expiry > expiry))
			cache->earliest_expiry = expiry;

		if (expiry && expiry < ctx->curr)
			/* routinely strip anything beyond its expiry */
			goto drop;

		if (ctx->wildcard_key_delete)
			lwsl_cache("%s: %s vs %s\n", __func__,
					tag, ctx->wildcard_key_delete);
		if (ctx->wildcard_key_delete &&
		    !lws_cache_nscookiejar_tag_match(&cache->cache,
						     ctx->wildcard_key_delete,
						     tag, 0)) {
			lwsl_cache("%s: %s matches wc delete %s\n", __func__,
					tag, ctx->wildcard_key_delete);
			goto drop;
		}
	}

	if (ctx->drop)
		return 0;

	cache->cache.current_footprint += (uint64_t)size;

	if (write(ctx->fdt, buf, /*msvc*/(unsigned int)size) != (ssize_t)size)
		return NIR_FINISH_ERROR;

	if (flags & LCN_EOL)
		if ((size_t)write(ctx->fdt, "\n", 1) != 1)
			return NIR_FINISH_ERROR;

	return 0;

drop:
	ctx->drop = 1;

	return NIR_CONTINUE;
}

static int
nsc_regen(lws_cache_nscookiejar_t *cache, const char *wc_delete,
	  const void *pay, size_t pay_size)
{
	struct nsc_regen_ctx ctx;
	char filepath[128];
	int fd, ret = 1;

	fd = nsc_backing_open_lock(cache, LWS_O_RDONLY, __func__);
	if (fd < 0)
		return 1;

	lws_snprintf(filepath, sizeof(filepath), "%s.tmp",
			cache->cache.info.u.nscookiejar.filepath);
	unlink(filepath);

	if (lws_fi(&cache->cache.info.cx->fic, "cache_regen_temp_open"))
		goto bail;

	ctx.fdt = open(filepath, LWS_O_CREAT | LWS_O_WRONLY, 0600);
	if (ctx.fdt < 0)
		goto bail;

	/* magic header */

	if (lws_fi(&cache->cache.info.cx->fic, "cache_regen_temp_write") ||
	/* other consumers insist to see this at start of cookie jar */
	    write(ctx.fdt, "# Netscape HTTP Cookie File\n", 28) != 28)
		goto bail1;

	/* if we are adding something, put it first */

	if (pay &&
	    write(ctx.fdt, pay, /*msvc*/(unsigned int)pay_size) !=
						    (ssize_t)pay_size)
		goto bail1;
	if (pay && write(ctx.fdt, "\n", 1u) != (ssize_t)1)
		goto bail1;

	cache->cache.current_footprint = 0;

	ctx.wildcard_key_delete = wc_delete;
	ctx.add_data = pay;
	ctx.add_size = pay_size;
	ctx.curr = lws_now_usecs();
	ctx.drop = 0;

	cache->earliest_expiry = 0;

	if (lws_fi(&cache->cache.info.cx->fic, "cache_regen_iter_fail") ||
	    nscookiejar_iterate(cache, fd, nsc_regen_cb, &ctx))
		goto bail1;

	close(ctx.fdt);
	ctx.fdt = -1;

	if (unlink(cache->cache.info.u.nscookiejar.filepath) == -1)
		lwsl_info("%s: unlink %s failed\n", __func__,
			  cache->cache.info.u.nscookiejar.filepath);
	if (rename(filepath, cache->cache.info.u.nscookiejar.filepath) == -1)
		lwsl_info("%s: rename %s failed\n", __func__,
			  cache->cache.info.u.nscookiejar.filepath);

	if (cache->earliest_expiry)
		lws_cache_schedule(&cache->cache, expiry_cb,
				   cache->earliest_expiry);

	ret = 0;
	goto bail;

bail1:
	if (ctx.fdt >= 0)
		close(ctx.fdt);
bail:
	unlink(filepath);

	nsc_backing_close_unlock(cache, fd);

	return ret;
}

static void
expiry_cb(lws_sorted_usec_list_t *sul)
{
	lws_cache_nscookiejar_t *cache = lws_container_of(sul,
					lws_cache_nscookiejar_t, cache.sul);

	/*
	 * regen the cookie jar without changes, so expired are removed and
	 * new earliest expired computed
	 */
	if (nsc_regen(cache, NULL, NULL, 0))
		return;

	if (cache->earliest_expiry)
		lws_cache_schedule(&cache->cache, expiry_cb,
				   cache->earliest_expiry);
}


/* specific_key and expiry are ignored, since it must be encoded in payload */

static int
lws_cache_nscookiejar_write(struct lws_cache_ttl_lru *_c,
			    const char *specific_key, const uint8_t *source,
			    size_t size, lws_usec_t expiry, void **ppvoid)
{
	lws_cache_nscookiejar_t *cache = (lws_cache_nscookiejar_t *)_c;
	char tag[128];

	lwsl_cache("%s: %s: len %d\n", __func__, _c->info.name, (int)size);

	assert(source);

	if (nsc_line_to_tag((const char *)source, size, tag, sizeof(tag), NULL))
		return 1;

	if (ppvoid)
		*ppvoid = NULL;

	if (nsc_regen(cache, tag, source, size)) {
		lwsl_err("%s: regen failed\n", __func__);

		return 1;
	}

	return 0;
}

struct nsc_get_ctx {
	struct lws_buflist	*buflist;
	const char		*specific_key;
	const void		**pdata;
	size_t			*psize;
	lws_cache_ttl_lru_t	*l1;
	lws_usec_t		expiry;
};

/*
 * We're looking for a specific key, if found, we want to make an entry for it
 * in L1 and return information about that
 */

static int
nsc_get_cb(lws_cache_nscookiejar_t *cache, void *opaque, int flags,
	   const char *buf, size_t size)
{
	struct nsc_get_ctx *ctx = (struct nsc_get_ctx *)opaque;
	char tag[200];
	uint8_t *q;

	if (ctx->buflist)
		goto collect;

	if (!(flags & LCN_SOL))
		return NIR_CONTINUE;

	if (nsc_line_to_tag(buf, size, tag, sizeof(tag), &ctx->expiry)) {
		lwsl_err("%s: can't get tag\n", __func__);
		return NIR_CONTINUE;
	}

	lwsl_cache("%s: %s %s\n", __func__, ctx->specific_key, tag);

	if (strcmp(ctx->specific_key, tag)) {
		lwsl_cache("%s: no match\n", __func__);
		return NIR_CONTINUE;
	}

	/* it's a match */

	lwsl_cache("%s: IS match\n", __func__);

	if (!(flags & LCN_EOL))
		goto collect;

	/* it all fit in the buffer, let's create it in L1 now */

	*ctx->psize = size;
	if (ctx->l1->info.ops->write(ctx->l1,
				     ctx->specific_key, (const uint8_t *)buf,
				     size, ctx->expiry, (void **)ctx->pdata))
		return NIR_FINISH_ERROR;

	return NIR_FINISH_OK;

collect:
	/*
	 * it's bigger than one buffer-load, we have to stash what we're getting
	 * on a buflist and create it when we have it all
	 */

	if (lws_buflist_append_segment(&ctx->buflist, (const uint8_t *)buf,
				       size))
		goto cleanup;

	if (!(flags & LCN_EOL))
		return NIR_CONTINUE;

	/* we have all the payload, create the L1 entry without payload yet */

	*ctx->psize = size;
	if (ctx->l1->info.ops->write(ctx->l1, ctx->specific_key, NULL,
				     lws_buflist_total_len(&ctx->buflist),
				     ctx->expiry, (void **)&q))
		goto cleanup;
	*ctx->pdata = q;

	/* dump the buflist into the L1 cache entry */

	do {
		uint8_t *p;
		size_t len = lws_buflist_next_segment_len(&ctx->buflist, &p);

		memcpy(q, p, len);
		q += len;

		lws_buflist_use_segment(&ctx->buflist, len);
	} while (ctx->buflist);

	return NIR_FINISH_OK;

cleanup:
	lws_buflist_destroy_all_segments(&ctx->buflist);

	return NIR_FINISH_ERROR;
}

static int
lws_cache_nscookiejar_get(struct lws_cache_ttl_lru *_c,
			  const char *specific_key, const void **pdata,
			  size_t *psize)
{
	lws_cache_nscookiejar_t *cache = (lws_cache_nscookiejar_t *)_c;
	struct nsc_get_ctx ctx;
	int ret, fd;

	fd = nsc_backing_open_lock(cache, LWS_O_RDONLY, __func__);
	if (fd < 0)
		return 1;

	/* get a pointer to l1 */
	ctx.l1 = &cache->cache;
	while (ctx.l1->child)
		ctx.l1 = ctx.l1->child;

	ctx.pdata = pdata;
	ctx.psize = psize;
	ctx.specific_key = specific_key;
	ctx.buflist = NULL;
	ctx.expiry = 0;

	ret = nscookiejar_iterate(cache, fd, nsc_get_cb, &ctx);

	nsc_backing_close_unlock(cache, fd);

	return ret != NIR_FINISH_OK;
}

static int
lws_cache_nscookiejar_invalidate(struct lws_cache_ttl_lru *_c,
				 const char *wc_key)
{
	lws_cache_nscookiejar_t *cache = (lws_cache_nscookiejar_t *)_c;

	return nsc_regen(cache, wc_key, NULL, 0);
}

static struct lws_cache_ttl_lru *
lws_cache_nscookiejar_create(const struct lws_cache_creation_info *info)
{
	lws_cache_nscookiejar_t *cache;

	cache = lws_fi(&info->cx->fic, "cache_createfail") ? NULL :
					lws_zalloc(sizeof(*cache), __func__);
	if (!cache)
		return NULL;

	cache->cache.info = *info;

	/*
	 * We need to scan the file, if it exists, and find the earliest
	 * expiry while cleaning out any expired entries
	 */
	expiry_cb(&cache->cache.sul);

	lwsl_notice("%s: create %s\n", __func__, info->name ? info->name : "?");

	return (struct lws_cache_ttl_lru *)cache;
}

static int
lws_cache_nscookiejar_expunge(struct lws_cache_ttl_lru *_c)
{
	lws_cache_nscookiejar_t *cache = (lws_cache_nscookiejar_t *)_c;
	int r;

	if (!cache)
		return 0;

	r = unlink(cache->cache.info.u.nscookiejar.filepath);
	if (r)
		lwsl_warn("%s: failed to unlink %s\n", __func__,
				cache->cache.info.u.nscookiejar.filepath);

	return r;
}

static void
lws_cache_nscookiejar_destroy(struct lws_cache_ttl_lru **_pc)
{
	lws_cache_nscookiejar_t *cache = (lws_cache_nscookiejar_t *)*_pc;

	if (!cache)
		return;

	lws_sul_cancel(&cache->cache.sul);

	lws_free_set_NULL(*_pc);
}

#if defined(_DEBUG)

static int
nsc_dump_cb(lws_cache_nscookiejar_t *cache, void *opaque, int flags,
	      const char *buf, size_t size)
{
	lwsl_hexdump_cache(buf, size);

	return 0;
}

static void
lws_cache_nscookiejar_debug_dump(struct lws_cache_ttl_lru *_c)
{
	lws_cache_nscookiejar_t *cache = (lws_cache_nscookiejar_t *)_c;
	int fd = nsc_backing_open_lock(cache, LWS_O_RDONLY, __func__);

	if (fd < 0)
		return;

	lwsl_cache("%s: %s\n", __func__, _c->info.name);

	nscookiejar_iterate(cache, fd, nsc_dump_cb, NULL);

	nsc_backing_close_unlock(cache, fd);
}
#endif

const struct lws_cache_ops lws_cache_ops_nscookiejar = {
	.create			= lws_cache_nscookiejar_create,
	.destroy		= lws_cache_nscookiejar_destroy,
	.expunge		= lws_cache_nscookiejar_expunge,

	.write			= lws_cache_nscookiejar_write,
	.tag_match		= lws_cache_nscookiejar_tag_match,
	.lookup			= lws_cache_nscookiejar_lookup,
	.invalidate		= lws_cache_nscookiejar_invalidate,
	.get			= lws_cache_nscookiejar_get,
#if defined(_DEBUG)
	.debug_dump		= lws_cache_nscookiejar_debug_dump,
#endif
};
