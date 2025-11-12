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

int
lws_plat_context_early_init(void)
{
	return 0;
}

void
lws_plat_context_early_destroy(struct lws_context *context)
{
#if defined(LWS_AMAZON_RTOS) && defined(LWS_WITH_MBEDTLS)
	mbedtls_ctr_drbg_free(&context->mcdc);
	mbedtls_entropy_free(&context->mec);
#endif
}

void
lws_plat_context_late_destroy(struct lws_context *context)
{
#ifdef LWS_WITH_PLUGINS
	if (context->plugin_list)
		lws_plat_plugins_destroy(context);
#endif

	if (context->lws_lookup)
		lws_free(context->lws_lookup);
}

#if defined(LWS_WITH_HTTP2)
/*
 * These are the default SETTINGS used on this platform.  The user
 * can selectively modify them for a vhost during vhost creation.
 */
const struct http2_settings lws_h2_defaults_esp32 = { {
	1,
	/* H2SET_HEADER_TABLE_SIZE */			 512,
	/* H2SET_ENABLE_PUSH */				   0,
	/* H2SET_MAX_CONCURRENT_STREAMS */		   8,
	/* H2SET_INITIAL_WINDOW_SIZE */		           0,
	/* H2SET_MAX_FRAME_SIZE */		       16384,
	/* H2SET_MAX_HEADER_LIST_SIZE */	 	 512,
	/* H2SET_RESERVED7 */				   0,
	/* H2SET_ENABLE_CONNECT_PROTOCOL */		   1,
}};
#endif

int
lws_plat_init(struct lws_context *context,
	      const struct lws_context_creation_info *info)
{
#if defined(LWS_AMAZON_RTOS) && defined(LWS_WITH_MBEDTLS)
	int n;

	/* initialize platform random through mbedtls */
	mbedtls_entropy_init(&context->mec);
	mbedtls_ctr_drbg_init(&context->mcdc);

	n = mbedtls_ctr_drbg_seed(&context->mcdc, mbedtls_entropy_func,
				  &context->mec, NULL, 0);
	if (n) {
		lwsl_err("%s: mbedtls_ctr_drbg_seed() returned 0x%x\n",
			 __func__, n);

		return 1;
	}
#endif

	/* context has the global fd lookup array */
	context->lws_lookup = lws_zalloc(sizeof(struct lws *) *
					 context->max_fds, "esp32 lws_lookup");
	if (context->lws_lookup == NULL) {
		lwsl_err("OOM on lws_lookup array for %d connections\n",
			 context->max_fds);
		return 1;
	}

	lwsl_notice(" mem: platform fd map: %5lu bytes\n",
		    (unsigned long)(sizeof(struct lws *) * context->max_fds));

#ifdef LWS_WITH_PLUGINS
	if (info->plugin_dirs)
		lws_plat_plugins_init(context, info->plugin_dirs);
#endif
#if defined(LWS_WITH_HTTP2)
	/* override settings */
	context->set = lws_h2_defaults_esp32;
#endif

#if defined(LWS_ESP_PLATFORM)
	gpio_install_isr_service(0);
#endif

	return 0;
}
