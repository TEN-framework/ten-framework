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
 *
 * The functions allow
 *
 *  - collecting a concordance of strings from one or more files (eg, a
 *    directory of files) into a single in-memory, lac-backed trie;
 *
 *  - to optimize and serialize the in-memory trie to an fd;
 *
 *  - to very quickly report any instances of a string in any of the files
 *    indexed by the trie, by a seeking around a serialized trie fd, without
 *    having to load it all in memory
 */

#include "private-lib-core.h"
#include "private-lib-misc-fts.h"

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>

struct lws_fts_entry;

/* notice these are stored in t->lwsac_input_head which has input file scope */

struct lws_fts_filepath {
	struct lws_fts_filepath *next;
	struct lws_fts_filepath *prev;
	char filepath[256];
	jg2_file_offset ofs;
	jg2_file_offset line_table_ofs;
	int filepath_len;
	int file_index;
	int total_lines;
	int priority;
};

/* notice these are stored in t->lwsac_input_head which has input file scope */

struct lws_fts_lines {
	struct lws_fts_lines *lines_next;
	/*
	 * amount of line numbers needs to meet average count for best
	 * efficiency.
	 *
	 * Line numbers are stored in VLI format since if we don't, around half
	 * the total lac allocation consists of struct lws_fts_lines...
	 * size chosen to maintain 8-byte struct alignment
	 */
	uint8_t vli[119];
	char count;
};

/* this represents the instances of a symbol inside a given filepath */

struct lws_fts_instance_file {
	/* linked-list of tifs generated for current file */
	struct lws_fts_instance_file *inst_file_next;
	struct lws_fts_entry *owner;
	struct lws_fts_lines *lines_list, *lines_tail;
	uint32_t file_index;
	uint32_t total;

	/*
	 * optimization for the common case there's only 1 - ~3 matches, so we
	 * don't have to allocate any lws_fts_lines struct
	 *
	 * Using 8 bytes total for this maintains 8-byte struct alignment...
	 */

	uint8_t vli[7];
	char count;
};

/*
 * this is the main trie in-memory allocation object
 */

struct lws_fts_entry {
	struct lws_fts_entry *parent;

	struct lws_fts_entry *child_list;
	struct lws_fts_entry *sibling;

	/*
	 * care... this points to content in t->lwsac_input_head, it goes
	 * out of scope when the input file being indexed completes
	 */
	struct lws_fts_instance_file *inst_file_list;

	jg2_file_offset ofs_last_inst_file;

	char *suffix; /* suffix string or NULL if one char (in .c) */
	jg2_file_offset ofs;
	uint32_t child_count;
	uint32_t instance_count;
	uint32_t agg_inst_count;
	uint32_t agg_child_count;
	uint32_t suffix_len;
	unsigned char c;
};

/* there's only one of these per trie file */

struct lws_fts {
	struct lwsac *lwsac_head;
	struct lwsac *lwsac_input_head;
	struct lws_fts_entry *root;
	struct lws_fts_filepath *filepath_list;
	struct lws_fts_filepath *fp;

	struct lws_fts_entry *parser;
	struct lws_fts_entry *root_lookup[256];

	/*
	 * head of linked-list of tifs generated for current file
	 * care... this points to content in t->lwsac_input_head
	 */
	struct lws_fts_instance_file *tif_list;

	jg2_file_offset c; /* length of output file so far */

	uint64_t agg_trie_creation_us;
	uint64_t agg_raw_input;
	uint64_t worst_lwsac_input_size;
	int last_file_index;
	int chars_in_line;
	jg2_file_offset last_block_len_ofs;
	int line_number;
	int lines_in_unsealed_linetable;
	int next_file_index;
	int count_entries;

	int fd;
	unsigned int agg_pos;
	unsigned int str_match_pos;

	unsigned char aggregate;
	unsigned char agg[128];
};

/* since the kernel case allocates >300MB, no point keeping this too low */

#define TRIE_LWSAC_BLOCK_SIZE (1024 * 1024)

#define spill(margin, force) \
	if (bp && ((uint32_t)bp >= (sizeof(buf) - (size_t)(margin)) || (force))) { \
		if ((int)write(t->fd, buf, (size_t)bp) != bp) { \
			lwsl_err("%s: write %d failed (%d)\n", __func__, \
				 bp, errno); \
			return 1; \
		} \
		t->c += (unsigned int)bp; \
		bp = 0; \
	}

static int
g32(unsigned char *b, uint32_t d)
{
	*b++ = (uint8_t)((d >> 24) & 0xff);
	*b++ = (uint8_t)((d >> 16) & 0xff);
	*b++ = (uint8_t)((d >> 8) & 0xff);
	*b = (uint8_t)(d & 0xff);

	return 4;
}

static int
g16(unsigned char *b, int d)
{
	*b++ = (uint8_t)((d >> 8) & 0xff);
	*b = (uint8_t)(d & 0xff);

	return 2;
}

static int
wq32(unsigned char *b, uint32_t d)
{
	unsigned char *ob = b;

	if (d > (1 << 28) - 1)
		*b++ = (uint8_t)(((d >> 28) | 0x80) & 0xff);

	if (d > (1 << 21) - 1)
		*b++ = (uint8_t)(((d >> 21) | 0x80) & 0xff);

	if (d > (1 << 14) - 1)
		*b++ = (uint8_t)(((d >> 14) | 0x80) & 0xff);

	if (d > (1 << 7) - 1)
		*b++ = (uint8_t)(((d >> 7) | 0x80) & 0xff);

	*b++ = (uint8_t)(d & 0x7f);

	return lws_ptr_diff(b, ob);
}


/* read a VLI, return the number of bytes used */

int
rq32(unsigned char *b, uint32_t *d)
{
	unsigned char *ob = b;
	uint32_t t = 0;

	t = *b & 0x7f;
	if (*(b++) & 0x80) {
		t = (t << 7) | (*b & 0x7f);
		if (*(b++) & 0x80) {
			t = (t << 7) | (*b & 0x7f);
			if (*(b++) & 0x80) {
				t = (t << 7) | (*b & 0x7f);
				if (*(b++) & 0x80) {
					t = (t << 7) | (*b & 0x7f);
					b++;
				}
			}
		}
	}

	*d = t;

	return (int)(b - ob);
}

struct lws_fts *
lws_fts_create(int fd)
{
	struct lws_fts *t;
	struct lwsac *lwsac_head = NULL;
	unsigned char buf[TRIE_FILE_HDR_SIZE];

	t = lwsac_use(&lwsac_head, sizeof(*t), TRIE_LWSAC_BLOCK_SIZE);
	if (!t)
		return NULL;

	memset(t, 0, sizeof(*t));

	t->fd = fd;
	t->lwsac_head = lwsac_head;
	t->root = lwsac_use(&lwsac_head, sizeof(*t->root),
			    TRIE_LWSAC_BLOCK_SIZE);
	if (!t->root)
		goto unwind;

	memset(t->root, 0, sizeof(*t->root));
	t->parser = t->root;
	t->last_file_index = -1;
	t->line_number = 1;
	t->filepath_list = NULL;

	memset(t->root_lookup, 0, sizeof(*t->root_lookup));

	/* write the header */

	buf[0] = 0xca;
	buf[1] = 0x7a;
	buf[2] = 0x5f;
	buf[3] = 0x75;

	/* (these are filled in with correct data at the end) */

	/* file offset to root trie entry */
	g32(&buf[4], 0);
	/* file length when it was created */
	g32(&buf[8], 0);
	/* fileoffset to the filepath table */
	g32(&buf[0xc], 0);
	/* count of filepaths */
	g32(&buf[0x10], 0);

	if (write(t->fd, buf, TRIE_FILE_HDR_SIZE) != TRIE_FILE_HDR_SIZE) {
		lwsl_err("%s: trie header write failed\n", __func__);
		goto unwind;
	}

	t->c = TRIE_FILE_HDR_SIZE;

	return t;

unwind:
	lwsac_free(&lwsac_head);

	return NULL;
}

void
lws_fts_destroy(struct lws_fts **trie)
{
	struct lwsac *lwsac_head = (*trie)->lwsac_head;
	lwsac_free(&(*trie)->lwsac_input_head);
	lwsac_free(&lwsac_head);
	*trie = NULL;
}

int
lws_fts_file_index(struct lws_fts *t, const char *filepath, int filepath_len,
		    int priority)
{
	struct lws_fts_filepath *fp = t->filepath_list;
#if 0
	while (fp) {
		if (fp->filepath_len == filepath_len &&
		    !strcmp(fp->filepath, filepath))
			return fp->file_index;

		fp = fp->next;
	}
#endif
	fp = lwsac_use(&t->lwsac_head, sizeof(*fp), TRIE_LWSAC_BLOCK_SIZE);
	if (!fp)
		return -1;

	fp->next = t->filepath_list;
	t->filepath_list = fp;
	strncpy(fp->filepath, filepath, sizeof(fp->filepath) - 1);
	fp->filepath[sizeof(fp->filepath) - 1] = '\0';
	fp->filepath_len = filepath_len;
	fp->file_index = t->next_file_index++;
	fp->line_table_ofs = t->c;
	fp->priority = priority;
	fp->total_lines = 0;
	t->fp = fp;

	return fp->file_index;
}

static struct lws_fts_entry *
lws_fts_entry_child_add(struct lws_fts *t, unsigned char c,
			struct lws_fts_entry *parent)
{
	struct lws_fts_entry *e, **pe;

	e = lwsac_use(&t->lwsac_head, sizeof(*e), TRIE_LWSAC_BLOCK_SIZE);
	if (!e)
		return NULL;

	memset(e, 0, sizeof(*e));

	e->c = c;
	parent->child_count++;
	e->parent = parent;
	t->count_entries++;

	/* keep the parent child list in ascending sort order for c */

	pe = &parent->child_list;
	while (*pe) {
		assert((*pe)->parent == parent);
		if ((*pe)->c > c) {
			/* add it before */
			e->sibling = *pe;
			*pe = e;
			break;
		}
		pe = &(*pe)->sibling;
	}

	if (!*pe) {
		/* add it at the end */
		e->sibling = NULL;
		*pe = e;
	}

	return e;
}

static int
finalize_per_input(struct lws_fts *t)
{
	struct lws_fts_instance_file *tif;
	unsigned char buf[8192];
	uint64_t lwsac_input_size;
	jg2_file_offset temp;
	int bp = 0;

	bp += g16(&buf[bp], 0);
	bp += g16(&buf[bp], 0);
	bp += g32(&buf[bp], 0);
	if ((int)write(t->fd, buf, (size_t)bp) != bp)
		return 1;
	t->c += (unsigned int)bp;
	bp = 0;

	/*
	 * Write the generated file index + instances (if any)
	 *
	 * Notice the next same-parent file instance fileoffset list is
	 * backwards, so it does not require seeks to fill in.  The first
	 * entry has 0 but the second entry points to the first entry (whose
	 * fileoffset is known).
	 *
	 * After all the file instance structs are finalized,
	 * .ofs_last_inst_file contains the fileoffset of that child's tif
	 * list head in the file.
	 *
	 * The file instances are written to disk in the order that the files
	 * were indexed, along with their prev pointers inline.
	 */

	tif = t->tif_list;
	while (tif) {
		struct lws_fts_lines *i;

		spill((3 * MAX_VLI) + tif->count, 0);

		temp = tif->owner->ofs_last_inst_file;
		if (tif->total)
			tif->owner->ofs_last_inst_file = t->c + (unsigned int)bp;

		assert(!temp || (temp > TRIE_FILE_HDR_SIZE && temp < t->c));

		/* fileoffset of prev instance file for this entry, or 0 */
		bp += wq32(&buf[bp], temp);
		bp += wq32(&buf[bp], tif->file_index);
		bp += wq32(&buf[bp], tif->total);

		/* remove any pointers into this disposable lac footprint */
		tif->owner->inst_file_list = NULL;

		memcpy(&buf[bp], &tif->vli, (size_t)tif->count);
		bp += tif->count;

		i = tif->lines_list;
		while (i) {
			spill(i->count, 0);
			memcpy(&buf[bp], &i->vli, (size_t)i->count);
			bp += i->count;

			i = i->lines_next;
		}

		tif = tif->inst_file_next;
	}

	spill(0, 1);

	assert(lseek(t->fd, 0, SEEK_END) == (off_t)t->c);

	if (t->lwsac_input_head) {
		lwsac_input_size = lwsac_total_alloc(t->lwsac_input_head);
		if (lwsac_input_size > t->worst_lwsac_input_size)
			t->worst_lwsac_input_size = lwsac_input_size;
	}

	/*
	 * those per-file allocations are all on a separate lac so we can
	 * free it cleanly afterwards
	 */
	lwsac_free(&t->lwsac_input_head);

	/* and lose the pointer into the deallocated lac */
	t->tif_list = NULL;

	return 0;
}

/*
 * 0 = punctuation, whitespace, brackets etc
 * 1 = character inside symbol set
 * 2 = upper-case character inside symbol set
 */

static char classify[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0,
	0, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 0, 0, 0, 0, 1, //1,
	0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
};

#if 0
static const char *
name_entry(struct lws_fts_entry *e1, char *s, int len)
{
	struct lws_fts_entry *e2;
	int n = len;

	s[--n] = '\0';

	e2 = e1;
	while (e2) {
		if (e2->suffix) {
			if ((int)e2->suffix_len < n) {
				n -= e2->suffix_len;
				memcpy(&s[n], e2->suffix, e2->suffix_len);
			}
		} else {
			n--;
			s[n] = e2->c;
		}

		e2 = e2->parent;
	}

	return &s[n + 1];
}
#endif

/*
 * as we parse the input, we create a line length table for the file index.
 * Only the file header has been written before we start doing this.
 */

int
lws_fts_fill(struct lws_fts *t, uint32_t file_index, const char *buf,
	     size_t len)
{
	unsigned long long tf = (unsigned long long)lws_now_usecs();
	unsigned char c, linetable[256], vlibuf[8];
	struct lws_fts_entry *e, *e1, *dcl;
	struct lws_fts_instance_file *tif;
	int bp = 0, sline, chars, m;
	char *osuff, skipline = 0;
	struct lws_fts_lines *tl;
	unsigned int olen, n;
	off_t lbh;

	if ((int)file_index != t->last_file_index) {
		if (t->last_file_index >= 0)
			finalize_per_input(t);
		t->last_file_index = (int)file_index;
		t->line_number = 1;
		t->chars_in_line = 0;
		t->lines_in_unsealed_linetable = 0;
	}

	t->agg_raw_input += len;

resume:

	chars = 0;
	lbh = (off_t)t->c;
	sline = t->line_number;
	bp += g16(&linetable[bp], 0);
	bp += g16(&linetable[bp], 0);
	bp += g32(&linetable[bp], 0);

	while (len) {
		char go_around = 0;

		if (t->lines_in_unsealed_linetable >= LWS_FTS_LINES_PER_CHUNK)
			break;

		len--;

		c = (unsigned char)*buf++;
		t->chars_in_line++;
		if (c == '\n') {
			skipline = 0;
			t->filepath_list->total_lines++;
			t->lines_in_unsealed_linetable++;
			t->line_number++;

			bp += wq32(&linetable[bp], (uint32_t)t->chars_in_line);
			if ((unsigned int)bp > sizeof(linetable) - 6) {
				if ((int)write(t->fd, linetable, (unsigned int)bp) != bp) {
					lwsl_err("%s: linetable write failed\n",
							__func__);
					return 1;
				}
				t->c += (unsigned int)bp;
				bp = 0;
				// assert(lseek(t->fd, 0, SEEK_END) == t->c);
			}

			chars += t->chars_in_line;
			t->chars_in_line = 0;

			/*
			 * Detect overlength lines and skip them (eg, BASE64
			 * in css etc)
			 */

			if (len > 200) {
				n = 0;
				m = 0;
				while (n < 200 && m < 80 && buf[n] != '\n') {
				       if (buf[n] == ' ' || buf[n] == '\t')
					       m = 0;
					n++;
					m++;
				}

				/* 80 lines no whitespace, or >=200-char line */

				if (m == 80 || n == 200)
					skipline = 1;
			}

			goto seal;
		}
		if (skipline)
			continue;

		m = classify[(int)c];
		if (!m)
			goto seal;
		if (m == 2)
			c = (unsigned char)((char)c + 'a' - 'A');

		if (t->aggregate) {

			/*
			 * We created a trie entry for an earlier char in this
			 * symbol already.  So we know at the moment, any
			 * further chars in the symbol are the only children.
			 *
			 * Aggregate them and add them as a string suffix to
			 * the trie symbol at the end (when we know how much to
			 * allocate).
			 */

			if (t->agg_pos < sizeof(t->agg) - 1)
				/* symbol is not too long to stash */
				t->agg[t->agg_pos++] = c;

			continue;
		}

		if (t->str_match_pos) {
			go_around = 1;
			goto seal;
		}

		/* zeroth-iteration child matching */

		if (t->parser == t->root) {
			e = t->root_lookup[(int)c];
			if (e) {
				t->parser = e;
				continue;
			}
		} else {

			/* look for the char amongst the children */

			e = t->parser->child_list;
			while (e) {

				/* since they're alpha ordered... */
				if (e->c > c) {
					e = NULL;
					break;
				}
				if (e->c == c) {
					t->parser = e;

					if (e->suffix)
						t->str_match_pos = 1;

					break;
				}

				e = e->sibling;
			}

			if (e)
				continue;
		}

		/*
		 * we are blazing a new trail, add a new child representing
		 * the whole suffix that couldn't be matched until now.
		 */

		e = lws_fts_entry_child_add(t, c, t->parser);
		if (!e) {
			lwsl_err("%s: lws_fts_entry_child_add failed\n",
					__func__);
			return 1;
		}

		/* if it's the root node, keep the root_lookup table in sync */

		if (t->parser == t->root)
			t->root_lookup[(int)c] = e;

		/* follow the new path */
		t->parser = e;

		{
			struct lws_fts_entry **pe = &e->child_list;
			while (*pe) {
				assert((*pe)->parent == e);

				pe = &(*pe)->sibling;
			}
		}

		/*
		 * If there are any more symbol characters coming, just
		 * create a suffix string on t->parser instead of what must
		 * currently be single-child nodes, since we just created e
		 * as a child with a single character due to no existing match
		 * on that single character... so if no match on 'h' with this
		 * guy's parent, we created e that matches on the single char
		 * 'h'.  If the symbol continues ... 'a' 'p' 'p' 'y', then
		 * instead of creating singleton child nodes under e,
		 * modify e to match on the whole string suffix "happy".
		 *
		 * If later "hoppy" appears, we will remove the suffix on e,
		 * so it reverts to a char match for 'h', add singleton children
		 * for 'a' and 'o', and attach a "ppy" suffix child to each of
		 * those.
		 *
		 * We want to do this so we don't have to allocate trie entries
		 * for every char in the string to save memory and consequently
		 * time.
		 *
		 * Don't try this optimization if the parent is the root node...
		 * it's not compatible with it's root_lookup table and it's
		 * highly likely children off the root entry are going to have
		 * to be fragmented.
		 */

		if (e->parent != t->root) {
			t->aggregate = 1;
			t->agg_pos = 0;
		}

		continue;

seal:
		if (t->str_match_pos) {

			/*
			 * We're partway through matching an elaborated string
			 * on a child, not just a character.  String matches
			 * only exist when we met a child entry that only had
			 * one path until now... so we had an 'h', and the
			 * only child had a string "hello".
			 *
			 * We are following the right path and will not need
			 * to back up, but we may find as we go we have the
			 * first instance of a second child path, eg, "help".
			 *
			 * When we get to the 'p', we have to split what was
			 * the only string option "hello" into "hel" and then
			 * two child entries, for "lo" and 'p'.
			 */

			if (c == t->parser->suffix[t->str_match_pos++]) {
				if (t->str_match_pos < t->parser->suffix_len)
					continue;

				/*
				 * We simply matched everything, continue
				 * parsing normally from this trie entry.
				 */

				t->str_match_pos = 0;
				continue;
			}

			/*
			 * So... we hit a mismatch somewhere... it means we
			 * have to split this string entry.
			 *
			 * We know the first char actually matched in order to
			 * start down this road.  So for the current trie entry,
			 * we need to truncate his suffix at the char before
			 * this mismatched one, where we diverged (if the
			 * second char, simply remove the suffix string from the
			 * current trie entry to turn it back to a 1-char match)
			 *
			 * The original entry, which becomes the lhs post-split,
			 * is t->parser.
			 */

			olen = t->parser->suffix_len;
			osuff = t->parser->suffix;

			if (t->str_match_pos == 2)
				t->parser->suffix = NULL;
			else
				t->parser->suffix_len = t->str_match_pos - 1;

			/*
			 * Then we need to create a new child trie entry that
			 * represents the remainder of the original string
			 * path that we didn't match.  For the "hello" /
			 * "help" case, this guy will have "lo".
			 *
			 * Any instances or children (not siblings...) that were
			 * attached to the original trie entry must be detached
			 * first and then migrate to this new guy that completes
			 * the original string.
			 */

			dcl = t->parser->child_list;
			m = (int)t->parser->child_count;

			t->parser->child_list = NULL;
			t->parser->child_count = 0;

			e = lws_fts_entry_child_add(t, (unsigned char)
					osuff[t->str_match_pos - 1], t->parser);
			if (!e) {
				lwsl_err("%s: lws_fts_entry_child_add fail1\n",
						__func__);
				return 1;
			}

			e->child_list = dcl;
			e->child_count = (uint32_t)m;
			/*
			 * any children we took over must point to us as the
			 * parent now they appear on our child list
			 */
			e1 = e->child_list;
			while (e1) {
				e1->parent = e;
				e1 = e1->sibling;
			}

			/*
			 * We detached any children, gave them to the new guy
			 * and replaced them with just our new guy
			 */
			t->parser->child_count = 1;
			t->parser->child_list = e;

			/*
			 * any instances that belonged to the original entry we
			 * are splitting now must be reassigned to the end
			 * part
			 */

			e->inst_file_list = t->parser->inst_file_list;
			if (e->inst_file_list)
				e->inst_file_list->owner = e;
			t->parser->inst_file_list = NULL;
			e->instance_count = t->parser->instance_count;
			t->parser->instance_count = 0;

			e->ofs_last_inst_file = t->parser->ofs_last_inst_file;
			t->parser->ofs_last_inst_file = 0;

			if (t->str_match_pos != olen) {
				/* we diverged partway */
				e->suffix = &osuff[t->str_match_pos - 1];
				e->suffix_len = olen - (t->str_match_pos - 1);
			}

			/*
			 * if the current char is a terminal, skip creating a
			 * new way forward.
			 */

			if (classify[(int)c]) {

				/*
				 * Lastly we need to create a new child trie
				 * entry that represents the new way forward
				 * from the point that we diverged.  For the
				 * "hello" / "help" case, this guy will start
				 * as a child of "hel" with the single
				 * character match 'p'.
				 *
				 * Since he becomes the current parser context,
				 * more symbol characters may be coming to make
				 * him into, eg, "helping", in which case he
				 * will acquire a suffix eventually of "ping"
				 * via the aggregation stuff
				 */

				e = lws_fts_entry_child_add(t, c, t->parser);
				if (!e) {
					lwsl_err("%s: child_add fail2\n",
						 __func__);
					return 1;
				}
			}

			/* go on following this path */
			t->parser = e;

			t->aggregate = 1;
			t->agg_pos = 0;

			t->str_match_pos = 0;

			if (go_around)
				continue;

			/* this is intended to be a seal */
		}


		/* end of token */

		if (t->aggregate && t->agg_pos) {

			/* if nothing in agg[]: leave as single char match */

			/* otherwise copy out the symbol aggregation */
			t->parser->suffix = lwsac_use(&t->lwsac_head,
						    t->agg_pos + 1,
						    TRIE_LWSAC_BLOCK_SIZE);
			if (!t->parser->suffix) {
				lwsl_err("%s: lac for suffix failed\n",
						__func__);
				return 1;
			}

			/* add the first char at the beginning */
			*t->parser->suffix = (char)t->parser->c;
			/* and then add the agg buffer stuff */
			memcpy(t->parser->suffix + 1, t->agg, t->agg_pos);
			t->parser->suffix_len = t->agg_pos + 1;
		}
		t->aggregate = 0;

		if (t->parser == t->root) /* multiple terminal chars */
			continue;

		if (!t->parser->inst_file_list ||
		    t->parser->inst_file_list->file_index != file_index) {
			tif = lwsac_use(&t->lwsac_input_head, sizeof(*tif),
				      TRIE_LWSAC_BLOCK_SIZE);
			if (!tif) {
				lwsl_err("%s: lac for tif failed\n",
						__func__);
				return 1;
			}

			tif->file_index = file_index;
			tif->owner = t->parser;
			tif->lines_list = NULL;
			tif->lines_tail = NULL;
			tif->total = 0;
			tif->count = 0;
			tif->inst_file_next = t->tif_list;
			t->tif_list = tif;

			t->parser->inst_file_list = tif;
		}

		/*
		 * A naive allocation strategy for this leads to 50% of the
		 * total inmem lac allocation being for line numbers...
		 *
		 * It's mainly solved by only holding the instance and line
		 * number tables for the duration of a file being input, as soon
		 * as one input file is finished it is written to disk.
		 *
		 * For the common case of 1 - ~3 matches the line number are
		 * stored in a small VLI array inside the filepath inst.  If the
		 * next one won't fit, it allocates a line number struct with
		 * more vli space and continues chaining those if needed.
		 */

		n = (unsigned int)wq32(vlibuf, (uint32_t)t->line_number);
		tif = t->parser->inst_file_list;

		if (!tif->lines_list) {
			/* we are still trying to use the file inst vli */
			if (LWS_ARRAY_SIZE(tif->vli) - (size_t)tif->count >= n) {
				tif->count = (char)((char)tif->count + (char)wq32(tif->vli + tif->count,
						   (uint32_t)t->line_number));
				goto after;
			}
			/* we are going to have to allocate */
		}

		/* can we add to an existing line numbers struct? */
		if (tif->lines_tail &&
		    LWS_ARRAY_SIZE(tif->lines_tail->vli) -
				(unsigned char)tif->lines_tail->count >= n) {
			tif->lines_tail->count = (char)((char)tif->lines_tail->count + (char)wq32(tif->lines_tail->vli +
						       tif->lines_tail->count,
						       (uint32_t)t->line_number));
			goto after;
		}

		/* either no existing line numbers struct at tail, or full */

		/* have to create a(nother) line numbers struct */
		tl = lwsac_use(&t->lwsac_input_head, sizeof(*tl),
			     TRIE_LWSAC_BLOCK_SIZE);
		if (!tl) {
			lwsl_err("%s: lac for tl failed\n", __func__);
			return 1;
		}
		tl->lines_next = NULL;
		if (tif->lines_tail)
			tif->lines_tail->lines_next = tl;

		tif->lines_tail = tl;
		if (!tif->lines_list)
			tif->lines_list = tl;

		tl->count = (char)wq32(tl->vli, (uint32_t)t->line_number);
after:
		tif->total++;
#if 0
		{
			char s[128];
			const char *ne = name_entry(t->parser, s, sizeof(s));

			if (!strcmp(ne, "describ")) {
				lwsl_err("     %s %d\n", ne, t->str_match_pos);
				write(1, buf - 10, 20);
			}
		}
#endif
		t->parser->instance_count++;
		t->parser = t->root;
		t->str_match_pos = 0;
	}

	/* seal off the line length table block */

	if (bp) {
		if ((int)write(t->fd, linetable, (size_t)bp) != bp)
			return 1;
		t->c += (unsigned int)bp;
		bp = 0;
	}

	if (lseek(t->fd, lbh, SEEK_SET) < 0) {
		lwsl_err("%s: seek to 0x%llx failed\n", __func__,
				(unsigned long long)lbh);
		return 1;
	}

	g16(linetable, (uint16_t)(t->c - (jg2_file_offset)lbh));
	g16(linetable + 2, (uint16_t)(t->line_number - sline));
	g32(linetable + 4, (uint32_t)chars);
	if ((int)write(t->fd, linetable, 8) != 8) {
		lwsl_err("%s: write linetable header failed\n", __func__);
		return 1;
	}

	assert(lseek(t->fd, 0, SEEK_END) == (off_t)t->c);

	if (lseek(t->fd, (off_t)t->c, SEEK_SET) < 0) {
		lwsl_err("%s: end seek failed\n", __func__);
		return 1;
	}

	bp = 0;

	if (len) {
		t->lines_in_unsealed_linetable = 0;
		goto resume;
	}

	/* dump the collected per-input instance and line data, and free it */

	t->agg_trie_creation_us += (uint64_t)((uint64_t)lws_now_usecs() - tf);

	return 0;
}

/* refer to ./README.md */

int
lws_fts_serialize(struct lws_fts *t)
{
	struct lws_fts_filepath *fp = t->filepath_list, *ofp;
	unsigned long long tf = (unsigned long long)lws_now_usecs();
	struct lws_fts_entry *e, *e1, *s[256];
	unsigned char buf[8192], stasis;
	int n, bp, sp = 0, do_parent;

	(void)tf;
	finalize_per_input(t);

	/*
	 * Compute aggregated instance counts (parents should know the total
	 * number of instances below each child path)
	 *
	 *
	 * If we have
	 *
	 * (root) -> (c1) -> (c2)
	 *        -> (c3)
	 *
	 * we need to visit the nodes in the order
	 *
	 * c2, c1, c3, root
	 */

	sp = 0;
	s[0] = t->root;
	do_parent = 0;
	while (sp >= 0) {
		int n;

		/* aggregate in every antecedent */

		for (n = 0; n <= sp; n++) {
			s[n]->agg_inst_count += s[sp]->instance_count;
			s[n]->agg_child_count += s[sp]->child_count;
		}

		/* handle any children before the parent */

		if (s[sp]->child_list) {
			if (sp + 1 == LWS_ARRAY_SIZE(s)) {
				lwsl_err("Stack too deep\n");

				goto bail;
			}

			s[sp + 1] = s[sp]->child_list;
			sp++;
			continue;
		}

		do {
			if (s[sp]->sibling) {
				s[sp] = s[sp]->sibling;
				break;
			} else
				sp--;
		} while (sp >= 0);
	}

	/* dump the filepaths and set prev */

	fp = t->filepath_list;
	ofp = NULL;
	bp = 0;
	while (fp) {

		fp->ofs = t->c + (unsigned int)bp;
		n = (int)strlen(fp->filepath);
		spill(15 + n, 0);

		bp += wq32(&buf[bp], fp->line_table_ofs);
		bp += wq32(&buf[bp], (uint32_t)fp->total_lines);
		bp += wq32(&buf[bp], (uint32_t)n);
		memcpy(&buf[bp], fp->filepath, (unsigned int)n);
		bp += n;

		fp->prev = ofp;
		ofp = fp;
		fp = fp->next;
	}

	spill(0, 1);

	/* record the fileoffset of the filepath map and filepath count */

	if (lseek(t->fd, 0xc, SEEK_SET) < 0)
		goto bail_seek;

	g32(buf, t->c + (unsigned int)bp);
	g32(buf + 4, (uint32_t)t->next_file_index);
	if ((int)write(t->fd, buf, 8) != 8)
		goto bail;

	if (lseek(t->fd, (off_t)(t->c + (unsigned int)bp), SEEK_SET) < 0)
		goto bail_seek;

	/* dump the filepath map, starting from index 0, which is at the tail */

	fp = ofp;
	bp = 0;
	while (fp) {
		spill(5, 0);
		g32(buf + bp, fp->ofs);
		bp += 4;
		fp = fp->prev;
	}
	spill(0, 1);

	/*
	 * The trie entries in reverse order... because of the reversal, we have
	 * always written children first, and marked them with their file offset
	 * before we come to refer to them.
	 */

	bp = 0;
	sp = 0;
	s[0] = t->root;
	do_parent = 0;
	while (s[sp]) {

		/* handle any children before the parent */

		if (!do_parent && s[sp]->child_list) {

			if (sp + 1 == LWS_ARRAY_SIZE(s)) {
				lwsl_err("Stack too deep\n");

				goto bail;
			}

			s[sp + 1] = s[sp]->child_list;
			sp++;
			continue;
		}

		/* leaf nodes with no children */

		e = s[sp];
		e->ofs = t->c + (unsigned int)bp;

		/* write the trie entry header */

		spill((3 * MAX_VLI), 0);

		bp += wq32(&buf[bp], e->ofs_last_inst_file);
		bp += wq32(&buf[bp], e->child_count);
		bp += wq32(&buf[bp], e->instance_count);
		bp += wq32(&buf[bp], e->agg_inst_count);

		/* sort the children in order of highest aggregate hits first */

		do {
			struct lws_fts_entry **pe, *te1, *te2;

			stasis = 1;

			/* bubble sort keeps going until nothing changed */

			pe = &e->child_list;
			while (*pe) {

				te1 = *pe;
				te2 = te1->sibling;

				if (te2 && te1->agg_inst_count <
					   te2->agg_inst_count) {
					stasis = 0;

					*pe = te2;
					te1->sibling = te2->sibling;
					te2->sibling = te1;
				}

				pe = &(*pe)->sibling;
			}

		} while (!stasis);

		/* write the children */

		e1 = e->child_list;
		while (e1) {
			spill((5 * MAX_VLI) + e1->suffix_len + 1, 0);

			bp += wq32(&buf[bp], e1->ofs);
			bp += wq32(&buf[bp], e1->instance_count);
			bp += wq32(&buf[bp], e1->agg_inst_count);
			bp += wq32(&buf[bp], e1->agg_child_count);

			if (e1->suffix) { /* string  */
				bp += wq32(&buf[bp], e1->suffix_len);
				memmove(&buf[bp], e1->suffix, e1->suffix_len);
				bp += (int)e1->suffix_len;
			} else { /* char */
				bp += wq32(&buf[bp], 1);
				buf[bp++] = e1->c;
			}
#if 0
			if (e1->suffix && e1->suffix_len == 3 &&
			    !memcmp(e1->suffix, "cri", 3)) {
				struct lws_fts_entry *e2;

				e2 = e1;
				while (e2){
					if (e2->suffix)
						lwsl_notice("%s\n", e2->suffix);
					else
						lwsl_notice("%c\n", e2->c);

					e2 = e2->parent;
				}

				lwsl_err("*** %c CRI inst %d ch %d\n", e1->parent->c,
						e1->instance_count, e1->child_count);
			}
#endif
			e1 = e1->sibling;
		}

		/* if there are siblings, do those next */

		if (do_parent) {
			do_parent = 0;
			sp--;
		}

		if (s[sp]->sibling)
			s[sp] = s[sp]->sibling;
		else {
			/* if there are no siblings, do the parent */
			do_parent = 1;
			s[sp] = s[sp]->parent;
		}
	}

	spill(0, 1);

	assert(lseek(t->fd, 0, SEEK_END) == (off_t)t->c);

	/* drop the correct root trie offset + file length into the header */

	if (lseek(t->fd, 4, SEEK_SET) < 0) {
		lwsl_err("%s: unable to seek\n", __func__);

		goto bail;
	}

	g32(buf, t->root->ofs);
	g32(buf + 4, t->c);
	if (write(t->fd, buf, 0x8) != 0x8)
		goto bail;

	lwsl_notice("%s: index %d files (%uMiB) cpu time %dms, "
		    "alloc: %dKiB + %dKiB, "
		    "serialize: %dms, file: %dKiB\n", __func__,
		    t->next_file_index,
		    (int)(t->agg_raw_input / (1024 * 1024)),
		    (int)(t->agg_trie_creation_us / 1000),
		    (int)(lwsac_total_alloc(t->lwsac_head) / 1024),
		    (int)(t->worst_lwsac_input_size / 1024),
		    (int)(((uint64_t)lws_now_usecs() - tf) / 1000),
		    (int)(t->c / 1024));

	return 0;

bail_seek:
	lwsl_err("%s: problem seekings\n", __func__);

bail:
	return 1;
}


