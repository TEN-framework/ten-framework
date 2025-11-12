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
#include "private-lib-core.h"

#include <pwd.h>
#include <grp.h>

#ifdef LWS_WITH_PLUGINS
#include <dlfcn.h>
#endif
#include <dirent.h>

int lws_plat_apply_FD_CLOEXEC(int n)
{
	if (n == -1)
		return 0;

	return fcntl(n, F_SETFD, FD_CLOEXEC);
}

int
lws_plat_write_file(const char *filename, void *buf, size_t len)
{
	ssize_t m;
	int fd;

	fd = lws_open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0600);

	if (fd == -1)
		return 1;

	m = write(fd, buf, len);
	close(fd);

	if (m < 0)
		return 1;

	return (size_t)m != len;
}

int
lws_plat_read_file(const char *filename, void *buf, size_t len)
{
	int fd = lws_open(filename, O_RDONLY);
	ssize_t n;

	if (fd == -1)
		return -1;

	n = read(fd, buf, len);
	close(fd);

	return (int)n;
}

lws_fop_fd_t
_lws_plat_file_open(const struct lws_plat_file_ops *fops, const char *filename,
		    const char *vpath, lws_fop_flags_t *flags)
{
	struct stat stat_buf;
	int ret = lws_open(filename, (*flags) & LWS_FOP_FLAGS_MASK, 0664);
	lws_fop_fd_t fop_fd;

	if (ret < 0)
		return NULL;

	if (fstat(ret, &stat_buf) < 0)
		goto bail;

	fop_fd = malloc(sizeof(*fop_fd));
	if (!fop_fd)
		goto bail;

	fop_fd->fops = fops;
	fop_fd->flags = *flags;
	fop_fd->fd = ret;
	fop_fd->filesystem_priv = NULL; /* we don't use it */
	fop_fd->len = (lws_filepos_t)stat_buf.st_size;
	fop_fd->pos = 0;

	return fop_fd;

bail:
	close(ret);
	return NULL;
}

int
_lws_plat_file_close(lws_fop_fd_t *fop_fd)
{
	int fd = (*fop_fd)->fd;

	free(*fop_fd);
	*fop_fd = NULL;

	return close(fd);
}

lws_fileofs_t
_lws_plat_file_seek_cur(lws_fop_fd_t fop_fd, lws_fileofs_t offset)
{
	lws_fileofs_t r;

	if (offset > 0 &&
	    offset > (lws_fileofs_t)fop_fd->len - (lws_fileofs_t)fop_fd->pos)
		offset = (lws_fileofs_t)(fop_fd->len - fop_fd->pos);

	if ((lws_fileofs_t)fop_fd->pos + offset < 0)
		offset = (lws_fileofs_t)(-fop_fd->pos);

	r = lseek(fop_fd->fd, (off_t)offset, SEEK_CUR);

	if (r >= 0)
		fop_fd->pos = (lws_filepos_t)r;
	else
		lwsl_err("error seeking from cur %ld, offset %ld\n",
                        (long)fop_fd->pos, (long)offset);

	return r;
}

int
_lws_plat_file_read(lws_fop_fd_t fop_fd, lws_filepos_t *amount,
		    uint8_t *buf, lws_filepos_t len)
{
	ssize_t n;

	n = read((int)fop_fd->fd, buf, (size_t)len);
	if (n == -1l) {
		*amount = 0;
		return -1;
	}
	fop_fd->pos = (lws_filepos_t)(fop_fd->pos + (lws_filepos_t)n);
	lwsl_debug("%s: read %ld of req %ld, pos %ld, len %ld\n", __func__,
			(long)n, (long)len, (long)fop_fd->pos,
			(long)fop_fd->len);
	*amount = (lws_filepos_t)n;

	return 0;
}

int
_lws_plat_file_write(lws_fop_fd_t fop_fd, lws_filepos_t *amount,
		     uint8_t *buf, lws_filepos_t len)
{
	ssize_t n;

	n = write((int)fop_fd->fd, buf, (size_t)len);
	if (n == -1) {
		*amount = 0;
		return -1;
	}

	fop_fd->pos = (lws_filepos_t)(fop_fd->pos + (lws_filepos_t)n);
	*amount = (lws_filepos_t)n;

	return 0;
}

