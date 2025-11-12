/*
 * libwebsockets - small server side websockets and web server implementation
 *
 * Copyright (C) 2010 - 2021 Andy Green <andy@warmcat.com>
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
#include "private-lib-tls-openssl.h"

extern int openssl_websocket_private_data_index,
	   openssl_SSL_CTX_private_data_index;
#if defined(LWS_WITH_NETWORK)
static char openssl_ex_indexes_acquired;
#endif

void
lws_tls_err_describe_clear(void)
{
	char buf[160];
	unsigned long l;

	do {
		l = ERR_get_error();
		if (!l)
			break;

		ERR_error_string_n(
#if defined(LWS_WITH_BORINGSSL)
				(uint32_t)
#endif
				l, buf, sizeof(buf));
		lwsl_info("   openssl error: %s\n", buf);
	} while (l);
	lwsl_info("\n");
}

#if LWS_MAX_SMP != 1

static pthread_mutex_t *openssl_mutexes = NULL;

static void
lws_openssl_lock_callback(int mode, int type, const char *file, int line)
{
	(void)file;
	(void)line;

	if (mode & CRYPTO_LOCK)
		pthread_mutex_lock(&openssl_mutexes[type]);
	else
		pthread_mutex_unlock(&openssl_mutexes[type]);
}

static unsigned long
lws_openssl_thread_id(void)
{
#ifdef __PTW32_H
	return (unsigned long)(intptr_t)(pthread_self()).p;
#else
	return (unsigned long)pthread_self();
#endif
}
#endif

int
lws_context_init_ssl_library(struct lws_context *cx,
                             const struct lws_context_creation_info *info)
{
#ifdef USE_WOLFSSL
#ifdef USE_OLD_CYASSL
	lwsl_cx_info(cx, " Compiled with CyaSSL support");
#else
	lwsl_cx_info(cx, " Compiled with wolfSSL support");
#endif
#else
#if defined(LWS_WITH_BORINGSSL)
	lwsl_cx_info(cx, " Compiled with BoringSSL support");
#else
	lwsl_cx_info(cx, " Compiled with OpenSSL support");
#endif
#endif
	if (!lws_check_opt(info->options, LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT)) {
		lwsl_cx_info(cx, " SSL disabled: no "
			  "LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT");
		return 0;
	}

	/* basic openssl init */

	lwsl_cx_info(cx, "Doing SSL library init");

#if OPENSSL_VERSION_NUMBER < 0x10100000L
	SSL_library_init();
	OpenSSL_add_all_algorithms();
	SSL_load_error_strings();
#else
	OPENSSL_init_ssl(OPENSSL_INIT_LOAD_SSL_STRINGS, NULL);
#endif
#if defined(LWS_WITH_NETWORK)
	if (!openssl_ex_indexes_acquired) {
		openssl_websocket_private_data_index =
			SSL_get_ex_new_index(0, "lws", NULL, NULL, NULL);

		openssl_SSL_CTX_private_data_index =
			SSL_CTX_get_ex_new_index(0, NULL, NULL, NULL, NULL);

		openssl_ex_indexes_acquired = 1;
	}
#endif

#if LWS_MAX_SMP != 1
	{
		int n;

		openssl_mutexes = (pthread_mutex_t *)
				OPENSSL_malloc((size_t)((unsigned long)CRYPTO_num_locks() *
					       (unsigned long)sizeof(openssl_mutexes[0])));

		for (n = 0; n < CRYPTO_num_locks(); n++)
			pthread_mutex_init(&openssl_mutexes[n], NULL);

		/*
		 * These "functions" disappeared in later OpenSSL which is
		 * already threadsafe.
		 */

		(void)lws_openssl_thread_id;
		(void)lws_openssl_lock_callback;

		CRYPTO_set_id_callback(lws_openssl_thread_id);
		CRYPTO_set_locking_callback(lws_openssl_lock_callback);
	}
#endif

	return 0;
}

void
lws_context_deinit_ssl_library(struct lws_context *context)
{
#if LWS_MAX_SMP != 1
	int n;

	if (!lws_check_opt(context->options,
			   LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT))
		return;

	CRYPTO_set_locking_callback(NULL);

	if (openssl_mutexes) {
		for (n = 0; n < CRYPTO_num_locks(); n++)
			pthread_mutex_destroy(&openssl_mutexes[n]);

		OPENSSL_free(openssl_mutexes);
		openssl_mutexes = NULL;
	}
#endif
}
