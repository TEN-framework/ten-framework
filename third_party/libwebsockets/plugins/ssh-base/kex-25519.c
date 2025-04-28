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

#include "libwebsockets.h"
#include "lws-ssh.h"

#include <string.h>

/*
 * ssh-keygen -t ed25519
 * head -n-1 srv-key-25519 | tail -n +2 | base64 -d | hexdump -C
 */

static void
lws_sized_blob(uint8_t **p, void *blob, uint32_t len)
{
	lws_p32((*p), len);
	*p += 4;
	memcpy(*p, blob, len);
	*p += len;
}

static const char key_leadin[] = "openssh-key-v1\x00\x00\x00\x00\x04none"
				 "\x00\x00\x00\x04none\x00"
				 "\x00\x00\x00\x00\x00\x00\x01\x00\x00\x00\x33"
				 "\x00\x00\x00\x0bssh-ed25519\x00\x00\x00\x20",
		  key_sep[] = 	 "\x00\x00\x00\x90\xb1\x4f\xa7\x28"
				 "\xb1\x4f\xa7\x28\x00\x00\x00\x0bssh-ed25519"
				 "\x00\x00\x00\x20",
		  key_privl[] =	 "\x00\x00\x00\x40",
		  key_trail[] =  "\x00\x00\x00\x0cself-gen@cbl\x01";

static size_t
lws_gen_server_key_ed25519(struct lws_context *context, uint8_t *buf256,
			   size_t max_len)
{
	uint8_t *p = buf256 + sizeof(key_leadin) - 1;

	if (max_len < sizeof(key_leadin) - 1 + 32 + sizeof(key_sep) - 1 + 32 +
		      sizeof(key_privl) - 1 + 64 + sizeof(key_trail) - 1)
		return 0;

	memcpy(buf256, key_leadin, sizeof(key_leadin) - 1);
	crypto_sign_ed25519_keypair(context, p, p + 32 + sizeof(key_sep) - 1 +
				    32 + sizeof(key_privl) - 1);
	memcpy(p + 32 + sizeof(key_sep) - 1, p, 32);
	p += 32;
	memcpy(p, key_sep, sizeof(key_sep) - 1);
	p += sizeof(key_sep) - 1 + 32;
	memcpy(p, key_privl, sizeof(key_privl) - 1);
	p += sizeof(key_privl) - 1 + 64;
	memcpy(p, key_trail, sizeof(key_trail) - 1);
	p += sizeof(key_trail) - 1;

	lwsl_notice("%s: Generated key len %ld\n", __func__, (long)(p - buf256));

	return (size_t)(p - buf256);
}

static int
lws_mpint_rfc4251(uint8_t *dest, const uint8_t *src, int bytes, int uns)
{
	uint8_t *odest = dest;

	while (!*src && bytes > 1) {
		src++;
		bytes--;
	}

	if (!*src) {
		*dest++ = 0;
		*dest++ = 0;
		*dest++ = 0;
		*dest++ = 0;

		return 4;
	}

	if (uns && (*src) & 0x80)
		bytes++;

	*dest++ = (uint8_t)(bytes >> 24);
	*dest++ = (uint8_t)(bytes >> 16);
	*dest++ = (uint8_t)(bytes >> 8);
	*dest++ = (uint8_t)(bytes);

	if (uns && (*src) & 0x80) {
		*dest++ = 0;
		bytes--;
	}

	while (bytes--)
		*dest++ = *src++;

	return lws_ptr_diff(dest, odest);
}

int
ed25519_key_parse(uint8_t *p, size_t len, char *type, size_t type_len,
		  uint8_t *pub, uint8_t *pri)
{
	uint32_t l, publ, m;
	uint8_t *op = p;

	if (len < 180)
		return 1;

	if (memcmp(p, "openssh-key-v1", 14))
		return 2;

	p += 15;

	l = lws_g32(&p); /* ciphername */
	if (l != 4 || memcmp(p, "none", 4))
		return 3;
	p += l;

	l = lws_g32(&p); /* kdfname */
	if (l != 4 || memcmp(p, "none", 4))
		return 4;
	p += l;

	l = lws_g32(&p); /* kdfoptions */
	if (l)
		return 5;

	l = lws_g32(&p); /* number of keys */
	if (l != 1)
		return 6;

	publ = lws_g32(&p); /* length of pubkey block */
	if ((size_t)((uint32_t)(p - op) + publ) >= len)
		return 7;

	l = lws_g32(&p); /* key type length */
	if (l > 31)
		return 8;
	m = l;
	if (m >= type_len)
		m = (uint32_t)type_len -1 ;
	lws_strncpy(type, (const char *)p, m + 1);

	p += l;
	l = lws_g32(&p); /* pub key length */
	if (l != 32)
		return 10;

	p += l;

	publ = lws_g32(&p); /* length of private key block */
	if ((size_t)((uint32_t)(p - op) + publ) != len)
		return 11;

	l = lws_g32(&p); /* checkint 1 */
	if (lws_g32(&p) != l) /* must match checkint 2 */
		return 12;

	l = lws_g32(&p); /* key type length */

	p += l;
	l = lws_g32(&p); /* public key part length */
	if (l != LWS_SIZE_EC25519_PUBKEY)
		return 15;

	if (pub)
		memcpy(pub, p, LWS_SIZE_EC25519_PUBKEY);
	p += l;
	l = lws_g32(&p); /* private key part length */
	if (l != LWS_SIZE_EC25519_PRIKEY)
		return 16;

	if (pri)
		memcpy(pri, p, LWS_SIZE_EC25519_PRIKEY);

	return 0;
}

static int
_genhash_update_len(struct lws_genhash_ctx *ctx, const void *input, size_t ilen)
{
	uint32_t be;

	lws_p32((uint8_t *)&be, (uint32_t)ilen);

	if (lws_genhash_update(ctx, (uint8_t *)&be, 4))
		return 1;
	if (lws_genhash_update(ctx, input, ilen))
		return 1;

	return 0;
}

static int
kex_ecdh_dv(uint8_t *dest, int dest_len, const uint8_t *kbi, int kbi_len,
	    const uint8_t *H, char c, const uint8_t *session_id)
{
	uint8_t pool[LWS_SIZE_SHA256];
	struct lws_genhash_ctx ctx;
	int n = 0, m;

	/*
	 * Key data MUST be taken from the beginning of the hash output.
	 * As many bytes as needed are taken from the beginning of the hash
	 * value.
	 *
	 * If the key length needed is longer than the output of the HASH,
	 * the key is extended by computing HASH of the concatenation of K
	 * and H and the entire key so far, and appending the resulting
	 * bytes (as many as HASH generates) to the key.  This process is
	 * repeated until enough key material is available; the key is taken
	 * from the beginning of this value.  In other words:
	 *
	 * K1 = HASH(K || H || X || session_id)   (X is e.g., "A")
	 * K2 = HASH(K || H || K1)
	 * K3 = HASH(K || H || K1 || K2)
	 *      ...
	 * key = K1 || K2 || K3 || ...
	 */

	while (n < dest_len) {

		if (lws_genhash_init(&ctx, LWS_GENHASH_TYPE_SHA256))
			return 1;

		if (lws_genhash_update(&ctx, kbi, (unsigned int)kbi_len))
			goto hash_failed;
		if (lws_genhash_update(&ctx, H, LWS_SIZE_SHA256))
			goto hash_failed;

		if (!n) {
			if (lws_genhash_update(&ctx, (void *)&c, 1))
				goto hash_failed;
			if (lws_genhash_update(&ctx, session_id,
					      LWS_SIZE_EC25519))
				goto hash_failed;
		} else
			if (lws_genhash_update(&ctx, pool, LWS_SIZE_EC25519))
				goto hash_failed;

		lws_genhash_destroy(&ctx, pool);

		m = LWS_SIZE_EC25519;
		if (m > (dest_len - n))
			m = dest_len - n;

		memcpy(dest, pool, (unsigned int)m);
		n += m;
		dest += m;
	}

	return 0;

hash_failed:
	lws_genhash_destroy(&ctx, NULL);

	return 1;
}


static const unsigned char basepoint[32] = { 9 };

size_t
get_gen_server_key_25519(struct per_session_data__sshd *pss, uint8_t *b,
			 size_t len)
{
	size_t s, mylen;

	mylen = pss->vhd->ops->get_server_key(pss->wsi, b, len);
	if (mylen)
		return mylen;

	/* create one then */
	lwsl_notice("Generating server hostkey\n");
	s = lws_gen_server_key_ed25519(pss->vhd->context, b, len);
	lwsl_notice("  gen key len %ld\n", (long)s);
	if (!s)
		return 0;
	/* set the key */
	if (!pss->vhd->ops->set_server_key(pss->wsi, b, s))
		return 0;

	/* new key stored OK */

	return s;
}

int
kex_ecdh(struct per_session_data__sshd *pss, uint8_t *reply, uint32_t *plen)
{
	uint8_t pri_key[64], temp[64], payload_sig[64 + 32], a, *lp, kbi[64];
	struct lws_kex *kex = pss->kex;
	struct lws_genhash_ctx ctx;
        unsigned long long smlen;
	uint8_t *p = reply + 5;
	uint32_t be, kbi_len;
	uint8_t servkey[256];
	char keyt[33];
	int r, c;

	r = (int)get_gen_server_key_25519(pss, servkey, (int)sizeof(servkey));
	if (!r) {
		lwsl_err("%s: Failed to get or gen server key\n", __func__);

		return 1;
	}

	r = ed25519_key_parse(servkey, (unsigned int)r, keyt, sizeof(keyt),
			      pss->K_S /* public key */, pri_key);
	if (r) {
		lwsl_notice("%s: server key parse failed: %d\n", __func__, r);

		return 1;
	}
	keyt[32] = '\0';

	lwsl_info("Server key type: %s\n", keyt);

	/*
	 * 1) Generate ephemeral key pair [ eph_pri_key | kex->Q_S ]
	 * 2) Compute shared secret.
	 * 3) Generate and sign exchange hash.
	 *
	 * 1) A 32 bytes private key should be generated for each new
	 *    connection, using a secure PRNG. The following actions
	 *    must be done on the private key:
	 *
	 *     mysecret[0] &= 248;
	 *     mysecret[31] &= 127;
	 *     mysecret[31] |= 64;
	 */
	lws_get_random(pss->vhd->context, kex->eph_pri_key, LWS_SIZE_EC25519);
	kex->eph_pri_key[0] &= 248;
	kex->eph_pri_key[31] &= 127;
	kex->eph_pri_key[31] |= 64;

	/*
	 * 2) The public key is calculated using the cryptographic scalar
	 *    multiplication:
	 *
	 *     const unsigned char privkey[32];
	 *     unsigned char pubkey[32];
	 *
	 *     crypto_scalarmult (pubkey, privkey, basepoint);
	 */
	crypto_scalarmult_curve25519(kex->Q_S, kex->eph_pri_key, basepoint);

	a = 0;
	for (r = 0; r < (int)sizeof(kex->Q_S); r++)
		a |= kex->Q_S[r];
	if (!a) {
		lwsl_notice("all zero pubkey\n");
		return SSH_DISCONNECT_KEY_EXCHANGE_FAILED;
	}

	/*
	 * The shared secret, k, is defined in SSH specifications to be a big
	 * integer.  This number is calculated using the following procedure:
	 *
	 * X is the 32 bytes point obtained by the scalar multiplication of
	 * the other side's public key and the local private key scalar.
	 */
	crypto_scalarmult_curve25519(pss->K, kex->eph_pri_key, kex->Q_C);

	/*
	 * The whole 32 bytes of the number X are then converted into a big
	 * integer k.  This conversion follows the network byte order. This
	 * step differs from RFC5656.
	 */
	kbi_len = (uint32_t)lws_mpint_rfc4251(kbi, pss->K, LWS_SIZE_EC25519, 1);

	/*
	 * The exchange hash H is computed as the hash of the concatenation of
	 * the following:
	 *
	 *      string    V_C, the client's identification string (CR and LF
         *		       excluded)
	 *      string    V_S, the server's identification string (CR and LF
         *		       excluded)
	 *      string    I_C, the payload of the client's SSH_MSG_KEXINIT
	 *      string    I_S, the payload of the server's SSH_MSG_KEXINIT
	 *      string    K_S, the host key
	 *      mpint     Q_C, exchange value sent by the client
	 *      mpint     Q_S, exchange value sent by the server
	 *      mpint     K, the shared secret
	 *
	 * However there are a lot of unwritten details in the hash
	 * definition...
	 */

	if (lws_genhash_init(&ctx, LWS_GENHASH_TYPE_SHA256)) {
		lwsl_notice("genhash init failed\n");
		return 1;
	}

	if (_genhash_update_len(&ctx, pss->V_C, strlen(pss->V_C)))
		goto hash_probs;
	if (_genhash_update_len(&ctx, pss->vhd->ops->server_string, /* aka V_S */
			       strlen(pss->vhd->ops->server_string)))
		goto hash_probs;
	if (_genhash_update_len(&ctx, kex->I_C, kex->I_C_payload_len))
		goto hash_probs;
	if (_genhash_update_len(&ctx, kex->I_S, kex->I_S_payload_len))
		goto hash_probs;
	/*
	 * K_S (host public key)
	 *
	 * sum of name + key lengths and headers
	 * name length: name
	 * key length: key
	 * ---> */
	lws_p32((uint8_t *)&be, (uint32_t)(8 + (int)strlen(keyt) + LWS_SIZE_EC25519));
	if (lws_genhash_update(&ctx, (void *)&be, 4))
		goto hash_probs;

	if (_genhash_update_len(&ctx, keyt, strlen(keyt)))
		goto hash_probs;
	if (_genhash_update_len(&ctx, pss->K_S, LWS_SIZE_EC25519))
		goto hash_probs;
	/* <---- */

	if (_genhash_update_len(&ctx, kex->Q_C, LWS_SIZE_EC25519))
		goto hash_probs;
	if (_genhash_update_len(&ctx, kex->Q_S, LWS_SIZE_EC25519))
		goto hash_probs;

	if (lws_genhash_update(&ctx, kbi, kbi_len))
		goto hash_probs;

	if (lws_genhash_destroy(&ctx, temp))
		goto hash_probs;

	/*
	 * Sign the 32-byte SHA256 "exchange hash" in temp
	 * The signature is itself 64 bytes
	 */
        smlen = LWS_SIZE_EC25519 + 64;
        if (crypto_sign_ed25519(payload_sig, &smlen, temp, LWS_SIZE_EC25519,
        			pri_key))
		return 1;

#if 0
        l = LWS_SIZE_EC25519;
        n = crypto_sign_ed25519_open(temp, &l, payload_sig, smlen, pss->K_S);

        lwsl_notice("own sig sanity check says %d\n", n);
#endif

	/* sig [64] and payload [32] concatenated in payload_sig
	 *
	 * The server then responds with the following
	 *
	 *	uint32    packet length (exl self + mac)
	 *	byte      padding len
	 *      byte      SSH_MSG_KEX_ECDH_REPLY
	 *      string    server public host key and certificates (K_S)
	 *      string    Q_S (exchange value sent by the server)
	 *      string    signature of H
	 *      padding
	 */
	*p++ = SSH_MSG_KEX_ECDH_REPLY;

	/* server public host key and certificates (K_S) */

	lp = p;
	p +=4;
	lws_sized_blob(&p, keyt, (uint32_t)strlen(keyt));
	lws_sized_blob(&p, pss->K_S, LWS_SIZE_EC25519);
	lws_p32(lp, (uint32_t)(lws_ptr_diff(p, lp) - 4));

	/* Q_S (exchange value sent by the server) */
	
	lws_sized_blob(&p, kex->Q_S, LWS_SIZE_EC25519);

	/* signature of H */

	lp = p;
	p +=4;
	lws_sized_blob(&p, keyt, (uint32_t)strlen(keyt));
	lws_sized_blob(&p, payload_sig, 64);
	lws_p32(lp, (uint32_t)(lws_ptr_diff(p, lp) - 4));

	/* end of message */

	lws_pad_set_length(pss, reply, &p, &pss->active_keys_stc);
	*plen = (uint32_t)lws_ptr_diff(p, reply);

	if (!pss->active_keys_stc.valid)
		memcpy(pss->session_id, temp, LWS_SIZE_EC25519);

	/* RFC4253 7.2:
	 *
	 * The key exchange produces two values: a shared secret K,
	 * and an exchange hash H.  Encryption and authentication
	 * keys are derived from these.  The exchange hash H from the
	 * first key exchange is additionally used as the session
	 * identifier, which is a unique identifier for this connection.
	 * It is used by authentication methods as a part of the data
	 * that is signed as a proof of possession of a private key.
	 * Once computed, the session identifier is not changed,
	 * even if keys are later re-exchanged.
	 *
	 * The hash alg used in the KEX must be used for key derivation.
	 *
	 * 1) Initial IV client to server:
	 *
	 *     HASH(K || H || "A" || session_id)
	 *
	 * (Here K is encoded as mpint and "A" as byte and session_id
	 * as raw data.  "A" means the single character A, ASCII 65).
	 *
	 *
	 */
	for (c = 0; c < 3; c++) {
		kex_ecdh_dv(kex->keys_next_cts.key[c], LWS_SIZE_CHACHA256_KEY,
			    kbi, (int)kbi_len, temp, (char)('A' + (c * 2)),
			    pss->session_id);
		kex_ecdh_dv(kex->keys_next_stc.key[c], LWS_SIZE_CHACHA256_KEY,
			    kbi, (int)kbi_len, temp, (char)('B' + (c * 2)),
			    pss->session_id);
	}

	lws_explicit_bzero(temp, sizeof(temp));

	return 0;

hash_probs:
	lws_genhash_destroy(&ctx, NULL);

	return 1;
}
