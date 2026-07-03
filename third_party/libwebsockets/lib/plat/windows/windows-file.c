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

#ifndef _WINSOCK_DEPRECATED_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#endif
#include "private-lib-core.h"

int lws_plat_apply_FD_CLOEXEC(int n)
{
	return 0;
}

lws_fop_fd_t
_lws_plat_file_open(const struct lws_plat_file_ops *fops, const char *filename,
		    const char *vpath, lws_fop_flags_t *flags)
{
	HANDLE ret;
	WCHAR buf[MAX_PATH];
	lws_fop_fd_t fop_fd;
	LARGE_INTEGER llFileSize = {0};

	MultiByteToWideChar(CP_UTF8, 0, filename, -1, buf, LWS_ARRAY_SIZE(buf));
	if (((*flags) & 7) == _O_RDONLY) {
		ret = CreateFileW(buf, GENERIC_READ, FILE_SHARE_READ,
			  NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	} else {
		ret = CreateFileW(buf, GENERIC_WRITE, 0, NULL,
			  CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	}

	if (ret == NULL)
		goto bail;

	fop_fd = malloc(sizeof(*fop_fd));
	if (!fop_fd)
		goto bail;

	fop_fd->fops = fops;
#if defined(__MINGW32__)
	/* we use filesystem_priv */
	fop_fd->fd = (int)(intptr_t)ret;
#else
	fop_fd->fd = ret;
#endif
	fop_fd->filesystem_priv = ret;
	fop_fd->flags = *flags;
	fop_fd->len = GetFileSize(ret, NULL);
	if(GetFileSizeEx(ret, &llFileSize))
		fop_fd->len = llFileSize.QuadPart;

	fop_fd->pos = 0;

	return fop_fd;

bail:
	return NULL;
}

int
_lws_plat_file_close(lws_fop_fd_t *fop_fd)
{
	HANDLE fd = (*fop_fd)->filesystem_priv;

	free(*fop_fd);
	*fop_fd = NULL;

	CloseHandle((HANDLE)fd);

	return 0;
}

lws_fileofs_t
_lws_plat_file_seek_cur(lws_fop_fd_t fop_fd, lws_fileofs_t offset)
{
	LARGE_INTEGER l;

	l.QuadPart = offset;
	if (!SetFilePointerEx((HANDLE)fop_fd->filesystem_priv, l, NULL, FILE_CURRENT))
	{
		lwsl_err("error seeking from cur %ld, offset %ld\n", (long)fop_fd->pos, (long)offset);
		return -1;
	}

	LARGE_INTEGER zero;
	zero.QuadPart = 0;
	LARGE_INTEGER newPos;
	if (!SetFilePointerEx((HANDLE)fop_fd->filesystem_priv, zero, &newPos, FILE_CURRENT))
	{
		lwsl_err("error seeking from cur %ld, offset %ld\n", (long)fop_fd->pos, (long)offset);
		return -1;
	}
	fop_fd->pos = newPos.QuadPart;

	return newPos.QuadPart;
}

int
_lws_plat_file_read(lws_fop_fd_t fop_fd, lws_filepos_t *amount,
		    uint8_t *buf, lws_filepos_t len)
{
	DWORD _amount;

	if (!ReadFile((HANDLE)fop_fd->filesystem_priv, buf, (DWORD)len, &_amount, NULL)) {
		*amount = 0;

		return 1;
	}

	fop_fd->pos += _amount;
	*amount = (unsigned long)_amount;

	return 0;
}

int
_lws_plat_file_write(lws_fop_fd_t fop_fd, lws_filepos_t *amount,
			 uint8_t* buf, lws_filepos_t len)
{
	DWORD _amount;

	if (!WriteFile((HANDLE)fop_fd->filesystem_priv, buf, (DWORD)len, &_amount, NULL)) {
		*amount = 0;

		return 1;
	}

	fop_fd->pos += _amount;
	*amount = (unsigned long)_amount;

	return 0;
}


int
lws_plat_write_cert(struct lws_vhost *vhost, int is_key, int fd, void *buf,
			size_t len)
{
	int n;

	n = (int)write(fd, buf, (unsigned int)len);

	lseek(fd, 0, SEEK_SET);

	return (size_t)n != len;
}

int
lws_plat_write_file(const char *filename, void *buf, size_t len)
{
	int m, fd;

	fd = lws_open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0600);

	if (fd == -1)
		return -1;

	m = (int)write(fd, buf, (unsigned int)len);
	close(fd);

	return (size_t)m != len;
}

int
lws_plat_read_file(const char *filename, void *buf, size_t len)
{
	int n, fd = lws_open(filename, O_RDONLY);
	if (fd == -1)
		return -1;

	n = (int)read(fd, buf, (unsigned int)len);
	close(fd);

	return n;
}

