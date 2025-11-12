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

/*
 * ie, if the plugins api needed at all
 */

#if defined(LWS_WITH_PLUGINS_API) && (UV_VERSION_MAJOR > 0)

const lws_plugin_header_t *
lws_plat_dlopen(struct lws_plugin **pplugin, const char *libpath,
		const char *sofilename, const char *_class,
		each_plugin_cb_t each, void *each_user)
{
	const lws_plugin_header_t *hdr;
	struct lws_plugin *pin;
	char sym[96], *dot;
	uv_lib_t lib;
	void *v;
	int m;

	lib.errmsg = NULL;
	lib.handle = NULL;

	if (uv_dlopen(libpath, &lib)) {
		uv_dlerror(&lib);
		lwsl_err("Error loading DSO: %s\n", lib.errmsg);
		uv_dlclose(&lib);
		return NULL;
	}

	/* we could open it... can we get his export struct? */
	m = lws_snprintf(sym, sizeof(sym) - 1, "%s", sofilename);
	if (m < 4)
		goto bail;
	dot = strchr(sym, '.');
	if (dot)
		*dot = '\0'; /* snip the .so or .lib or what-have-you*/

	if (uv_dlsym(&lib, sym, &v)) {
		uv_dlerror(&lib);
		lwsl_err("%s: Failed to get '%s' on %s: %s\n",
			 __func__, path, libpath, lib.errmsg);
		goto bail;
	}

	hdr = (const lws_plugin_header_t *)v;
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

	pin->u.lib = lib;
	pin->hdr = hdr;

	if (each)
		each(pin, each_user);

	return hdr;

bail:
	uv_dlclose(&lib);

	return NULL;
}

int
lws_plat_destroy_dl(struct lws_plugin *p)
{
	uv_dlclose(&p->u.lib);

	return 0;
}

#endif

/*
 * Specifically for protocol plugins support
 */

#if defined(LWS_WITH_PLUGINS) && (UV_VERSION_MAJOR > 0)

static int
protocol_plugin_cb(struct lws_plugin *pin, void *each_user)
{
	struct lws_context *context = (struct lws_context *)each_user;
	const lws_plugin_protocol_t *plpr =
				(const lws_plugin_protocol_t *)pin->hdr;

	context->plugin_protocol_count += plpr->count_protocols;
	context->plugin_extension_count += plpr->count_extensions;

	return 0;
}
#endif

int
lws_plat_plugins_init(struct lws_context *context, const char * const *d)
{
#if defined(LWS_WITH_PLUGINS) && (UV_VERSION_MAJOR > 0)
	if (info->plugin_dirs) {
		uv_loop_init(&context->uv.loop);
		lws_plugins_init(&context->plugin_list, info->plugin_dirs,
				 "lws_protocol_plugin", NULL,
				 protocol_plugin_cb, context);
	}
#endif

	return 0;
}

int
lws_plat_plugins_destroy(struct lws_context * context)
{
#if defined(LWS_WITH_PLUGINS) && (UV_VERSION_MAJOR > 0)
	if (lws_check_opt(context->options, LWS_SERVER_OPTION_LIBUV) &&
	    context->plugin_list) {
		lws_plugins_destroy(&context->plugin_list, NULL, NULL);
		while (uv_loop_close(&context->uv.loop))
			;
	}
#endif

	return 0;
}
