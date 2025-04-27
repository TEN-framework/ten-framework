/*
 * lws-crypto-jwk
 *
 * Written in 2010-2019 by Andy Green <andy@warmcat.com>
 *
 * This file is made available under the Creative Commons CC0 1.0
 * Universal Public Domain Dedication.
 */

#include <libwebsockets.h>
#include <sys/types.h>
#include <fcntl.h>

/*
 * handles escapes and line wrapping suitable for use
 * defining a C char array ( -c option )
 */

static int
format_c(int fd, const char *key)
{
	const char *k = key;
	int seq = 0;

	while (*k) {
		if (*k == '{') {
			if (write(fd, "\"{\"\n\t\"", 6) < 6)
				return -1;
			k++;
			seq = 0;
			continue;
		}
		if (*k == '}') {
			if (write(fd, "\"\n\"}\"\n", 6) < 6)
				return -1;
			k++;
			seq = 0;
			continue;
		}
		if (*k == '\"') {
			if (write(fd, "\\\"", 2) < 2)
				return -1;
			seq += 2;
			k++;
			continue;
		}
		if (*k == ',') {
			if (write(fd, ",\"\n\t\"", 5) < 5)
				return -1;
			k++;
			seq = 0;
			continue;
		}
		if (write(fd, k, 1) < 1)
			return -1;
		seq++;
		if (seq >= 60) {
			if (write(fd, "\"\n\t \"", 5) < 5)
				return -1;
			seq = 1;
		}
		k++;
	}

	return 0;
}

int main(int argc, const char **argv)
{
	int result = 0, logs = LLL_USER | LLL_ERR | LLL_WARN | LLL_NOTICE;
	enum lws_gencrypto_kty kty = LWS_GENCRYPTO_KTY_RSA;
	struct lws_context_creation_info info;
	const char *curve = "P-256", *p;
	struct lws_context *context;
	struct lws_jwk jwk;
	int bits = 4096;
	char key[32768];
	int vl = sizeof(key);

	if ((p = lws_cmdline_option(argc, argv, "-d")))
		logs = atoi(p);

	lws_set_log_level(logs, NULL);
	lwsl_user("LWS JWK example\n");

	if ((p = lws_cmdline_option(argc, argv, "-b")))
		bits = atoi(p);

	if ((p = lws_cmdline_option(argc, argv, "-t"))) {
		if (!strcmp(p, "RSA"))
			kty = LWS_GENCRYPTO_KTY_RSA;
		else
			if (!strcmp(p, "OCT"))
				kty = LWS_GENCRYPTO_KTY_OCT;
			else
				if (!strcmp(p, "EC"))
					kty = LWS_GENCRYPTO_KTY_EC;
				else {
					lwsl_err("Unknown key type (must be "
						 "OCT, RSA or EC)\n");

					return 1;
				}
	}

	memset(&info, 0, sizeof info); /* otherwise uninitialized garbage */
#if defined(LWS_WITH_NETWORK)
	info.port = CONTEXT_PORT_NO_LISTEN;
#endif
	info.options = 0;

	context = lws_create_context(&info);
	if (!context) {
		lwsl_err("lws init failed\n");
		return 1;
	}

	if ((p = lws_cmdline_option(argc, argv, "-v")))
		curve = p;

	if (lws_jwk_generate(context, &jwk, kty, bits, curve)) {
		lwsl_err("lws_jwk_generate failed\n");

		return 1;
	}

	if ((p = lws_cmdline_option(argc, argv, "--kid")))
		lws_jwk_strdup_meta(&jwk, JWK_META_KID, p, (int)strlen(p));

	if ((p = lws_cmdline_option(argc, argv, "--use")))
		lws_jwk_strdup_meta(&jwk, JWK_META_USE, p, (int)strlen(p));

	if ((p = lws_cmdline_option(argc, argv, "--alg")))
		lws_jwk_strdup_meta(&jwk, JWK_META_ALG, p, (int)strlen(p));

	if ((p = lws_cmdline_option(argc, argv, "--key-ops")))
		lws_jwk_strdup_meta(&jwk, JWK_META_KEY_OPS, p, (int)strlen(p));

	if ((p = lws_cmdline_option(argc, argv, "--public")) &&
	    kty != LWS_GENCRYPTO_KTY_OCT) {

		int fd;

		/* public version */

		if (lws_jwk_export(&jwk, 0, key, &vl) < 0) {
			lwsl_err("lws_jwk_export failed\n");

			return 1;
		}

		fd = open(p, LWS_O_CREAT | LWS_O_TRUNC | LWS_O_WRONLY, 0600);
		if (fd < 0) {
			lwsl_err("Can't open public key file %s\n", p);
			return 1;
		}

		if (lws_cmdline_option(argc, argv, "-c"))
			format_c(fd, key);
		else {
			if (write(fd, key,
#if defined(WIN32)
					(unsigned int)
#endif
					strlen(key)) < 0) {
				lwsl_err("Write public failed\n");
				return 1;
			}
		}

		close(fd);
	}

	/* private version */

	if (lws_jwk_export(&jwk, LWSJWKF_EXPORT_PRIVATE, key, &vl) < 0) {
		lwsl_err("lws_jwk_export failed\n");

		return 1;
	}

	if (lws_cmdline_option(argc, argv, "-c")) {
		if (format_c(1, key) < 0)
			return 1;
	} else
		if (write(1, key,
#if defined(WIN32)
				(unsigned int)
#endif
				strlen(key)) < 0) {
			lwsl_err("Write stdout failed\n");
			return 1;
		}

	lws_jwk_destroy(&jwk);

	lws_context_destroy(context);

	return result;
}
