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

void
lws_set_fops(struct lws_context *context, const struct lws_plat_file_ops *fops)
{
	context->fops = fops;
}

lws_filepos_t
lws_vfs_tell(lws_fop_fd_t fop_fd)
{
	return fop_fd->pos;
}

lws_filepos_t
lws_vfs_get_length(lws_fop_fd_t fop_fd)
{
	return fop_fd->len;
}

uint32_t
lws_vfs_get_mod_time(lws_fop_fd_t fop_fd)
{
	return fop_fd->mod_time;
}

lws_fileofs_t
lws_vfs_file_seek_set(lws_fop_fd_t fop_fd, lws_fileofs_t offset)
{
	lws_fileofs_t ofs;

	ofs = fop_fd->fops->LWS_FOP_SEEK_CUR(fop_fd,
			offset - (lws_fileofs_t)fop_fd->pos);

	return ofs;
}


lws_fileofs_t
lws_vfs_file_seek_end(lws_fop_fd_t fop_fd, lws_fileofs_t offset)
{
	return fop_fd->fops->LWS_FOP_SEEK_CUR(fop_fd,
			(lws_fileofs_t)fop_fd->len + (lws_fileofs_t)fop_fd->pos + offset);
}


const struct lws_plat_file_ops *
lws_vfs_select_fops(const struct lws_plat_file_ops *fops, const char *vfs_path,
		    const char **vpath)
{
	const struct lws_plat_file_ops *pf;
	const char *p = vfs_path;
	int n;

	*vpath = NULL;

	/* no non-platform fops, just use that */

	if (!fops->next)
		return fops;

	/*
	 *  scan the vfs path looking for indications we are to be
	 * handled by a specific fops
	 */

	while (p && *p) {
		if (*p != '/') {
			p++;
			continue;
		}
		/* the first one is always platform fops, so skip */
		pf = fops->next;
		while (pf) {
			n = 0;
			while (n < (int)LWS_ARRAY_SIZE(pf->fi) && pf->fi[n].sig) {
				if (p >= vfs_path + pf->fi[n].len)
					if (!strncmp(p - (pf->fi[n].len - 1),
						     pf->fi[n].sig,
						     (unsigned int)(pf->fi[n].len - 1))) {
						*vpath = p + 1;
						return pf;
					}

				n++;
			}
			pf = pf->next;
		}
		p++;
	}

	return fops;
}

lws_fop_fd_t LWS_WARN_UNUSED_RESULT
lws_vfs_file_open(const struct lws_plat_file_ops *fops, const char *vfs_path,
		  lws_fop_flags_t *flags)
{
	const char *vpath = "";
	const struct lws_plat_file_ops *selected;

	selected = lws_vfs_select_fops(fops, vfs_path, &vpath);

	return selected->LWS_FOP_OPEN(fops, vfs_path, vpath, flags);
}


struct lws_plat_file_ops *
lws_get_fops(struct lws_context *context)
{
	return (struct lws_plat_file_ops *)context->fops;
}

