/*
 * This code originally came from here
 *
 * http://base64.sourceforge.net/b64.c
 *
 * already with MIT license, which is retained.
 *
 * LICENCE:        Copyright (c) 2001 Bob Trower, Trantor Standard Systems Inc.
 *
 *                Permission is hereby granted, free of charge, to any person
 *                obtaining a copy of this software and associated
 *                documentation files (the "Software"), to deal in the
 *                Software without restriction, including without limitation
 *                the rights to use, copy, modify, merge, publish, distribute,
 *                sublicense, and/or sell copies of the Software, and to
 *                permit persons to whom the Software is furnished to do so,
 *                subject to the following conditions:
 *
 *                The above copyright notice and this permission notice shall
 *                be included in all copies or substantial portions of the
 *                Software.
 *
 *                THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY
 *                KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
 *                WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 *                PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS
 *                OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 *                OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 *                OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 *                SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * VERSION HISTORY:
 *               Bob Trower 08/04/01 -- Create Version 0.00.00B
 *
 * I cleaned it up quite a bit to match the (linux kernel) style of the rest
 * of libwebsockets
 */

#include "private-lib-core.h"

#include <stdio.h>
#include <string.h>

static const char encode_orig[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
			     "abcdefghijklmnopqrstuvwxyz0123456789+/";
static const char encode_url[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
			     "abcdefghijklmnopqrstuvwxyz0123456789-_";
static const char decode[] = "|$$$}rstuvwxyz{$$$$$$$>?@ABCDEFGHIJKLMNOPQRSTUVW"
			     "$$$$$$XYZ[\\]^_`abcdefghijklmnopq";

static int
_lws_b64_encode_string(const char *encode, const char *in, int in_len,
		       char *out, int out_size)
{
	unsigned char triple[3];
	int i, done = 0;

	while (in_len) {
		int len = 0;
		for (i = 0; i < 3; i++) {
			if (in_len) {
				triple[i] = (unsigned char)*in++;
				len++;
				in_len--;
			} else
				triple[i] = 0;
		}

		if (done + 4 >= out_size)
			return -1;

		*out++ = encode[triple[0] >> 2];
		*out++ = encode[(((triple[0] & 0x03) << 4) & 0x30) |
					     (((triple[1] & 0xf0) >> 4) & 0x0f)];
		*out++ = (char)(len > 1 ? encode[(((triple[1] & 0x0f) << 2) & 0x3c) |
					(((triple[2] & 0xc0) >> 6) & 3)] : '=');
		*out++ = (char)(len > 2 ? encode[triple[2] & 0x3f] : '=');

		done += 4;
	}

	if (done + 1 >= out_size)
		return -1;

	*out++ = '\0';

	return done;
}

int
lws_b64_encode_string(const char *in, int in_len, char *out, int out_size)
{
	return _lws_b64_encode_string(encode_orig, in, in_len, out, out_size);
}

int
lws_b64_encode_string_url(const char *in, int in_len, char *out, int out_size)
{
	return _lws_b64_encode_string(encode_url, in, in_len, out, out_size);
}


void
lws_b64_decode_state_init(struct lws_b64state *state)
{
	memset(state, 0, sizeof(*state));
}

int
lws_b64_decode_stateful(struct lws_b64state *s, const char *in, size_t *in_len,
			uint8_t *out, size_t *out_size, int final)
{
	const char *orig_in = in, *end_in = in + *in_len;
	uint8_t *orig_out = out, *end_out = out + *out_size;

	while (in < end_in && *in && out + 4 < end_out) {

		for (; s->i < 4 && in < end_in && *in; s->i++) {
			uint8_t v;

			v = 0;
			s->c = 0;
			while (in < end_in && *in && !v) {
				s->c = v = (unsigned char)*in++;
				/* support the url base64 variant too */
				if (v == '-')
					s->c = v = '+';
				if (v == '_')
					s->c = v = '/';
				v = (uint8_t)((v < 43 || v > 122) ? 0 : decode[v - 43]);
				if (v)
					v = (uint8_t)((v == '$') ? 0 : v - 61);
			}
			if (s->c) {
				s->len++;
				if (v)
					s->quad[s->i] = (uint8_t)(v - 1);
			} else
				s->quad[s->i] = 0;
		}

		if (s->i != 4 && !final)
			continue;

		s->i = 0;

		/*
		 * "The '==' sequence indicates that the last group contained
		 * only one byte, and '=' indicates that it contained two
		 * bytes." (wikipedia)
		 */

		if ((in >= end_in || !*in) && s->c == '=')
			s->len--;

		if (s->len >= 2)
			*out++ = (uint8_t)(s->quad[0] << 2 | s->quad[1] >> 4);
		if (s->len >= 3)
			*out++ = (uint8_t)(s->quad[1] << 4 | s->quad[2] >> 2);
		if (s->len >= 4)
			*out++ = (uint8_t)(((s->quad[2] << 6) & 0xc0) | s->quad[3]);

		s->done += s->len - 1;
		s->len = 0;
	}

	*out = '\0';
	*in_len = (unsigned int)(in - orig_in);
	*out_size = (unsigned int)(out - orig_out);

	return 0;
}


/*
 * returns length of decoded string in out, or -1 if out was too small
 * according to out_size
 *
 * Only reads up to in_len chars, otherwise if in_len is -1 on entry reads until
 * the first NUL in the input.
 */

static size_t
_lws_b64_decode_string(const char *in, int in_len, char *out, size_t out_size)
{
	struct lws_b64state state;
	size_t il = (size_t)in_len, ol = out_size;

	if (in_len == -1)
		il = strlen(in);

	lws_b64_decode_state_init(&state);
	lws_b64_decode_stateful(&state, in, &il, (uint8_t *)out, &ol, 1);

	if (!il)
		return 0;

	return ol;
}

int
lws_b64_decode_string(const char *in, char *out, int out_size)
{
	return (int)_lws_b64_decode_string(in, -1, out, (unsigned int)out_size);
}

int
lws_b64_decode_string_len(const char *in, int in_len, char *out, int out_size)
{
	return (int)_lws_b64_decode_string(in, in_len, out, (unsigned int)out_size);
}

#if 0
static const char * const plaintext[] = {
	"any carnal pleasure.",
	"any carnal pleasure",
	"any carnal pleasur",
	"any carnal pleasu",
	"any carnal pleas",
	"Admin:kloikloi"
};
static const char * const coded[] = {
	"YW55IGNhcm5hbCBwbGVhc3VyZS4=",
	"YW55IGNhcm5hbCBwbGVhc3VyZQ==",
	"YW55IGNhcm5hbCBwbGVhc3Vy",
	"YW55IGNhcm5hbCBwbGVhc3U=",
	"YW55IGNhcm5hbCBwbGVhcw==",
	"QWRtaW46a2xvaWtsb2k="
};

int
lws_b64_selftest(void)
{
	char buf[64];
	unsigned int n,  r = 0;
	unsigned int test;

	lwsl_notice("%s\n", __func__);

	/* examples from https://en.wikipedia.org/wiki/Base64 */

	for (test = 0; test < (int)LWS_ARRAY_SIZE(plaintext); test++) {

		buf[sizeof(buf) - 1] = '\0';
		n = lws_b64_encode_string(plaintext[test],
				      strlen(plaintext[test]), buf, sizeof buf);
		if (n != strlen(coded[test]) || strcmp(buf, coded[test])) {
			lwsl_err("Failed lws_b64 encode selftest "
					   "%d result '%s' %d\n", test, buf, n);
			r = -1;
		}

		buf[sizeof(buf) - 1] = '\0';
		n = lws_b64_decode_string(coded[test], buf, sizeof buf);
		if (n != strlen(plaintext[test]) ||
		    strcmp(buf, plaintext[test])) {
			lwsl_err("Failed lws_b64 decode selftest "
				 "%d result '%s' / '%s', %d / %zu\n",
				 test, buf, plaintext[test], n,
				 strlen(plaintext[test]));
			lwsl_hexdump_err(buf, n);
			r = -1;
		}
	}

	if (!r)
		lwsl_notice("Base 64 selftests passed\n");
	else
		lwsl_notice("Base64 selftests failed\n");

	return r;
}
#endif
