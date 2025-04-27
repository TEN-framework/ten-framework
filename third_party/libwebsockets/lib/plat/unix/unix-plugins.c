/*
 * libwebsockets - small server side websockets and web server implementation
 *
 * Copyright (C) 2010 - 2020 Andy Green <andy@warmcat.com>
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
#include <dlfcn.h>

const lws_plugin_header_t *
lws_plat_dlopen(struct lws_plugin **pplugin, const char *libpath,
		const char *sofilename, const char *_class,
		each_plugin_cb_t each, void *each_user)
{
	const lws_plugin_header_t *hdr;
	struct lws_plugin *pin;
	char sym[96];
	void *l;
	int m;

	if (strlen(sofilename) < 6)
		/* [lib]...[.so] */
		return NULL;

	lwsl_info("   trying %s\n", libpath);

	l = dlopen(libpath, RTLD_NOW);
	if (!l) {
		lwsl_info("%s: Error loading DSO: %s\n", __func__, dlerror());

		return NULL;
	}

	/* we could open it... can we get his export struct? */
	m = lws_snprintf(sym, sizeof(sym) - 1, "%s", sofilename);
	if (m < 4)
		goto bail;
	if (!strcmp(&sym[m - 3], ".so"))
		sym[m - 3] = '\0'; /* snip the .so */

	hdr = (const lws_plugin_header_t *)dlsym(l, sym);
	if (!hdr) {
		lwsl_info("%s: Failed to get export '%s' from %s: %s\n",
			 __func__, sym, libpath, dlerror());
		goto bail;
	}

	if (hdr->api_magic != LWS_PLUGIN_API_MAGIC) {
		lwsl_info("%s: plugin %s has outdated api %d (vs %d)\n",
			 __func__, libpath, hdr->api_magic,
			 LWS_PLUGIN_API_MAGIC);
		goto bail;
	}

	if (strcmp(hdr->lws_build_hash, LWS_BUILD_HASH))
		goto bail;

	if (strcmp(hdr->_class, _class))
		goto bail;

	/*
	 * We don't already have one of these, right?
	 */

	pin = *pplugin;
	while (pin) {
		if (!strcmp(pin->hdr->name, hdr->name))
			goto bail;
		pin = pin->list;
	}

	/*
	 * OK let's bring it in
	 */

	pin = lws_malloc(sizeof(*pin), __func__);
	if (!pin)
		goto bail;

	pin->list = *pplugin;
	*pplugin = pin;

	pin->u.l = l;
	pin->hdr = hdr;

	if (each)
		each(pin, each_user);

	lwsl_notice("   %s\n", libpath);

	return hdr;

bail:
	dlclose(l);

	return NULL;
}

int
lws_plat_destroy_dl(struct lws_plugin *p)
{
	return dlclose(p->u.l);
}
