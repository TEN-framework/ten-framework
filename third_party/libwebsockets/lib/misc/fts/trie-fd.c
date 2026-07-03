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
#include "private-lib-misc-fts.h"

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#define AC_COUNT_STASHED_CHILDREN 8

struct ch {
	jg2_file_offset ofs;
	char name[64];
	int inst;
	int child_agg;
	int name_length;
	int effpos;
	int descendents;
};

struct wac {
	struct ch ch[AC_COUNT_STASHED_CHILDREN];

	jg2_file_offset self;
	jg2_file_offset tifs;
	int child_count;
	int child;

	int agg;
	int desc;
	char done_children;
	char once;
};

struct linetable {
	struct linetable *next;

	int chunk_line_number_start;
	int chunk_line_number_count;

	off_t chunk_filepos_start;

	off_t vli_ofs_in_index;
};

static uint32_t
b32(unsigned char *b)
{
	return (uint32_t)((b[0] << 24) | (b[1] << 16) | (b[2] << 8) | b[3]);
}

static uint16_t
b16(unsigned char *b)
{
	return (uint16_t)((b[0] << 8) | b[1]);
}

static int
lws_fts_filepath(struct lws_fts_file *jtf, int filepath_index, char *result,
		 size_t len, uint32_t *ofs_linetable, uint32_t *lines)
{
	unsigned char buf[256 + 15];
	uint32_t flen;
	int ra, bp = 0;
	size_t m;
	off_t o;

	if (filepath_index > jtf->filepaths)
		return 1;

	if (lseek(jtf->fd, (off_t)(jtf->filepath_table + (4 * (unsigned int)filepath_index)),
			SEEK_SET) < 0) {
		lwsl_err("%s: unable to seek\n", __func__);

		return 1;
	}

	ra = (int)read(jtf->fd, buf, 4);
	if (ra < 0)
		return 1;

	o = (off_t)b32(buf);
	if (lseek(jtf->fd, o, SEEK_SET) < 0) {
		lwsl_err("%s: unable to seek\n", __func__);

		return 1;
	}

	ra = (int)read(jtf->fd, buf, sizeof(buf));
	if (ra < 0)
		return 1;

	if (ofs_linetable)
		bp += rq32(&buf[bp], ofs_linetable);
	else
		bp += rq32(&buf[bp], &flen);
	if (lines)
		bp += rq32(&buf[bp], lines);
	else
		bp += rq32(&buf[bp], &flen);
	bp += rq32(&buf[bp], &flen);

	m = flen;
	if (len - 1 < m)
		m = flen - 1;

	strncpy(result, (char *)&buf[bp], m);
	result[m] = '\0';
	result[len - 1] = '\0';

	return 0;
}

/*
 * returns -1 for fail or fd open on the trie file.
 *
 * *root is set to the position of the root trie entry.
 * *flen is set to the length of the whole file
 */

int
lws_fts_adopt(struct lws_fts_file *jtf)
{
	unsigned char buf[256];
	off_t ot;

	if (read(jtf->fd, buf, TRIE_FILE_HDR_SIZE) != TRIE_FILE_HDR_SIZE) {
		lwsl_err("%s: unable to read file header\n", __func__);
		goto bail;
	}

	if (buf[0] != 0xca || buf[1] != 0x7a ||
	    buf[2] != 0x5f || buf[3] != 0x75) {
		lwsl_err("%s: bad magic %02X %02X %02X %02X\n", __func__,
			 buf[0], buf[1], buf[2], buf[3]);
		goto bail;
	}

	jtf->root = b32(&buf[4]);

	ot = lseek(jtf->fd, 0, SEEK_END);
	if (ot < 0) {
		lwsl_err("%s: unable to seek\n", __func__);

		goto bail;
	}
	jtf->flen = (jg2_file_offset)ot;

	if (jtf->flen != b32(&buf[8])) {
		lwsl_err("%s: file size doesn't match expected\n", __func__);

		goto bail;
	}

	jtf->filepath_table = b32(&buf[12]);
	jtf->filepaths = (int)b32(&buf[16]);

	return jtf->fd;

bail:
	return -1;
}

struct lws_fts_file *
lws_fts_open(const char *filepath)
{
	struct lws_fts_file *jtf;

	jtf = lws_malloc(sizeof(*jtf), "fts open");
	if (!jtf)
		goto bail1;

	jtf->fd = open(filepath, O_RDONLY);
	if (jtf->fd < 0) {
		lwsl_err("%s: unable to open %s\n", __func__, filepath);
		goto bail2;
	}

	if (lws_fts_adopt(jtf) < 0)
		goto bail3;

	return jtf;

bail3:
	close(jtf->fd);
bail2:
	lws_free(jtf);
bail1:
	return NULL;
}

void
lws_fts_close(struct lws_fts_file *jtf)
{
	close(jtf->fd);
	lws_free(jtf);
}

#define grab(_pos, _size) { \
		bp = 0; \
		if (lseek(jtf->fd, (off_t)(_pos), SEEK_SET) < 0) { \
			lwsl_err("%s: unable to seek\n", __func__); \
\
			goto bail; \
		} \
\
		ra = (int)read(jtf->fd, buf, (size_t)(_size)); \
		if (ra < 0) \
			goto bail; \
}

static struct linetable *
lws_fts_cache_chunktable(struct lws_fts_file *jtf, uint32_t ofs_linetable,
			 struct lwsac **linetable_head)
{
	struct linetable *lt, *first = NULL, **prev = NULL;
	unsigned char buf[8];
	int line = 1, bp, ra;
	off_t cfs = 0;

	*linetable_head = NULL;

	do {
		grab(ofs_linetable, sizeof(buf));

		lt = lwsac_use(linetable_head, sizeof(*lt), 0);
		if (!lt)
			goto bail;
		if (!first)
			first = lt;

		lt->next = NULL;
		if (prev)
			*prev = lt;
		prev = &lt->next;

		lt->chunk_line_number_start = line;
		lt->chunk_line_number_count = b16(&buf[bp + 2]);
		lt->vli_ofs_in_index = (off_t)(ofs_linetable + 8);
		lt->chunk_filepos_start = cfs;

		line += lt->chunk_line_number_count;

		cfs += (int32_t)b32(&buf[bp + 4]);
		ofs_linetable += b16(&buf[bp]);

	} while (b16(&buf[bp]));

	return first;

bail:
	lwsac_free(linetable_head);

	return NULL;
}

static int
lws_fts_getfileoffset(struct lws_fts_file *jtf, struct linetable *ltstart,
		      int line, off_t *_ofs)
{
	struct linetable *lt = ltstart;
	unsigned char buf[LWS_FTS_LINES_PER_CHUNK * 5];
	uint32_t ll;
	off_t ofs;
	int bp, ra;

	/* first figure out which chunk */

	do {
		if (line >= lt->chunk_line_number_start &&
		    line < lt->chunk_line_number_start +
		    	    lt->chunk_line_number_count)
			break;

		lt = lt->next;
	} while (lt);

	if (!lt)
		goto bail;

	/* we know it's in this chunk */

	ofs = lt->chunk_filepos_start;
	line -= lt->chunk_line_number_start;

	grab(lt->vli_ofs_in_index, sizeof(buf));

	bp = 0;
	while (line) {
		bp += rq32(&buf[bp], &ll);
		ofs += (int32_t)ll;
		line--;
	}

	/* we know the offset it is at in the original file */

	*_ofs = ofs;

	return 0;

bail:
	lwsl_info("%s: bail %d\n", __func__, line);

	return 1;
}

static int
ac_record(struct lws_fts_file *jtf, struct lwsac **results_head,
	  const char *needle, int pos, struct wac *s, int sp,
	  uint32_t instances, uint32_t agg_instances, uint32_t children,
	  struct lws_fts_result_autocomplete ***ppac)
{
	struct lws_fts_result_autocomplete *ac;
	int n, m;
	char *p;

	if (!instances && !agg_instances)
		return 1;

	m = pos;
	for (n = 1; n <= sp; n++)
		m += s[n].ch[s[n].child - 1].name_length;

	ac = lwsac_use(results_head, sizeof(*ac) + (unsigned int)m + 1, 0);
	if (!ac)
		return -1;

	p = (char *)(ac + 1);

	**ppac = ac;
	ac->next = NULL;
	*ppac = &ac->next;
	ac->instances = (int)instances;
	ac->agg_instances = (int)agg_instances;
	ac->ac_length = m;
	ac->has_children = !!children;
	ac->elided = 0;

	memcpy(p, needle, (size_t)pos);
	p += pos;

	for (n = 1; n <= sp; n++) {
		int w = s[n].child - 1;

		memcpy(p, s[n].ch[w].name, (size_t)s[n].ch[w].name_length);
		p += s[n].ch[w].name_length;
	}
	p = (char *)(ac + 1);
	p[m] = '\0';

	/*
	 * deduct this child's instance weight from his antecdents to track
	 * relative path attractiveness dynamically, after we already used its
	 * best results (children are sorted best-first)
	 */
	for (n = sp; n >= 0; n--) {
		s[n].ch[s[n].child - 1].child_agg -= (int)instances;
		s[n].agg -= (int)instances;
	}

	return 0;
}

struct lws_fts_result *
lws_fts_search(struct lws_fts_file *jtf, struct lws_fts_search_params *ftsp)
{
	uint32_t children, instances, co, sl, agg, slt, chunk,
		 fileofs_tif_start, desc, agg_instances;
	int pos = 0, n, m, nl, bp, base = 0, ra, palm, budget, sp, ofd = -1;
	unsigned long long tf = (unsigned long long)lws_now_usecs();
	struct lws_fts_result_autocomplete **pac = NULL;
	char stasis, nac = 0, credible, needle[32];
	struct lws_fts_result_filepath *fp;
	struct lws_fts_result *result;
	unsigned char buf[4096];
	off_t o, child_ofs;
	struct wac s[128];

	ftsp->results_head = NULL;

	if (!ftsp->needle)
		return NULL;

	nl = (int)strlen(ftsp->needle);
	if ((size_t)nl > sizeof(needle) - 2)
		return NULL;

	result = lwsac_use(&ftsp->results_head, sizeof(*result), 0);
	if (!result)
		return NULL;

	/* start with no results... */

	result->autocomplete_head = NULL;
	pac = &result->autocomplete_head;
	result->filepath_head = NULL;
	result->duration_ms = 0;
	result->effective_flags = ftsp->flags;

	palm = 0;

	for (n = 0; n < nl; n++)
		needle[n] = (char)tolower(ftsp->needle[n]);
	needle[nl] = '\0';

	o = (off_t)jtf->root;
	do {
		bp = 0;
		base = 0;

		grab(o, sizeof(buf));

		child_ofs = o + bp;
		bp += rq32(&buf[bp], &fileofs_tif_start);
		bp += rq32(&buf[bp], &children);
		bp += rq32(&buf[bp], &instances);
		bp += rq32(&buf[bp], &agg_instances);
		palm = pos;

		/* the children follow here */

		if (pos == nl) {

			nac = 0;
			if (!fileofs_tif_start)
				/*
				 * we matched, but there are no instances of
				 * this, it's actually an intermediate
				 */

				goto autocomp;

			/* we leave with bp positioned at the instance list */

			o = (off_t)fileofs_tif_start;
			grab(o, sizeof(buf));
			break;
		}

		if (ra - bp < 1024) {

			/*
			 * We don't have enough.  So reload the buffer starting
			 * at where we got to.
			 */

			base += bp;
			grab(o + base, sizeof(buf));
		}

		/* gets set if any child COULD match needle if it went on */

		credible = 0;
		for (n = 0; (uint32_t)n < children; n++) {
			uint32_t inst;

			bp += rq32(&buf[bp], &co);
			bp += rq32(&buf[bp], &inst);
			bp += rq32(&buf[bp], &agg);
			bp += rq32(&buf[bp], &desc);
			bp += rq32(&buf[bp], &sl);

			if (sl > (uint32_t)(nl - pos)) {

				/*
				 * it can't be a match because it's longer than
				 * our needle string (but that leaves it as a
				 * perfectly fine autocomplete candidate)
				 */
				size_t g = (size_t)(nl - pos);

				/*
				 * "credible" means at least one child matches
				 * all the chars in needle up to as many as it
				 * has.  If not "credible" this path cannot
				 * match.
				 */
				if (!strncmp((char *)&buf[bp], &needle[pos], g))
					credible = 1;
				else
					/*
					 * deflate the parent agg using the
					 * knowledge this child is not on the
					 * path shown by the remainder of needle
					 */
					agg_instances -= agg;

				nac = 0;
				bp += (int)sl;
				slt = 0;
				pos = palm;
				goto ensure;
			}

			/* the comparison string potentially has huge length */

			slt = sl;
			while (slt) {

				/*
				 * the strategy is to compare whatever we have
				 * lying around, then bring in more if it didn't
				 * fail to match yet.  That way we don't bring
				 * in anything we could already have known was
				 * not needed due to a match fail.
				 */

				chunk = (uint32_t)(ra - bp);
				if (chunk > slt)
					chunk = slt;

				if ((chunk == 1 && needle[pos] != buf[bp]) ||
				    (chunk != 1 &&
				     memcmp(&needle[pos], &buf[bp], chunk))) {

					/*
					 * it doesn't match... so nothing can
					 * autocomplete this...
					 */
					bp += (int)slt;
					slt = 0;
					nac = 1;
					goto ensure;
				}

				slt -= chunk;
				pos += (int)chunk;
				bp += (int)chunk;

				/* so far, it matches */

				if (!slt) {
					/* we matched the whole thing */
					o = (int32_t)co;
					if (!co)
						goto bail;
					n = (int)children;
					credible = 1;
				}

ensure:
				/*
				 * do we have at least buf more to match, or the
				 * remainder of the string, whichever is less?
				 *
				 * bp may exceed sizeof(buf) on no match path
				 */
				chunk = sizeof(buf);
				if (slt < chunk)
					chunk = slt;

				if (ra - bp >= (int)chunk)
					continue;

				/*
				 * We don't have enough.  So reload buf starting
				 * at where we got to.
				 */
				base += bp;
				grab(o + base, sizeof(buf));

			} /* while we are still comparing */

		} /* for each child */

		if ((uint32_t)n == children) {
			if (!credible)
				goto bail;

			nac = 0;
			goto autocomp;
		}
	} while(1);

	result->duration_ms = (int)(((uint64_t)lws_now_usecs() - tf) / 1000);

	if (!instances && !children)
		return result;

	/* the match list may easily exceed one read buffer load ... */

	o += bp;

	/*
	 * Only do the file match list if it was requested in the search flags
	 */

	if (!(ftsp->flags & LWSFTS_F_QUERY_FILES))
		goto autocomp;

	do {
		uint32_t fi, tot, line, ro, ofs_linetable, lines, fplen,
			*u, _o;
		struct lwsac *lt_head = NULL;
		struct linetable *ltst;
		char path[256], *pp;
		int footprint;
		off_t fo;

		ofd = -1;
		grab(o, sizeof(buf));

		ro = (uint32_t)o;
		bp += rq32(&buf[bp], &_o);
		o = (off_t)_o;

		assert(!o || o > TRIE_FILE_HDR_SIZE);

		bp += rq32(&buf[bp], &fi);
		bp += rq32(&buf[bp], &tot);

		if (lws_fts_filepath(jtf, (int)fi, path, sizeof(path) - 1,
				     &ofs_linetable, &lines)) {
			lwsl_err("can't get filepath index %d\n", fi);
			goto bail;
		}

		if (ftsp->only_filepath && strcmp(path, ftsp->only_filepath))
			continue;

		ltst = lws_fts_cache_chunktable(jtf, ofs_linetable, &lt_head);
		if (!ltst)
			goto bail;

		if (ftsp->flags & LWSFTS_F_QUERY_QUOTE_LINE) {
			ofd = open(path, O_RDONLY);
			if (ofd < 0) {
				lwsac_free(&lt_head);
				goto bail;
			}
		}

		fplen = (uint32_t)strlen(path);
		footprint = (int)(sizeof(*fp) + fplen + 1);
		if (ftsp->flags & LWSFTS_F_QUERY_FILE_LINES) {
			/* line number and offset in file */
			footprint += (int)(2 * sizeof(uint32_t) * tot);

			if (ftsp->flags & LWSFTS_F_QUERY_QUOTE_LINE)
				/* pointer to quote string */
				footprint += (int)(sizeof(void *) * tot);
		}

		fp = lwsac_use(&ftsp->results_head, (unsigned int)footprint, 0);
		if (!fp) {
			lwsac_free(&lt_head);
			goto bail;
		}

		fp->filepath_length = (int)fplen;
		fp->lines_in_file = (int)lines;
		fp->matches = (int)tot;
		fp->matches_length = footprint - (int)sizeof(*fp) - (int)(fplen + 1);
		fp->next = result->filepath_head;
		result->filepath_head = fp;

		/* line table first so it can be aligned */

		u = (uint32_t*)(fp + 1);

		if (ftsp->flags & LWSFTS_F_QUERY_FILE_LINES) {

			/* for each line number */

			for (n = 0; (uint32_t)n < tot; n++) {

				unsigned char lbuf[256], *p;
				char ebuf[384];
				const char **v;
				int m;

				if ((ra - bp) < 8) {
					base += bp;
					grab((int32_t)ro + base, sizeof(buf));
				}

				bp += rq32(&buf[bp], &line);
				*u++ = line;

				if (lws_fts_getfileoffset(jtf, ltst, (int)line, &fo))
					continue;

				*u++ = (uint32_t)fo;

				if (!(ftsp->flags & LWSFTS_F_QUERY_QUOTE_LINE))
					continue;

				if (lseek(ofd, fo, SEEK_SET) < 0)
					continue;

				m = (int)read(ofd, lbuf, sizeof(lbuf) - 1);
				if (m < 0)
					continue;
				lbuf[sizeof(lbuf) - 1] = '\0';

				p = (unsigned char *)strchr((char *)lbuf, '\n');
				if (p)
					m = lws_ptr_diff(p, lbuf);
				lbuf[m] = '\0';
				p = (unsigned char *)strchr((char *)lbuf, '\r');
				if (p)
					m = lws_ptr_diff(p, lbuf);
				lbuf[m] = '\0';

				lws_json_purify(ebuf, (const char *)lbuf,
						sizeof(ebuf) - 1, NULL);
				m = (int)strlen(ebuf);

				p = lwsac_use(&ftsp->results_head, (unsigned int)m + 1, 0);
				if (!p) {
					lwsac_free(&lt_head);
					goto bail;
				}

				memcpy(p, ebuf, (unsigned int)m);
				p[m] = '\0';
				v = (const char **)u;
				*v = (const char *)p;
				u += sizeof(const char *) / sizeof(uint32_t);
			}
		}

		pp = ((char *)&fp[1]) + fp->matches_length;
		memcpy(pp, path, fplen);
		pp[fplen] = '\0';

		if (ofd >= 0) {
			close(ofd);
			ofd = -1;
		}

		lwsac_free(&lt_head);

		if (ftsp->only_filepath)
			break;

	} while (o);

	/* sort the instance file list by results density */

	do {
		struct lws_fts_result_filepath **prf, *rf1, *rf2;

		stasis = 1;

		/* bubble sort keeps going until nothing changed */

		prf = &result->filepath_head;
		while (*prf) {

			rf1 = *prf;
			rf2 = rf1->next;

			if (rf2 && rf1->lines_in_file && rf2->lines_in_file &&
			    ((rf1->matches * 1000) / rf1->lines_in_file) <
			    ((rf2->matches * 1000) / rf2->lines_in_file)) {
				stasis = 0;

				*prf = rf2;
				rf1->next = rf2->next;
				rf2->next = rf1;
			}

			prf = &(*prf)->next;
		}

	} while (!stasis);

autocomp:

	if (!(ftsp->flags & LWSFTS_F_QUERY_AUTOCOMPLETE) || nac)
		return result;

	/*
	 * autocomplete (ie, the descendent paths that yield the most hits)
	 *
	 * We actually need to spider the earliest terminal descendents from
	 * the child we definitely got past, and present the first n terminal
	 * strings.  The descendents are already sorted in order of highest
	 * aggregated hits in their descendents first, so simply collecting n
	 * earliest leaf children is enough.
	 *
	 * The leaf children may be quite deep down in a stack however.  So we
	 * have to go through all the walking motions collecting and retaining
	 * child into for when we come back up the walk.
	 *
	 * We can completely ignore file instances for this, we just need the
	 * earliest children.  And we can restrict how many children we stash
	 * in each stack level to eg, 5.
	 *
	 * child_ofs comes in pointing at the start of the trie entry that is
	 * to be the starting point for making suggestions.
	 */

	budget = ftsp->max_autocomplete;
	base = 0;
	bp = 0;
	pac = &result->autocomplete_head;
	sp = 0;
	if (pos > (int)sizeof(s[sp].ch[0].name) - 1)
		pos = (int)sizeof(s[sp].ch[0].name) - 1;

	memset(&s[sp], 0, sizeof(s[sp]));

	s[sp].child = 1;
	s[sp].tifs = fileofs_tif_start;
	s[sp].self = (jg2_file_offset)child_ofs;
	s[sp].ch[0].effpos = pos;

	if (pos == nl)
		n = ac_record(jtf, &ftsp->results_head, needle, pos, s, 0,
			      instances, agg_instances, children, &pac);

	while (sp >= 0 && budget) {
		int nobump = 0;
		struct ch *tch = &s[sp].ch[s[sp].child - 1];

		grab(child_ofs, sizeof(buf));

		bp += rq32(&buf[bp], &fileofs_tif_start);
		bp += rq32(&buf[bp], &children);
		bp += rq32(&buf[bp], &instances);
		bp += rq32(&buf[bp], &agg_instances);

		if (sp > 0 && s[sp - 1].done_children &&
		    tch->effpos + tch->name_length >= nl &&
		    tch->inst && fileofs_tif_start) {
			n = ac_record(jtf, &ftsp->results_head, needle, pos, s,
				      sp, (uint32_t)tch->inst, (uint32_t)tch->child_agg,
				      (uint32_t)tch->descendents, &pac);
			if (n < 0)
				goto bail;
			if (!n)
				if (--budget == 0)
					break;
		}

		if (!s[sp].done_children && children) {
			s[sp].done_children = 1;
			sp++;
			memset(&s[sp], 0, sizeof(s[sp]));
			s[sp].tifs = fileofs_tif_start;
			s[sp].self = (jg2_file_offset)child_ofs;

			for (n = 0; n < (int)children && s[sp].child_count <
					    (int)LWS_ARRAY_SIZE(s[0].ch); n++) {
				uint32_t slen, cho, agg, inst;
				int i = s[sp].child_count;
				struct ch *ch = &s[sp].ch[i];
				size_t max;

				bp += rq32(&buf[bp], &cho);
				bp += rq32(&buf[bp], &inst);
				bp += rq32(&buf[bp], &agg);
				bp += rq32(&buf[bp], &desc);
				bp += rq32(&buf[bp], &slen);

				max = slen;
				if (max > sizeof(ch->name) - 1)
					max = sizeof(ch->name) - 1;

				strncpy(ch->name, (char *)&buf[bp], max);
				bp += (int)slen;

				ch->name_length = (int)max;
				ch->name[sizeof(ch->name) - 1] = '\0';
				ch->inst = (int)inst;
				ch->effpos =
				       s[sp - 1].ch[s[sp - 1].child - 1].effpos;

				ch->child_agg = (int)agg;
				ch->descendents = (int)desc;

				/*
				 * if we have more needle chars than we matched
				 * to get this far, we can only allow potential
				 * matches that are consistent with the
				 * additional unmatched character(s)...
				 */

				m = nl - ch->effpos;
				if (m > ch->name_length)
					m = ch->name_length;

				if (m > 0 &&
				    strncmp(&needle[ch->effpos], ch->name, (unsigned int)m))
					continue;

				ch->effpos += m;
				s[sp].ch[s[sp].child_count++].ofs = cho;
			}

		}

		while (sp >= 0 && s[sp].child >= s[sp].child_count) {
			s[sp].done_children = 0;
			sp--;
		}

		/*
		 * Compare parent remaining agg vs parent's next siblings' still
		 * intact original agg... if the next sibling has more, abandon
		 * the parent path and go with the sibling... this keeps the
		 * autocomplete results related to popularity.
		 */

		nobump = 0;
		n = sp - 1;
		while (n >= 0) {
			struct lws_fts_result_autocomplete *ac =
				(struct lws_fts_result_autocomplete *)pac;

			if (s[n].child < s[n].child_count &&
			    s[n].ch[s[n].child - 1].child_agg <
			    	    s[n].ch[s[n].child].child_agg) {

				if (pac)
					/*
					 * mark the autocomplete result that
					 * there were more children down his
					 * path that we skipped in these results
					 */
					ac->elided = 1;

				for (m = n; m < sp + 1; m++)
					s[m].done_children = 0;
				sp = n;
				child_ofs = (off_t)s[sp].ch[s[sp].child++].ofs;
				nobump = 1;
			}

			n--;
		}

		if (nobump || sp < 0)
			continue;

		child_ofs = (off_t)s[sp].ch[s[sp].child++].ofs;
	}

	/* let's do a final sort into agg order */

	do {
		struct lws_fts_result_autocomplete *ac1, *ac2;

		stasis = 1;

		/* bubble sort keeps going until nothing changed */

		pac = &result->autocomplete_head;
		while (*pac) {

			ac1 = *pac;
			ac2 = ac1->next;

			if (ac2 && ac1->instances < ac2->instances) {
				stasis = 0;

				*pac = ac2;
				ac1->next = ac2->next;
				ac2->next = ac1;
			}

			pac = &(*pac)->next;
		}

	} while (!stasis);

	return result;

bail:
	if (ofd >= 0)
		close(ofd);

	lwsl_info("%s: search ended up at bail\n", __func__);

	return result;
}
