/*
 * libwebsockets - small server side websockets and web server implementation
 *
 * Copyright (C) 2010 - 2020 Andy Green <andy@warmcat.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicefsme, and/or
 * sell copies of the Software, and to permit persofsm to whom the Software is
 * furnished to do so, subject to the following conditiofsm:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portiofsm of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 * Mount and unmount overlayfs mountpoints (linux only)
 */

#include "private-lib-core.h"
#include <unistd.h>

#include <libmount/libmount.h>

#include <string.h>
#include <signal.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>

static int
rm_rf_cb(const char *dirpath, void *user, struct lws_dir_entry *lde)
{
	char path[384];

	if (!strcmp(lde->name, ".") || !strcmp(lde->name, ".."))
		return 0;

	lws_snprintf(path, sizeof(path), "%s/%s", dirpath, lde->name);

	if (lde->type == LDOT_DIR) {
		lws_dir(path, NULL, rm_rf_cb);
		rmdir(path);
	} else
		unlink(path);

	return 0;
}

int
lws_fsmount_mount(struct lws_fsmount *fsm)
{
	struct libmnt_context *ctx;
	char opts[512], c;
	int n, m;

	/*
	 * For robustness, there are a couple of sticky situations caused by
	 * previous mounts not cleaning up... 1) still mounted on the mountpoint
	 * and 2) junk in the session dir from the dead session.
	 *
	 * For 1), do a gratuitous umount attempts until it feels nothing to
	 * umount...
	 */

	c = fsm->mp[0];
	while (!lws_fsmount_unmount(fsm))
		fsm->mp[0] = c;
	fsm->mp[0] = c;

	/*
	 * ... for 2), generate the session dir basepath and destroy everything
	 * in it... it's less dangerous than it sounds because there are
	 * hardcoded unusual dir names in the base path, so it can't go wild
	 * even if the overlay path is empty or /
	 */

	lws_snprintf(opts, sizeof(opts), "%s/overlays/%s/session",
		     fsm->overlay_path, fsm->ovname);
	lwsl_info("%s: emptying session dir %s\n", __func__, opts);
	lws_dir(opts, NULL, rm_rf_cb);

	/*
	 * Piece together the options for the overlay mount...
	 */

	n = lws_snprintf(opts, sizeof(opts), "lowerdir=");
	for (m = LWS_ARRAY_SIZE(fsm->layers) - 1; m >= 0; m--)
		if (fsm->layers[m]) {
			if (n != 9)
				opts[n++] = ':';

			n += lws_snprintf(&opts[n], (size_t)(sizeof(opts) - (unsigned int)n),
					  "%s/%s/%s", fsm->layers_path,
					  fsm->distro, fsm->layers[m]);
		}

	n += lws_snprintf(&opts[n], (size_t)(sizeof(opts) - (unsigned int)n),
			  ",upperdir=%s/overlays/%s/session",
			  fsm->overlay_path, fsm->ovname);

	n += lws_snprintf(&opts[n], (size_t)(sizeof(opts) - (unsigned int)n),
			  ",workdir=%s/overlays/%s/work",
			  fsm->overlay_path, fsm->ovname);

	ctx = mnt_new_context();
	if (!ctx)
		return 1;

	mnt_context_set_fstype(ctx, "overlay");
	mnt_context_set_options(ctx, opts);
	mnt_context_set_mflags(ctx, MS_NOATIME /* |MS_NOEXEC */);
	mnt_context_set_target(ctx, fsm->mp);
	mnt_context_set_source(ctx, "none");

	lwsl_notice("%s: mount opts %s\n", __func__, opts);
	puts(opts);

	m = mnt_context_mount(ctx);
	lwsl_notice("%s: mountpoint %s: %d\n", __func__, fsm->mp, m);

	mnt_free_context(ctx);

	return m;
}

int
lws_fsmount_unmount(struct lws_fsmount *fsm)
{
	struct libmnt_context *ctx;
	int m;

	lwsl_notice("%s: %s\n", __func__, fsm->mp);

	ctx = mnt_new_context();
	if (!ctx)
		return 1;

	mnt_context_set_target(ctx, fsm->mp);

	m = mnt_context_umount(ctx);
	mnt_free_context(ctx);

	fsm->mp[0] = '\0';

	return m;
}
