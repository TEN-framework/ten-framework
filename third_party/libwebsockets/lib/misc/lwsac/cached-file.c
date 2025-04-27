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

#if !defined(LWS_PLAT_OPTEE) && !defined(OPTEE_DEV_KIT)

#include "private-lib-core.h"
#include "private-lib-misc-lwsac.h"

/*
 * Helper for caching a file in memory in a lac, but also to check at intervals
 * no less than 5s if the file is still fresh.
 *
 * Set *cache to NULL the first time before calling.
 *
 * You should call this each time before using the cache... if it's
 *
 *  - less than 5s since the last freshness check, and
 *  - the file is already in memory
 *
 * it just returns with *cache left alone; this costs very little.  You should
 * call `lwsac_use_cached_file_start()` and `lwsac_use_cached_file_end()`
 * to lock the cache against deletion while you are using it.
 *
 * If it's
 *
 *  - at least 5s since the last freshness check, and
 *  - the file timestamp has changed
 *
 * then
 *
 *  - the file is reloaded into a new lac and *cache set to that
 *
 *  - the old cache lac, if any, is detached (so it will be freed when its
 *    reference count reaches zero, or immediately if nobody has it)
 *
 * Note the call can fail due to OOM or filesystem issue at any time.
 *
 *
 * After the LAC header there is stored a `struct cached_file_info` and then
 * the raw file contents.  *
 *
 *  [LAC header]
 *  [struct cached_file_info]
 *  [file contents]  <--- *cache is set to here
 *
 * The api returns a lwsac_cached_file_t type offset to point to the file
 * contents.  Helpers for reference counting and freeing are also provided
 * that take that type and know how to correct it back to operate on the LAC.
 */

#define cache_file_to_lac(c) ((struct lwsac *)((char *)c - \
			      sizeof(struct cached_file_info) - \
			      sizeof(struct lwsac_head) - \
			      sizeof(struct lwsac)))

void
lwsac_use_cached_file_start(lwsac_cached_file_t cache)
{
	struct lwsac *lac = cache_file_to_lac(cache);
	struct lwsac_head *lachead = (struct lwsac_head *)&lac->head[1];

	lachead->refcount++;
	// lwsl_debug("%s: html refcount: %d\n", __func__, lachead->refcount);
}

void
lwsac_use_cached_file_end(lwsac_cached_file_t *cache)
{
	struct lwsac *lac;
	struct lwsac_head *lachead;

	if (!cache || !*cache)
		return;

	lac = cache_file_to_lac(*cache);
	lachead = (struct lwsac_head *)&lac->head[1];

	if (!lachead->refcount)
		lwsl_err("%s: html refcount zero on entry\n", __func__);

	if (lachead->refcount && !--lachead->refcount && lachead->detached) {
		*cache = NULL; /* not usable any more */
		lwsac_free(&lac);
	}
}

void
lwsac_use_cached_file_detach(lwsac_cached_file_t *cache)
{
	struct lwsac *lac = cache_file_to_lac(*cache);
	struct lwsac_head *lachead = NULL;

	if (lac) {
		lachead = (struct lwsac_head *)&lac->head[1];

		lachead->detached = 1;
		if (lachead->refcount)
			return;
	}

	*cache = NULL;
	lwsac_free(&lac);
}

int
lwsac_cached_file(const char *filepath, lwsac_cached_file_t *cache, size_t *len)
{
	struct cached_file_info *info = NULL;
	lwsac_cached_file_t old = *cache;
	struct lwsac *lac = NULL;
	time_t t = time(NULL);
	unsigned char *a;
	struct stat s;
	size_t all;
	ssize_t rd;
	int fd;

	if (old) { /* we already have a cached copy of it */

		info = (struct cached_file_info *)((*cache) - sizeof(*info));

		if (t - info->last_confirm < 5)
			/* we checked it as fresh less than 5s ago, use old */
			return 0;
	}

	/*
	 * ...it's been 5s, we should check again on the filesystem
	 * that the file hasn't changed
	 */

	fd = open(filepath, O_RDONLY);
	if (fd < 0) {
		lwsl_err("%s: cannot open %s\n", __func__, filepath);

		return 1;
	}

	if (fstat(fd, &s)) {
		lwsl_err("%s: cannot stat %s\n", __func__, filepath);

		goto bail;
	}

	if (old && s.st_mtime == info->s.st_mtime) {
		/* it still seems to be the same as our cached one */
		info->last_confirm = t;

		close(fd);

		return 0;
	}

	/*
	 * we either didn't cache it yet, or it has changed since we cached
	 * it... reload in a new lac and then detach the old lac.
	 */

	all = sizeof(*info) + (unsigned long)s.st_size + 2;

	info = lwsac_use(&lac, all, all);
	if (!info)
		goto bail;

	info->s = s;
	info->last_confirm = t;

	a = (unsigned char *)(info + 1);

	*len = (unsigned long)s.st_size;
	a[s.st_size] = '\0';

	rd = read(fd, a, (unsigned long)s.st_size);
	if (rd != s.st_size) {
		lwsl_err("%s: cannot read %s (%d)\n", __func__, filepath,
			 (int)rd);
		goto bail1;
	}

	close(fd);

	*cache = (lwsac_cached_file_t)a;
	if (old)
		lwsac_use_cached_file_detach(&old);

	return 0;

bail1:
	lwsac_free(&lac);

bail:
	close(fd);

	return 1;
}

#endif
