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
#include <pthread.h>

#include <libwebsockets.h>
#include "private-lib-core.h"

#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <time.h>
#include <errno.h>
#include <stdarg.h>

#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>

#if defined(__APPLE__)
#include <sys/dirent.h>
/* Travis OSX does not have DT_REG... */
#if !defined(DT_REG)
#define DT_REG 8
#endif
#endif

struct file_entry {
	lws_list_ptr sorted;
	lws_list_ptr prev;
	char name[64];
	time_t modified;
	size_t size;
};

struct lws_diskcache_scan {
	struct file_entry *batch;
	const char *cache_dir_base;
	lws_list_ptr head;
	time_t last_scan_completed;
	uint64_t agg_size;
	uint64_t cache_size_limit;
	uint64_t avg_size;
	uint64_t cache_tries;
	uint64_t cache_hits;
	int cache_subdir;
	int batch_in_use;
	int agg_file_count;
	int secs_waiting;
};

#define KIB (1024)
#define MIB (KIB * KIB)

#define lp_to_fe(p, _n) lws_list_ptr_container(p, struct file_entry, _n)

static const char *hex = "0123456789abcdef";

#define BATCH_COUNT 128

static int
fe_modified_sort(lws_list_ptr a, lws_list_ptr b)
{
	struct file_entry *p1 = lp_to_fe(a, sorted), *p2 = lp_to_fe(b, sorted);

	return (int)((long)p2->modified - (long)p1->modified);
}

struct lws_diskcache_scan *
lws_diskcache_create(const char *cache_dir_base, uint64_t cache_size_limit)
{
	struct lws_diskcache_scan *lds = lws_malloc(sizeof(*lds), "cachescan");

	if (!lds)
		return NULL;

	memset(lds, 0, sizeof(*lds));

	lds->cache_dir_base = cache_dir_base;
	lds->cache_size_limit = cache_size_limit;

	return lds;
}

void
lws_diskcache_destroy(struct lws_diskcache_scan **lds)
{
	if ((*lds)->batch)
		lws_free((*lds)->batch);
	lws_free(*lds);
	*lds = NULL;
}

int
lws_diskcache_prepare(const char *cache_base_dir, int mode, uid_t uid)
{
	char dir[256];
	int n, m;

	(void)mkdir(cache_base_dir, (unsigned short)mode);
	if (chown(cache_base_dir, uid, (gid_t)-1))
		lwsl_err("%s: %s: unable to chown %d\n", __func__,
			 cache_base_dir, uid);

	for (n = 0; n < 16; n++) {
		lws_snprintf(dir, sizeof(dir), "%s/%c", cache_base_dir, hex[n]);
		(void)mkdir(dir, (mode_t)mode);
		if (chown(dir, uid, (uid_t)-1))
			lwsl_err("%s: %s: unable to chown %d\n", __func__,
						 dir, uid);
		for (m = 0; m < 16; m++) {
			lws_snprintf(dir, sizeof(dir), "%s/%c/%c",
				     cache_base_dir, hex[n], hex[m]);
			(void)mkdir(dir, (mode_t)mode);
			if (chown(dir, uid, (uid_t)-1))
				lwsl_err("%s: %s: unable to chown %d\n",
					 __func__, dir, uid);
		}
	}

	return 0;
}

/* copies and then truncates the incoming name, and renames the file at the
 * untruncated path to have the new truncated name */

int
lws_diskcache_finalize_name(char *cache)
{
	char ren[256], *p;

	strncpy(ren, cache, sizeof(ren) - 1);
	ren[sizeof(ren) - 1] = '\0';
	p = strchr(cache, '~');
	if (p) {
		*p = '\0';
		if (rename(ren, cache)) {
			lwsl_err("%s: problem renaming %s to %s\n", __func__,
				 ren, cache);
			return 1;
		}

		return 0;
	}

	return 1;
}

int
lws_diskcache_query(struct lws_diskcache_scan *lds, int is_bot,
		    const char *hash_hex, int *_fd, char *cache, int cache_len,
		    size_t *extant_cache_len)
{
	struct stat s;
	int n;

	/* caching is disabled? */
	if (!lds->cache_dir_base)
		return LWS_DISKCACHE_QUERY_NO_CACHE;

	if (!is_bot)
		lds->cache_tries++;

	n = lws_snprintf(cache, (size_t)cache_len, "%s/%c/%c/%s", lds->cache_dir_base,
			 hash_hex[0], hash_hex[1], hash_hex);

	lwsl_info("%s: job cache %s\n", __func__, cache);

	*_fd = open(cache, O_RDONLY);
	if (*_fd >= 0) {
		int fd;

		if (!is_bot)
			lds->cache_hits++;

		if (fstat(*_fd, &s)) {
			close(*_fd);

			return LWS_DISKCACHE_QUERY_NO_CACHE;
		}

		*extant_cache_len = (size_t)s.st_size;

		/* "touch" the hit cache file so it's last for LRU now */
		fd = open(cache, O_RDWR);
		if (fd >= 0)
			close(fd);

		return LWS_DISKCACHE_QUERY_EXISTS;
	}

	/* bots are too random to pollute the cache with their antics */
	if (is_bot)
		return LWS_DISKCACHE_QUERY_NO_CACHE;

	/* let's create it first with a unique temp name */

	lws_snprintf(cache + n, (size_t)cache_len - (unsigned int)n, "~%d-%p", (int)getpid(),
		     extant_cache_len);

	*_fd = open(cache, O_RDWR | O_CREAT | O_TRUNC, 0600);
	if (*_fd < 0) {
		/* well... ok... we will proceed without cache then... */
		lwsl_notice("%s: Problem creating cache %s: errno %d\n",
			    __func__, cache, errno);
		return LWS_DISKCACHE_QUERY_NO_CACHE;
	}

	return LWS_DISKCACHE_QUERY_CREATING;
}

int
lws_diskcache_secs_to_idle(struct lws_diskcache_scan *lds)
{
	return lds->secs_waiting;
}

/*
 * The goal is to collect the oldest BATCH_COUNT filepaths and filesizes from
 * the dirs under the cache dir.  Since we don't need or want a full list of
 * files in there in memory at once, we restrict the linked-list size to
 * BATCH_COUNT entries, and once it is full, simply ignore any further files
 * that are newer than the newest one on that list.  Files older than the
 * newest guy already on the list evict the newest guy already on the list
 * and are sorted into the correct order.  In this way no matter the number
 * of files to be processed the memory requirement is fixed at BATCH_COUNT
 * struct file_entry-s.
 *
 * The oldest subset of BATCH_COUNT files are sorted into the cd->batch
 * allocation in more recent -> least recent order.
 *
 * We want to track the total size of all files we saw as well, so we know if
 * we need to actually do anything yet to restrict how much space it's taking
 * up.
 *
 * And we want to do those things statefully and incrementally instead of one
 * big atomic operation, since the user may want a huge cache, so we look in
 * one cache dir at a time and track state in the repodir struct.
 *
 * When we have seen everything, we add the doubly-linked prev pointers and then
 * if we are over the limit, start deleting up to BATCH_COUNT files working back
 * from the end.
 */

int
lws_diskcache_trim(struct lws_diskcache_scan *lds)
{
	size_t cache_size_limit = (size_t)lds->cache_size_limit;
	char dirpath[132], filepath[132 + 32];
	lws_list_ptr lp, op = NULL;
	int files_trimmed = 0;
	struct file_entry *p;
	int fd, n, ret = -1;
	size_t trimmed = 0;
	struct dirent *de;
	struct stat s;
	DIR *dir;

	if (!lds->cache_subdir) {

		if (lds->last_scan_completed + lds->secs_waiting > time(NULL))
			return 0;

		lds->batch = lws_malloc(sizeof(struct file_entry) *
				BATCH_COUNT, "cache_trim");
		if (!lds->batch) {
			lwsl_err("%s: OOM\n", __func__);

			return 1;
		}
		lds->agg_size = 0;
		lds->head = NULL;
		lds->batch_in_use = 0;
		lds->agg_file_count = 0;
	}

	lws_snprintf(dirpath, sizeof(dirpath), "%s/%c/%c",
		     lds->cache_dir_base, hex[(lds->cache_subdir >> 4) & 15],
		     hex[lds->cache_subdir & 15]);

	dir = opendir(dirpath);
	if (!dir) {
		lwsl_err("Unable to walk repo dir '%s'\n",
			 lds->cache_dir_base);
		return -1;
	}

	do {
		de = readdir(dir);
		if (!de)
			break;

		if (de->d_type != DT_REG)
			continue;

		lds->agg_file_count++;

		lws_snprintf(filepath, sizeof(filepath), "%s/%s", dirpath,
			     de->d_name);

		fd = open(filepath, O_RDONLY);
		if (fd < 0) {
			lwsl_err("%s: cannot open %s\n", __func__, filepath);

			continue;
		}

		n = fstat(fd, &s);
		close(fd);
		if (n) {
			lwsl_notice("%s: cannot stat %s\n", __func__, filepath);
			continue;
		}

		lds->agg_size += (uint64_t)s.st_size;

		if (lds->batch_in_use == BATCH_COUNT) {
			/*
			 * once we filled up the batch with candidates, we don't
			 * need to consider any files newer than the newest guy
			 * on the list...
			 */
			if (lp_to_fe(lds->head, sorted)->modified < s.st_mtime)
				continue;

			/*
			 * ... and if we find an older file later, we know it
			 * will be replacing the newest guy on the list, so use
			 * that directly...
			 */
			p = lds->head;
			lds->head = p->sorted;
		} else
			/* we are still accepting anything to fill the batch */

			p = &lds->batch[lds->batch_in_use++];

		p->sorted = NULL;
		strncpy(p->name, de->d_name, sizeof(p->name) - 1);
		p->name[sizeof(p->name) - 1] = '\0';
		p->modified = s.st_mtime;
		p->size = (size_t)s.st_size;

		lws_list_ptr_insert(&lds->head, &p->sorted, fe_modified_sort);
	} while (de);

	ret = 0;

	lds->cache_subdir++;
	if (lds->cache_subdir != 0x100)
		goto done;

	/* we completed the whole scan... */

	/* if really no guidence, then 256MiB */
	if (!cache_size_limit)
		cache_size_limit = 256 * 1024 * 1024;

	if (lds->agg_size > cache_size_limit) {

		/* apply prev pointers to make the list doubly-linked */

		lp = lds->head;
		while (lp) {
			p = lp_to_fe(lp, sorted);

			p->prev = op;
			op = &p->prev;
			lp = p->sorted;
		}

		/*
		 * reverse the list (start from tail, now traverse using
		 * .prev)... it's oldest-first now...
		 */

		lp = op;

		while (lp && lds->agg_size > cache_size_limit) {
			p = lp_to_fe(lp, prev);

			lws_snprintf(filepath, sizeof(filepath), "%s/%c/%c/%s",
				     lds->cache_dir_base, p->name[0],
				     p->name[1], p->name);

			if (!unlink(filepath)) {
				lds->agg_size -= p->size;
				trimmed += p->size;
				files_trimmed++;
			} else
				lwsl_notice("%s: Failed to unlink %s\n",
					    __func__, filepath);

			lp = p->prev;
		}

		if (files_trimmed)
			lwsl_notice("%s: %s: trimmed %d files totalling "
				    "%lldKib, leaving %lldMiB\n", __func__,
				    lds->cache_dir_base, files_trimmed,
				    ((unsigned long long)trimmed) / KIB,
				    ((unsigned long long)lds->agg_size) / MIB);
	}

	if (lds->agg_size && lds->agg_file_count)
		lds->avg_size = lds->agg_size / (uint64_t)lds->agg_file_count;

	/*
	 * estimate how long we can go before scanning again... default we need
	 * to start again immediately
	 */

	lds->last_scan_completed = time(NULL);
	lds->secs_waiting = 1;

	if (lds->agg_size < cache_size_limit) {
		uint64_t avg = 4096, capacity, projected;

		/* let's use 80% of the real average for margin */
		if (lds->agg_size && lds->agg_file_count)
			avg = ((lds->agg_size * 8) / (uint64_t)lds->agg_file_count) / 10;

		/*
		 * if we collected BATCH_COUNT files of the average size,
		 * how much can we clean up in 256s?
		 */

		capacity = avg * BATCH_COUNT;

		/*
		 * if the cache grew by 10%, would we hit the limit even then?
		 */
		projected = (lds->agg_size * 11) / 10;
		if (projected < cache_size_limit)
			/* no... */
			lds->secs_waiting  = (int)((256 / 2) * ((cache_size_limit -
						    projected) / capacity));

		/*
		 * large waits imply we may not have enough info yet, so
		 * check once an hour at least.
		 */

		if (lds->secs_waiting > 3600)
			lds->secs_waiting = 3600;
	} else
		lds->secs_waiting = 0;

	lwsl_info("%s: cache %s: %lldKiB / %lldKiB, next scan %ds\n", __func__,
		  lds->cache_dir_base,
		  (unsigned long long)lds->agg_size / KIB,
		  (unsigned long long)cache_size_limit / KIB,
		  lds->secs_waiting);

	lws_free(lds->batch);
	lds->batch = NULL;

	lds->cache_subdir = 0;

done:
	closedir(dir);

	return ret;
}
