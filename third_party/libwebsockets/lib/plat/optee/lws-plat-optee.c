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

#if !defined(LWS_WITH_NETWORK)
#include <crypto/crypto.h>
#endif

int errno;

#if !defined(LWS_WITH_NETWORK)
char *
strcpy(char *dest, const char *src)
{
	char *desto = dest;

	while (*src)
		*(dest++) = *(src++);

	*(dest++) = '\0';

	return desto;
}

char *strncpy(char *dest, const char *src, size_t limit)
{
	char *desto = dest;

	while (*src && limit--)
		*(dest++) = *(src++);

	if (limit)
		*(dest++) = '\0';

	return desto;
}

#endif

int lws_plat_apply_FD_CLOEXEC(int n)
{
	return 0;
}

void TEE_GenerateRandom(void *randomBuffer, uint32_t randomBufferLen);
#if defined(LWS_WITH_NETWORK)
uint64_t
lws_now_usecs(void)
{
	return ((unsigned long long)time(NULL)) * 1000000;
}
#endif

size_t
lws_get_random(struct lws_context *context, void *buf, size_t len)
{
#if defined(LWS_WITH_NETWORK)
	TEE_GenerateRandom(buf, len);
#else
	crypto_rng_read(buf, len);
#endif

	return len;
}


static const char * const colours[] = {
        "[31;1m", /* LLL_ERR */
        "[36;1m", /* LLL_WARN */
        "[35;1m", /* LLL_NOTICE */
        "[32;1m", /* LLL_INFO */
        "[34;1m", /* LLL_DEBUG */
        "[33;1m", /* LLL_PARSER */
        "[33;1m", /* LLL_HEADER */
        "[33;1m", /* LLL_EXT */
        "[33;1m", /* LLL_CLIENT */
        "[33;1m", /* LLL_LATENCY */
        "[30;1m", /* LLL_USER */
};

void lwsl_emit_optee(int level, const char *line)
{
        char buf[50], linecp[512];
        int n, m = LWS_ARRAY_SIZE(colours) - 1;

        lwsl_timestamp(level, buf, sizeof(buf));

        n = 1 << (LWS_ARRAY_SIZE(colours) - 1);
        while (n) {
                if (level & n)
                        break;
                m--;
                n >>= 1;
        }
        n = strlen(line);
        if ((unsigned int)n > sizeof(linecp) - 1)
                n = sizeof(linecp) - 1;
        if (n) {
                memcpy(linecp, line, n - 1);
	        linecp[n - 1] = '\0';
	} else
		linecp[0] = '\0';
        EMSG("%c%s%s%s%c[0m", 27, colours[m], buf, linecp, 27);
}

int
lws_plat_set_nonblocking(lws_sockfd_type fd)
{
	return 0;
}

int
lws_plat_drop_app_privileges(struct lws_context *context, int actually_init)
{
	return 0;
}

int
lws_plat_context_early_init(void)
{
	return 0;
}

void
lws_plat_context_early_destroy(struct lws_context *context)
{
}

void
lws_plat_context_late_destroy(struct lws_context *context)
{
#if defined(LWS_WITH_NETWORK)
	if (context->lws_lookup)
		lws_free(context->lws_lookup);
#endif
}

lws_fop_fd_t
_lws_plat_file_open(const struct lws_plat_file_ops *fops,
		    const char *filename, const char *vpath, lws_fop_flags_t *flags)
{
	return NULL;
}

int
_lws_plat_file_close(lws_fop_fd_t *fop_fd)
{
	return 0;
}

lws_fileofs_t
_lws_plat_file_seek_cur(lws_fop_fd_t fop_fd, lws_fileofs_t offset)
{
	return 0;
}

 int
_lws_plat_file_read(lws_fop_fd_t fop_fd, lws_filepos_t *amount,
		    uint8_t *buf, lws_filepos_t len)
{

	return 0;
}

 int
_lws_plat_file_write(lws_fop_fd_t fop_fd, lws_filepos_t *amount,
		     uint8_t *buf, lws_filepos_t len)
{

	return 0;
}


int
lws_plat_init(struct lws_context *context,
	      const struct lws_context_creation_info *info)
{
#if defined(LWS_WITH_NETWORK)
	/* context has the global fd lookup array */
	context->lws_lookup = lws_zalloc(sizeof(struct lws *) *
					 context->max_fds, "lws_lookup");
	if (context->lws_lookup == NULL) {
		lwsl_err("OOM on lws_lookup array for %d connections\n",
			 context->max_fds);
		return 1;
	}

	lwsl_notice(" mem: platform fd map: %5lu bytes\n",
		    (long)sizeof(struct lws *) * context->max_fds);
#endif
#ifdef LWS_WITH_PLUGINS
	if (info->plugin_dirs)
		lws_plat_plugins_init(context, info->plugin_dirs);
#endif

	return 0;
}

int
lws_plat_write_file(const char *filename, void *buf, size_t len)
{
	return 1;
}

int
lws_plat_read_file(const char *filename, void *buf, int len)
{
	return -1;
}

int
lws_plat_recommended_rsa_bits(void)
{
	return 4096;
}

int
lws_plat_ntpclient_config(struct lws_context *context)
{
#if 0
	char *ntpsrv = getenv("LWS_NTP_SERVER");

	if (ntpsrv && strlen(ntpsrv) < 64) {
		lws_system_blob_heap_append(lws_system_get_blob(context,
					    LWS_SYSBLOB_TYPE_NTP_SERVER, 0),
					    (const uint8_t *)ntpsrv,
					    strlen(ntpsrv));
		return 1;
	}
#endif
	return 0;
}

void
lws_msleep(unsigned int ms)
{
}


