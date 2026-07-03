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

#if !defined(NO_GNU_SOURCE_THIS_TIME)
#define NO_GNU_SOURCE_THIS_TIME
#endif
#if !defined(_DARWIN_C_SOURCE)
#define _DARWIN_C_SOURCE
#endif

#include "private-lib-core.h"
#include <string.h>
#include <stdio.h>

#include <sys/stat.h>
#if defined(WIN32)
#include <direct.h>
#define read _read
#define open _open
#define close _close
#define write _write
#define mkdir(x,y) _mkdir(x)
#define rmdir _rmdir
#define unlink _unlink
#define HAVE_STRUCT_TIMESPEC
#if defined(pid_t)
#undef pid_t
#endif
#endif /* win32 */

#define COMBO_SIZEOF 512

#if !defined(LWS_PLAT_FREERTOS)

#if defined(WIN32)
#include "../../win32port/dirent/dirent-win32.h"
#else
#include <dirent.h>
#endif

static int filter(const struct dirent *ent)
{
	if (!strcmp(ent->d_name, ".") || !strcmp(ent->d_name, ".."))
		return 0;

	return 1;
}


#if !defined(WIN32)
static char csep = '/';
#else
static char csep = '\\';
#endif

static void
lws_dir_via_stat(char *combo, size_t l, const char *path, struct lws_dir_entry *lde)
{
        struct stat s;

        lws_strncpy(combo + l, path, COMBO_SIZEOF - l);

        lde->type = LDOT_UNKNOWN;

        if (!stat(combo, &s)) {
		switch (s.st_mode & S_IFMT) {
		case S_IFBLK:
			lde->type = LDOT_BLOCK;
			break;
		case S_IFCHR:
			lde->type = LDOT_CHAR;
			break;
		case S_IFDIR:
			lde->type = LDOT_DIR;
			break;
		case S_IFIFO:
			lde->type = LDOT_FIFO;
			break;
#if !defined(WIN32)
		case S_IFLNK:
			lde->type = LDOT_LINK;
			break;
#endif
		case S_IFREG:
			lde->type = LDOT_FILE;
			break;
		default:
			break;
		}
        }
}

int
lws_dir(const char *dirpath, void *user, lws_dir_callback_function cb)
{
	struct lws_dir_entry lde;
	struct dirent **namelist;
	int n, i, ret = 1;
	char combo[COMBO_SIZEOF];
	size_t l;

	l = (size_t)(ssize_t)lws_snprintf(combo, COMBO_SIZEOF - 2, "%s", dirpath);
	combo[l++] = csep;
	combo[l] = '\0';

	n = scandir((char *)dirpath, &namelist, filter, alphasort);
	if (n < 0) {
		lwsl_err("Scandir on '%s' failed, errno %d\n", dirpath, LWS_ERRNO);
		return 1;
	}

	for (i = 0; i < n; i++) {
#if !defined(__sun)
		unsigned int type = namelist[i]->d_type;
#endif
		if (strchr(namelist[i]->d_name, '~'))
			goto skip;
		lde.name = namelist[i]->d_name;

		/*
		 * some filesystems don't report this (ZFS) and tell that
		 * files are LDOT_UNKNOWN
		 */

#if defined(__sun)
		lws_dir_via_stat(combo, l, namelist[i]->d_name, &lde);
#else
		/*
		 * XFS on Linux doesn't fill in d_type at all, always zero.
		 */

		if (DT_BLK != DT_UNKNOWN && type == DT_BLK)
			lde.type = LDOT_BLOCK;
		else if (DT_CHR != DT_UNKNOWN && type == DT_CHR)
			lde.type = LDOT_CHAR;
		else if (DT_DIR != DT_UNKNOWN && type == DT_DIR)
			lde.type = LDOT_DIR;
		else if (DT_FIFO != DT_UNKNOWN && type == DT_FIFO)
			lde.type = LDOT_FIFO;
		else if (DT_LNK != DT_UNKNOWN && type == DT_LNK)
			lde.type = LDOT_LINK;
		else if (DT_REG != DT_UNKNOWN && type == DT_REG)
			lde.type = LDOT_FILE;
		else if (DT_SOCK != DT_UNKNOWN && type == DT_SOCK)
			lde.type = LDOTT_SOCKET;
		else {
			lde.type = LDOT_UNKNOWN;
			lws_dir_via_stat(combo, l, namelist[i]->d_name, &lde);
		}
#endif
		if (cb(dirpath, user, &lde)) {
			while (i < n)
				free(namelist[i++]);
			ret = 0; /* told to stop by cb */
			goto bail;
		}
skip:
		free(namelist[i]);
	}

bail:
	free(namelist);

	return ret;
}

/*
 * Check filename against one globby filter
 *
 * We can support things like "*.rpm"
 */

static int
lws_dir_glob_check(const char *nm, const char *filt)
{
	while (*nm) {
		if (*filt == '*') {
			if (!strcmp(nm, filt + 1))
				return 1;
		} else {
			if (*nm != *filt)
				return 0;
			filt++;
		}
		nm++;
	}

	return 0;
}

/*
 * We get passed a single filter string, like "*.txt" or "mydir/\*.rpm" or so.
 */

int
lws_dir_glob_cb(const char *dirpath, void *user, struct lws_dir_entry *lde)
{
	lws_dir_glob_t *filter = (lws_dir_glob_t*)user;
	char path[384];

	if (!strcmp(lde->name, ".") || !strcmp(lde->name, ".."))
		return 0;

	if (lde->type == LDOT_DIR)
		return 0;

	if (lws_dir_glob_check(lde->name, filter->filter)) {
		lws_snprintf(path, sizeof(path), "%s%c%s", dirpath, csep,
							   lde->name);
		filter->cb(filter->user, path);
	}

	return 0;
}

int
lws_dir_rm_rf_cb(const char *dirpath, void *user, struct lws_dir_entry *lde)
{
	char path[384];

	if (!strcmp(lde->name, ".") || !strcmp(lde->name, ".."))
		return 0;

	lws_snprintf(path, sizeof(path), "%s%c%s", dirpath, csep, lde->name);

	if (lde->type == LDOT_DIR) {
#if !defined(WIN32) && !defined(_WIN32) && !defined(__COVERITY__)
		char dummy[8];
		/*
		 * hm... eg, recursive dir symlinks can show up a LDOT_DIR
		 * here.  If it's a symlink, don't recurse into it.
		 *
		 * Notice we immediately discard dummy without looking in it.
		 * There is no way to get into trouble from its lack of NUL
		 * termination in dummy[].  We just wanted to know if it was
		 * a symlink at all.
		 *
		 * Hide this from Coverity since it flags any use of readlink()
		 * even if safe.
		 */
		if (readlink(path, dummy, sizeof(dummy)) < 0)
#endif
			lws_dir(path, NULL, lws_dir_rm_rf_cb);

		if (rmdir(path))
			lwsl_warn("%s: rmdir %s failed %d\n", __func__, path, errno);
	} else {
		if (unlink(path)) {
#if defined(WIN32)
			SetFileAttributesA(path, FILE_ATTRIBUTE_NORMAL);
			if (unlink(path))
#else
			if (rmdir(path))
#endif
			lwsl_warn("%s: unlink %s failed %d (type %d)\n",
					__func__, path, errno, lde->type);
		}
	}

	return 0;
}


#endif

#if defined(LWS_WITH_PLUGINS_API)

struct lws_plugins_args {
	struct lws_plugin	**pplugin;
	const char		*_class;
	const char		*filter;
	each_plugin_cb_t	each;
	void			*each_user;
};

static int
lws_plugins_dir_cb(const char *dirpath, void *user, struct lws_dir_entry *lde)
{
	struct lws_plugins_args *pa = (struct lws_plugins_args *)user;
	char path[256], base[64], *q = base;
	const lws_plugin_header_t *pl;
	const char *p;

	if (strlen(lde->name) < 7)
		return 0; /* keep going */

	/*
	 * The actual plugin names for protocol plugins look like
	 * "libprotocol_lws_ssh_base.so" and for event libs
	 * "libwebsockets-evlib_ev.so"... to recover the base name of
	 * "lws_ssh_base" and "evlib_ev" we strip from the left to after the
	 * first _ or -, and then truncate at the first .
	 */

	p = lde->name;
	while (*p && *p != '_' && *p != '-')
		p++;
	if (!*p)
		return 0;
	p++;
	while (*p && *p != '.' && lws_ptr_diff(q, base) < (int)sizeof(base) - 1)
		*q++ = *p++;
	*q = '\0';

	/* if he's given a filter, only match if base matches it */
	if (pa->filter && strcmp(base, pa->filter))
		return 0; /* keep going */

	lws_snprintf(path, sizeof(path) - 1, "%s/%s", dirpath, lde->name);

	pl = lws_plat_dlopen(pa->pplugin, path, base, pa->_class,
			     pa->each, pa->each_user);

	/*
	 * If we were looking for a specific plugin, finding it should make
	 * us stop looking (eg, to account for directory precedence of the
	 * same plugin).  If scanning for plugins in a dir, we always keep
	 * going.
	 */

	return pa->filter && pl;
}

int
lws_plugins_init(struct lws_plugin **pplugin, const char * const *d,
		 const char *_class, const char *filter,
		 each_plugin_cb_t each, void *each_user)
{
	struct lws_plugins_args pa;
	char *ld_env;
	int ret = 1;

	pa.pplugin = pplugin;
	pa._class = _class;
	pa.each = each;
	pa.each_user = each_user;
	pa.filter = filter;

	/*
	 * Check LD_LIBRARY_PATH override path first if present
	 */

	ld_env = getenv("LD_LIBRARY_PATH");
	if (ld_env) {
		char temp[128];
		struct lws_tokenize ts;

		memset(&ts, 0, sizeof(ts));
		ts.start = ld_env;
		ts.len = strlen(ld_env);
		ts.flags = LWS_TOKENIZE_F_SLASH_NONTERM |
			   LWS_TOKENIZE_F_DOT_NONTERM |
			   LWS_TOKENIZE_F_MINUS_NONTERM |
			   LWS_TOKENIZE_F_NO_INTEGERS |
			   LWS_TOKENIZE_F_NO_FLOATS;

		do {
			ts.e = (int8_t)lws_tokenize(&ts);
			if (ts.e != LWS_TOKZE_TOKEN)
				continue;

			lws_strnncpy(temp, ts.token,
				     ts.token_len,
				     sizeof(temp));

			lwsl_info("%s: trying %s\n", __func__, temp);
			if (!lws_dir(temp, &pa, lws_plugins_dir_cb))
				ret = 0;

		} while (ts.e > 0);
	}

	while (d && *d) {
		lwsl_info("%s: trying %s\n", __func__, *d);
		if (!lws_dir(*d, &pa, lws_plugins_dir_cb))
			ret = 0;

		d++;
	}

	return ret;
}

int
lws_plugins_destroy(struct lws_plugin **pplugin, each_plugin_cb_t each,
		    void *each_user)
{
	struct lws_plugin *p = *pplugin, *p1;

	while (p) {
		if (each)
			each(p, each_user);
		lws_plat_destroy_dl(p);
		p1 = p->list;
		p->list = NULL;
		lws_free(p);
		p = p1;
	}

	*pplugin = NULL;

	return 0;
}
#endif
