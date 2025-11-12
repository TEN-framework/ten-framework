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
#include <stdlib.h>

void *sshd_zalloc(size_t s)
{
	void *p = malloc(s);

	if (p)
		memset(p, 0, s);

	return p;
}

uint32_t
lws_g32(uint8_t **p)
{
	uint32_t v = 0;

	v = (v << 8) | *((*p)++);
	v = (v << 8) | *((*p)++);
	v = (v << 8) | *((*p)++);
	v = (v << 8) | *((*p)++);

	return v;
}

uint32_t
lws_p32(uint8_t *p, uint32_t v)
{
	*p++ = (uint8_t)(v >> 24);
	*p++ = (uint8_t)(v >> 16);
	*p++ = (uint8_t)(v >> 8);
	*p++ = (uint8_t)v;

	return v;
}

int
lws_cstr(uint8_t **p, const char *s, uint32_t max)
{
	uint32_t n = (uint32_t)strlen(s);

	if (n > max)
		return 1;

	lws_p32(*p, n);
	*p += 4;
	strcpy((char *)(*p), s);
	*p += n;

	return 0;
}

int
lws_buf(uint8_t **p, void *s, uint32_t len)
{
	lws_p32(*p, len);
	*p += 4;
	memcpy((char *)(*p), s, len);
	*p += len;

	return 0;
}

void
write_task(struct per_session_data__sshd *pss, struct lws_ssh_channel *ch,
	   int task)
{
	pss->write_task[pss->wt_head] = (uint8_t)task;
	pss->write_channel[pss->wt_head] = ch;
	pss->wt_head = (pss->wt_head + 1) & 7;
	lws_callback_on_writable(pss->wsi);
}

void
write_task_insert(struct per_session_data__sshd *pss, struct lws_ssh_channel *ch,
	   int task)
{
	pss->wt_tail = (pss->wt_tail - 1) & 7;
	pss->write_task[pss->wt_tail] = (uint8_t)task;
	pss->write_channel[pss->wt_tail] = ch;
	lws_callback_on_writable(pss->wsi);
}


void
lws_pad_set_length(struct per_session_data__sshd *pss, void *start, uint8_t **p,
		   struct lws_ssh_keys *keys)
{
	uint32_t len = (uint32_t)lws_ptr_diff(*p, start);
	uint8_t padc = 4, *bs = start;

	if (keys->full_length)
		len -= 4;

	if ((len + padc) & (uint32_t)(keys->padding_alignment - 1))
		padc = (uint8_t)((uint8_t)padc + (uint8_t)(keys->padding_alignment -
			((len + padc) & (uint32_t)(keys->padding_alignment - 1))));

	bs[4] = padc;
	len += padc;

	if (!keys->valid) /* no crypto = pad with 00 */
		while (padc--)
			*((*p)++) = 0;
	else { /* crypto active = pad with random */
		lws_get_random(pss->vhd->context, *p, padc);
		(*p) += padc;
	}
	if (keys->full_length)
		len += 4;

	lws_p32(start, len - 4);
}

static uint32_t
offer(struct per_session_data__sshd *pss, uint8_t *p, uint32_t len, int first,
      int *payload_len)
{
	uint8_t *op = p, *lp, *end = p + len - 1;
	int n, padc = 4, keylen;
	char keyt[32];
	uint8_t keybuf[256];

	keylen = (int)get_gen_server_key_25519(pss, keybuf, (int)sizeof(keybuf));
	if (!keylen) {
		lwsl_notice("get_gen_server_key failed\n");
		return 1;
	}
	lwsl_info("keylen %d\n", keylen);
	n = ed25519_key_parse(keybuf, (unsigned int)keylen,
			      keyt, sizeof(keyt), NULL, NULL);
	if (n) {
		lwsl_notice("unable to parse server key: %d\n", n);
		return 1;
	}

	/*
	 *     byte         SSH_MSG_KEXINIT
	 *     byte[16]     cookie (random bytes)
	 *     name-list    kex_algorithms
	 *     name-list    server_host_key_algorithms
	 *     name-list    encryption_algorithms_client_to_server
	 *     name-list    encryption_algorithms_server_to_client
	 *     name-list    mac_algorithms_client_to_server
	 *     name-list    mac_algorithms_server_to_client
	 *     name-list    compression_algorithms_client_to_server
	 *     name-list    compression_algorithms_server_to_client
	 *     name-list    langua->es_client_to_server
	 *     name-list    langua->es_server_to_client
	 *     boolean      first_kex_packet_follows
	 *     uint32       0 (reserved for future extension)
      	 */

	p += 5; /* msg len + padding */

	*p++ = SSH_MSG_KEXINIT;
	lws_get_random(pss->vhd->context, p, 16);
	p += 16;

	/* KEX algorithms */

	lp = p;
	p += 4;
	n = lws_snprintf((char *)p, lws_ptr_diff_size_t(end, p), "curve25519-sha256@libssh.org");
	p += lws_p32(lp, (uint32_t)n);

	/* Server Host Key Algorithms */

	lp = p;
	p += 4;
	n = lws_snprintf((char *)p, lws_ptr_diff_size_t(end, p), "%s", keyt);
	p += lws_p32(lp, (uint32_t)n);

	/* Encryption Algorithms: C -> S */

	lp = p;
	p += 4;
//	n = lws_snprintf((char *)p, end - p, "aes256-gcm@openssh.com");
	n = lws_snprintf((char *)p, lws_ptr_diff_size_t(end, p), "chacha20-poly1305@openssh.com");
	p += lws_p32(lp, (uint32_t)n);

	/* Encryption Algorithms: S -> C */

	lp = p;
	p += 4;
//	n = lws_snprintf((char *)p, end - p, "aes256-gcm@openssh.com");
	n = lws_snprintf((char *)p, lws_ptr_diff_size_t(end, p), "chacha20-poly1305@openssh.com");
	p += lws_p32(lp, (uint32_t)n);

	/* MAC Algorithms: C -> S */

	lp = p;
	p += 4;
	/* bogus: chacha20 does not use MACs, but 'none' is not offered */
	n = lws_snprintf((char *)p, lws_ptr_diff_size_t(end, p), "hmac-sha2-256");
	p += lws_p32(lp, (uint32_t)n);

	/* MAC Algorithms: S -> C */

	lp = p;
	p += 4;
	/* bogus: chacha20 does not use MACs, but 'none' is not offered */
	n = lws_snprintf((char *)p, lws_ptr_diff_size_t(end, p), "hmac-sha2-256");
	p += lws_p32(lp, (uint32_t)n);

	/* Compression Algorithms: C -> S */

	lp = p;
	p += 4;
	n = lws_snprintf((char *)p, lws_ptr_diff_size_t(end, p), "none");
	p += lws_p32(lp, (uint32_t)n);

	/* Compression Algorithms: S -> C */

	lp = p;
	p += 4;
	n = lws_snprintf((char *)p, lws_ptr_diff_size_t(end, p), "none");
	p += lws_p32(lp, (uint32_t)n);

	if (p - op < 13 + padc + 8)
		return 0;

	/* Languages: C -> S */

	*p++ = 0;
	*p++ = 0;
	*p++ = 0;
	*p++ = 0;

	/* Languages: S -> C */

	*p++ = 0;
	*p++ = 0;
	*p++ = 0;
	*p++ = 0;

	/* First KEX packet coming */

	*p++ = !!first;

	/* Reserved */

	*p++ = 0;
	*p++ = 0;
	*p++ = 0;
	*p++ = 0;

	len = (uint32_t)lws_ptr_diff(p, op);
	if (payload_len)
		/* starts at buf + 5 and excludes padding */
		*payload_len = (int)(len - 5);

	/* we must give at least 4 bytes of 00 padding */

	if (((int)len + padc) & 7)
		padc += 8 - (((int)len + padc) & 7);

	op[4] = (uint8_t)padc;
	len += (uint32_t)padc;

	while (padc--)
		*p++ = 0;

	/* recorded length does not include the uint32_t len itself */
	lws_p32(op, len - 4);

	return len;
}

static int
handle_name(struct per_session_data__sshd *pss)
{
	struct lws_kex *kex = pss->kex;
	char keyt[32];
	uint8_t keybuf[256];
	int n = 0, len;

	switch (pss->parser_state) {
	case SSH_KEX_NL_KEX_ALGS:
		if (!strcmp(pss->name, "curve25519-sha256@libssh.org"))
			kex->match_bitfield |= 1;
		break;
	case SSH_KEX_NL_SHK_ALGS:
		len = (int)get_gen_server_key_25519(pss, keybuf, (int)sizeof(keybuf));
		if (!len)
			break;
		if (ed25519_key_parse(keybuf, (unsigned int)len,
				      keyt, sizeof(keyt),
				      NULL, NULL)) {
			lwsl_err("Unable to parse host key %d\n", n);
		} else {
			if (!strcmp(pss->name, keyt)) {
				kex->match_bitfield |= 2;
				break;
			}
		}
		break;
	case SSH_KEX_NL_EACTS_ALGS:
		if (!strcmp(pss->name, "chacha20-poly1305@openssh.com"))
			kex->match_bitfield |= 4;
		break;
	case SSH_KEX_NL_EASTC_ALGS:
		if (!strcmp(pss->name, "chacha20-poly1305@openssh.com"))
			kex->match_bitfield |= 8;
		break;
	case SSH_KEX_NL_MACTS_ALGS:
		if (!strcmp(pss->name, "hmac-sha2-256"))
			kex->match_bitfield |= 16;
		break;
	case SSH_KEX_NL_MASTC_ALGS:
		if (!strcmp(pss->name, "hmac-sha2-256"))
			kex->match_bitfield |= 32;
		break;
	case SSH_KEX_NL_CACTS_ALGS:
		if (!strcmp(pss->name, "none"))
			kex->match_bitfield |= 64;
		break;
	case SSH_KEX_NL_CASTC_ALGS:
		if (!strcmp(pss->name, "none"))
			kex->match_bitfield |= 128;
		break;
	case SSH_KEX_NL_LCTS_ALGS:
	case SSH_KEX_NL_LSTC_ALGS:
		break;
	default:
		break;
	}

	return 0;
}


static int
lws_kex_create(struct per_session_data__sshd *pss)
{
	pss->kex = sshd_zalloc(sizeof(struct lws_kex));
	lwsl_info("%s\n", __func__);
	return !pss->kex;
}

static void
lws_kex_destroy(struct per_session_data__sshd *pss)
{
	if (!pss->kex)
		return;

	lwsl_info("Destroying KEX\n");

	if (pss->kex->I_C) {
		free(pss->kex->I_C);
		pss->kex->I_C = NULL;
	}
	if (pss->kex->I_S) {
		free(pss->kex->I_S);
		pss->kex->I_S = NULL;
	}

	lws_explicit_bzero(pss->kex, sizeof(*pss->kex));
	free(pss->kex);
	pss->kex = NULL;
}

static void
ssh_free(void *p)
{
	if (!p)
		return;

	lwsl_debug("%s: FREE %p\n", __func__, p);
	free(p);
}

#define ssh_free_set_NULL(x) if (x) { ssh_free(x); (x) = NULL; }

static void
lws_ua_destroy(struct per_session_data__sshd *pss)
{
	if (!pss->ua)
		return;

	lwsl_info("%s\n", __func__);

	if (pss->ua->username)
		ssh_free(pss->ua->username);
	if (pss->ua->service)
		ssh_free(pss->ua->service);
	if (pss->ua->alg)
		ssh_free(pss->ua->alg);
	if (pss->ua->pubkey)
		ssh_free(pss->ua->pubkey);
	if (pss->ua->sig) {
		lws_explicit_bzero(pss->ua->sig, pss->ua->sig_len);
		ssh_free(pss->ua->sig);
	}

	lws_explicit_bzero(pss->ua, sizeof(*pss->ua));
	free(pss->ua);
	pss->ua = NULL;
}


static int
rsa_hash_alg_from_ident(const char *ident)
{
	if (strcmp(ident, "ssh-rsa") == 0 ||
	    strcmp(ident, "ssh-rsa-cert-v01@openssh.com") == 0)
		return LWS_GENHASH_TYPE_SHA1;
	if (strcmp(ident, "rsa-sha2-256") == 0)
		return LWS_GENHASH_TYPE_SHA256;
	if (strcmp(ident, "rsa-sha2-512") == 0)
		return LWS_GENHASH_TYPE_SHA512;

        return -1;
}

static void
state_get_string_alloc(struct per_session_data__sshd *pss, int next)
{
	pss->parser_state = SSHS_GET_STRING_LEN_ALLOC;
        pss->state_after_string = (char)next;
}

static void
state_get_string(struct per_session_data__sshd *pss, int next)
{
	pss->parser_state = SSHS_GET_STRING_LEN;
        pss->state_after_string = (char)next;
}

static void
state_get_u32(struct per_session_data__sshd *pss, int next)
{
	pss->parser_state = SSHS_GET_U32;
        pss->state_after_string = (char)next;
}

static struct lws_ssh_channel *
ssh_get_server_ch(struct per_session_data__sshd *pss, uint32_t chi)
{
	struct lws_ssh_channel *ch = pss->ch_list;

	while (ch) {
		if (ch->server_ch == chi)
			return ch;
		ch = ch->next;
	}

	return NULL;
}

#if 0
static struct lws_ssh_channel *
ssh_get_peer_ch(struct per_session_data__sshd *pss, uint32_t chi)
{
	struct lws_ssh_channel *ch = pss->ch_list;

	while (ch) {
		if (ch->sender_ch == chi)
			return ch;
		ch = ch->next;
	}

	return NULL;
}
#endif

static void
ssh_destroy_channel(struct per_session_data__sshd *pss,
		    struct lws_ssh_channel *ch)
{
	lws_start_foreach_llp(struct lws_ssh_channel **, ppch, pss->ch_list) {
		if (*ppch == ch) {
			lwsl_info("Deleting ch %p\n", ch);
			if (pss->vhd && pss->vhd->ops &&
			    pss->vhd->ops->channel_destroy)
				pss->vhd->ops->channel_destroy(ch->priv);
			*ppch = ch->next;
			if (ch->sub)
				free(ch->sub);
			free(ch);

			return;
		}
	} lws_end_foreach_llp(ppch, next);

	lwsl_notice("Failed to delete ch\n");
}

static void
lws_ssh_exec_finish(void *finish_handle, int retcode)
{
	struct lws_ssh_channel *ch = (struct lws_ssh_channel *)finish_handle;
	struct per_session_data__sshd *pss = ch->pss;

	ch->retcode = retcode;
	write_task(pss, ch, SSH_WT_EXIT_STATUS);
	ch->scheduled_close = 1;
	write_task(pss, ch, SSH_WT_CH_CLOSE);
}

static int
lws_ssh_parse_plaintext(struct per_session_data__sshd *pss, uint8_t *p, size_t len)
{
	struct lws_gencrypto_keyelem e[LWS_GENCRYPTO_RSA_KEYEL_COUNT];
	struct lws_genrsa_ctx ctx;
	struct lws_ssh_channel *ch;
	struct lws_subprotocol_scp *scp;
	uint8_t *pp, *ps, hash[64];
#if !defined(MBEDTLS_VERSION_NUMBER) || MBEDTLS_VERSION_NUMBER < 0x03000000
	uint8_t *otmp = NULL;
#endif
	uint32_t m;
	int n;

	while (len --) {
again:
		switch(pss->parser_state) {
		case SSH_INITIALIZE_TRANSIENT:
			pss->parser_state = SSHS_IDSTRING;
			pss->ctr = 0;
			pss->copy_to_I_C = 0;

			/* fallthru */
		case SSHS_IDSTRING:
			if (*p == 0x0d) {
				pss->V_C[pss->npos] = '\0';
				pss->npos = 0;
				lwsl_info("peer id: %s\n", pss->V_C);
				p++;
				pss->parser_state = SSHS_IDSTRING_CR;
				break;
			}
			if (pss->npos < sizeof(pss->V_C) - 1)
				pss->V_C[pss->npos++] = (char)*p;
			p++;
			break;

		case SSHS_IDSTRING_CR:
			if (*p++ != 0x0a) {
				lwsl_notice("mangled id string\n");
				return 1;
			}
			pss->ssh_sequence_ctr_cts = 0;
			pss->parser_state = SSHS_MSG_LEN;
			break;

		case SSHS_MSG_LEN:
			pss->msg_len = (pss->msg_len << 8) | *p++;
			if (++pss->ctr != 4)
				break;

			if (pss->active_keys_cts.valid) {
				uint8_t b[4];

				POKE_U32(b, (uint32_t)pss->msg_len);
				pss->msg_len = lws_chachapoly_get_length(
					&pss->active_keys_cts,
					pss->ssh_sequence_ctr_cts, b);
			} else
				pss->ssh_sequence_ctr_cts++;

			lwsl_info("msg len %d\n", pss->msg_len);

			pss->parser_state = SSHS_MSG_PADDING;
			pss->ctr = 0;
			pss->pos = 4;
			if (pss->msg_len < 2 + 4) {
				lwsl_notice("illegal msg size\n");
				goto bail;
			}
			break;

		case SSHS_MSG_PADDING:
			pss->msg_padding = *p++;
			pss->parser_state = SSHS_MSG_ID;
			break;

		case SSHS_MSG_ID:
			pss->msg_id = *p++;
			pss->ctr = 0;
			switch (pss->msg_id) {
			case SSH_MSG_DISCONNECT:
				/*
				 *       byte      SSH_MSG_DISCONNECT
      	      	      	         *	 uint32    reason code
				 *	 string    description in ISO-10646
				 *	 	   UTF-8 encoding [RFC3629]
      	      	      	      	 *	 string    language tag [RFC3066]
				 */
				lwsl_notice("SSH_MSG_DISCONNECT\n");
				state_get_u32(pss, SSHS_NVC_DISCONNECT_REASON);
				break;
			case SSH_MSG_IGNORE:
				lwsl_notice("SSH_MSG_IGNORE\n");
				break;
			case SSH_MSG_UNIMPLEMENTED:
				lwsl_notice("SSH_MSG_UNIMPLEMENTED\n");
				break;
			case SSH_MSG_DEBUG:
				lwsl_notice("SSH_MSG_DEBUG\n");
				break;
			case SSH_MSG_SERVICE_REQUEST:
				lwsl_info("SSH_MSG_SERVICE_REQUEST\n");
				/* payload is a string */
				state_get_string(pss, SSHS_DO_SERVICE_REQUEST);
				break;
			case SSH_MSG_SERVICE_ACCEPT:
				lwsl_notice("SSH_MSG_ACCEPT\n");
				break;

			case SSH_MSG_KEXINIT:
				if (pss->kex_state !=
					    KEX_STATE_EXPECTING_CLIENT_OFFER) {
					pss->parser_state = SSH_KEX_STATE_SKIP;
					break;
				}
				if (!pss->kex) {
					lwsl_notice("%s: SSH_MSG_KEXINIT: NULL pss->kex\n", __func__);
					goto bail;
				}
				pss->parser_state = SSH_KEX_STATE_COOKIE;
				pss->kex->I_C_payload_len = 0;
				pss->kex->I_C_alloc_len = pss->msg_len;
				pss->kex->I_C = sshd_zalloc(pss->kex->I_C_alloc_len);
				if (!pss->kex->I_C) {
					lwsl_notice("OOM 3\n");
					goto bail;
				}
				pss->kex->I_C[pss->kex->I_C_payload_len++] =
					pss->msg_id;
				pss->copy_to_I_C = 1;
				break;
			case SSH_MSG_KEX_ECDH_INIT:
				pss->parser_state = SSH_KEX_STATE_ECDH_KEYLEN;
				break;

			case SSH_MSG_NEWKEYS:
				if (pss->kex_state !=
						KEX_STATE_REPLIED_TO_OFFER &&
				    pss->kex_state !=
				    		KEX_STATE_CRYPTO_INITIALIZED) {
					lwsl_notice("unexpected newkeys\n");

					goto bail;
				}
				/*
				 * it means we should now use the keys we
				 * agreed on
				 */
				lwsl_info("Activating CTS keys\n");
				pss->active_keys_cts = pss->kex->keys_next_cts;
				if (lws_chacha_activate(&pss->active_keys_cts))
					goto bail;

				pss->kex->newkeys |= 2;
				if (pss->kex->newkeys == 3)
					lws_kex_destroy(pss);

				if (pss->msg_padding) {
					pss->copy_to_I_C = 0;
					pss->parser_state =
						SSHS_MSG_EAT_PADDING;
					break;
				} else {
					pss->parser_state = SSHS_MSG_LEN;
				}
				break;

			case SSH_MSG_USERAUTH_REQUEST:
				/*
				 *    byte      SSH_MSG_USERAUTH_REQUEST
				 *    string    user name in UTF-8
				 *    		encoding [RFC3629]
				 *    string    service name in US-ASCII
				 *    string    "publickey"
				 *    boolean   FALSE
				 *    string    public key alg
				 *    string    public key blob
				 */
				lwsl_info("SSH_MSG_USERAUTH_REQUEST\n");
				if (pss->ua) {
					lwsl_notice("pss->ua overwrite\n");

					goto bail;
				}

				pss->ua = sshd_zalloc(sizeof(*pss->ua));
				if (!pss->ua)
					goto bail;

				state_get_string_alloc(pss, SSHS_DO_UAR_SVC);
				/* username is destroyed with userauth struct */
				if (!pss->sent_banner) {
					if (pss->vhd->ops->banner)
						write_task(pss, NULL,
							   SSH_WT_UA_BANNER);
					pss->sent_banner = 1;
				}
                                break;
			case SSH_MSG_USERAUTH_FAILURE:
				goto bail;
			case SSH_MSG_USERAUTH_SUCCESS:
				goto bail;
			case SSH_MSG_USERAUTH_BANNER:
				goto bail;

			case SSH_MSG_CHANNEL_OPEN:
				state_get_string(pss, SSHS_NVC_CHOPEN_TYPE);
				break;

			case SSH_MSG_CHANNEL_REQUEST:
				/* RFC4254
				 *
				 *  byte      SSH_MSG_CHANNEL_REQUEST
				 *  uint32    recipient channel
				 *  string    "pty-req"
				 *  boolean   want_reply
				 *  string    TERM environment variable value
				 *      		(e.g., vt100)
				 *  uint32    terminal width, characters
				 *      		(e.g., 80)
				 *  uint32    terminal height, rows (e.g., 24)
				 *  uint32    terminal width, px (e.g., 640)
				 *  uint32    terminal height, px (e.g., 480)
				 *  string    encoded terminal modes
				 */
				state_get_u32(pss, SSHS_NVC_CHRQ_RECIP);
				break;

			case SSH_MSG_CHANNEL_EOF:
				/* RFC4254
				 * When a party will no longer send more data
				 * to a channel, it SHOULD send
				 * SSH_MSG_CHANNEL_EOF.
				 *
				 *  byte      SSH_MSG_CHANNEL_EOF
 	 	 	 	 *  uint32    recipient channel
				 */
				state_get_u32(pss, SSHS_NVC_CH_EOF);
				break;

			case SSH_MSG_CHANNEL_CLOSE:
				/* RFC4254
				 *
				 *  byte      SSH_MSG_CHANNEL_CLOSE
				 *  uint32    recipient channel
				 *
				 * This message does not consume window space
				 * and can be sent even if no window space is
				 * available.
				 *
				 * It is RECOMMENDED that all data sent before
				 * this message be delivered to the actual
				 * destination, if possible.
				 */
				state_get_u32(pss, SSHS_NVC_CH_CLOSE);
				break;

			case SSH_MSG_CHANNEL_DATA:
				/* RFC4254
				 *
				 *      byte      SSH_MSG_CHANNEL_DATA
				 *      uint32    recipient channel
				 *      string    data
				 */
				state_get_u32(pss, SSHS_NVC_CD_RECIP);
				break;

			case SSH_MSG_CHANNEL_WINDOW_ADJUST:
				/* RFC452
				 *
				 * byte      SSH_MSG_CHANNEL_WINDOW_ADJUST
				 * uint32    recipient channel
				 * uint32    bytes to add
				 */
				if (!pss->ch_list)
					goto bail;

				state_get_u32(pss, SSHS_NVC_WA_RECIP);
				break;
			default:
				lwsl_notice("unk msg_id %d\n", pss->msg_id);

				goto bail;
			}
			break;

		case SSH_KEX_STATE_COOKIE:
			if (pss->msg_len < 16 + 1 + 1 + (10 * 4) + 5) {
				lwsl_notice("sanity: kex length failed\n");
				goto bail;
			}
			pss->kex->kex_cookie[pss->ctr++] = *p++;
			if (pss->ctr != sizeof(pss->kex->kex_cookie))
				break;
			pss->parser_state = SSH_KEX_NL_KEX_ALGS_LEN;
			pss->ctr = 0;
			break;
		case SSH_KEX_NL_KEX_ALGS_LEN:
		case SSH_KEX_NL_SHK_ALGS_LEN:
		case SSH_KEX_NL_EACTS_ALGS_LEN:
		case SSH_KEX_NL_EASTC_ALGS_LEN:
		case SSH_KEX_NL_MACTS_ALGS_LEN:
		case SSH_KEX_NL_MASTC_ALGS_LEN:
		case SSH_KEX_NL_CACTS_ALGS_LEN:
		case SSH_KEX_NL_CASTC_ALGS_LEN:
		case SSH_KEX_NL_LCTS_ALGS_LEN:
		case SSH_KEX_NL_LSTC_ALGS_LEN:
		case SSH_KEX_STATE_ECDH_KEYLEN:

			pss->len = (pss->len << 8) | *p++;
			if (++pss->ctr != 4)
				break;

			switch (pss->parser_state) {
			case SSH_KEX_STATE_ECDH_KEYLEN:
				pss->parser_state = SSH_KEX_STATE_ECDH_Q_C;
				break;
			default:
				pss->parser_state++;
				if (pss->len == 0)
					pss->parser_state++;
				break;
			}
			pss->ctr = 0;
			pss->npos = 0;
			if (pss->msg_len - pss->pos < pss->len) {
				lwsl_notice("sanity: length  %d - %d < %d\n",
					    pss->msg_len, pss->pos, pss->len);
				goto bail;
			}
			break;

		case SSH_KEX_NL_KEX_ALGS:
		case SSH_KEX_NL_SHK_ALGS:
		case SSH_KEX_NL_EACTS_ALGS:
		case SSH_KEX_NL_EASTC_ALGS:
		case SSH_KEX_NL_MACTS_ALGS:
		case SSH_KEX_NL_MASTC_ALGS:
		case SSH_KEX_NL_CACTS_ALGS:
		case SSH_KEX_NL_CASTC_ALGS:
		case SSH_KEX_NL_LCTS_ALGS:
		case SSH_KEX_NL_LSTC_ALGS:
			if (*p != ',') {
				if (pss->npos < sizeof(pss->name) - 1)
					pss->name[pss->npos++] = (char)*p;
			} else {
				pss->name[pss->npos] = '\0';
				pss->npos = 0;
				handle_name(pss);
			}
			p++;
			if (!--pss->len) {
				pss->name[pss->npos] = '\0';
				if (pss->npos)
					handle_name(pss);
				pss->parser_state++;
				break;
			}
			break;

		case SSH_KEX_FIRST_PKT:
			pss->first_coming = !!*p++;
			pss->parser_state = SSH_KEX_RESERVED;
			break;

		case SSH_KEX_RESERVED:
			pss->len = (pss->len << 8) | *p++;
			if (++pss->ctr != 4)
				break;
			pss->ctr = 0;
			if (pss->msg_padding) {
				pss->copy_to_I_C = 0;
				pss->parser_state = SSHS_MSG_EAT_PADDING;
				break;
			}
			pss->parser_state = SSHS_MSG_LEN;
			break;

		case SSH_KEX_STATE_ECDH_Q_C:
			if (pss->len != 32) {
				lwsl_notice("wrong key len\n");
				goto bail;
			}
			pss->kex->Q_C[pss->ctr++] = *p++;
			if (pss->ctr != 32)
				break;
			lwsl_info("Q_C parsed\n");
			pss->parser_state = SSHS_MSG_EAT_PADDING;
			break;

		case SSH_KEX_STATE_SKIP:
			if (pss->pos - 4 < pss->msg_len) {
				p++;
				break;
			}
			lwsl_debug("skip done pos %d, msg_len %d len=%ld, \n",
				       pss->pos, pss->msg_len, (long)len);
			pss->parser_state = SSHS_MSG_LEN;
			pss->ctr = 0;
			break;

		case SSHS_MSG_EAT_PADDING:
			p++;
			if (--pss->msg_padding)
				break;
			if (pss->msg_len + 4 != pss->pos) {
				lwsl_notice("sanity: kex end mismatch %d %d\n",
						pss->pos, pss->msg_len);
				goto bail;
			}

			switch (pss->msg_id) {
			case SSH_MSG_KEX_ECDH_INIT:
				if (pss->kex->match_bitfield != 0xff) {
					lwsl_notice("unable to negotiate\n");
					goto bail;
				}
				if (kex_ecdh(pss, pss->kex->kex_r,
					     &pss->kex->kex_r_len)) {
					lwsl_notice("hex_ecdh failed\n");
					goto bail;
				}
				write_task(pss, NULL, SSH_WT_OFFER_REPLY);
				break;
			}

			pss->parser_state = SSHS_MSG_LEN;
			pss->ctr = 0;
			break;

		case SSHS_GET_STRING_LEN:
			pss->npos = 0;
			pss->len = (pss->len << 8) | *p++;
                        if (++pss->ctr != 4)
                                break;
                        pss->ctr = 0;
			pss->parser_state = SSHS_GET_STRING;
			break;

		case SSHS_GET_STRING:
			if (pss->npos >= sizeof(pss->name) - 1) {
				lwsl_notice("non-alloc string too big\n");
				goto bail;
			}
			pss->name[pss->npos++] = (char)*p++;
			if (pss->npos != pss->len)
				break;

			pss->name[pss->npos] = '\0';
			pss->parser_state = pss->state_after_string;
			goto again;

		case SSHS_GET_STRING_LEN_ALLOC:
			pss->npos = 0;
			pss->len = (pss->len << 8) | *p++;
                        if (++pss->ctr != 4)
                                break;
                        pss->ctr = 0;
			pss->last_alloc = sshd_zalloc(pss->len + 1);
			lwsl_debug("SSHS_GET_STRING_LEN_ALLOC: %p, state %d\n",
				   pss->last_alloc, pss->state_after_string);
			if (!pss->last_alloc) {
				lwsl_notice("alloc string too big\n");
				goto bail;
			}
			pss->parser_state = SSHS_GET_STRING_ALLOC;
			break;

		case SSHS_GET_STRING_ALLOC:
			if (pss->npos >= pss->len)
				goto bail;
			pss->last_alloc[pss->npos++] = *p++;
			if (pss->npos != pss->len)
				break;
			pss->last_alloc[pss->npos] = '\0';
			pss->parser_state = pss->state_after_string;
			goto again;

		/*
		 * User Authentication
		 */

		case SSHS_DO_SERVICE_REQUEST:
			pss->okayed_userauth = 1;
			pss->parser_state = SSHS_MSG_EAT_PADDING;
			/*
			 * this only 'accepts' that we can negotiate auth for
			 * this service, not accepts the auth
			 */
			write_task(pss, NULL, SSH_WT_UA_ACCEPT);
			break;

		case SSHS_DO_UAR_SVC:
			pss->ua->username = (char *)pss->last_alloc;
			pss->last_alloc = NULL; /* it was adopted */
			state_get_string_alloc(pss, SSHS_DO_UAR_PUBLICKEY);
			/* destroyed with UA struct */
			break;

		case SSHS_DO_UAR_PUBLICKEY:
			pss->ua->service = (char *)pss->last_alloc;
			pss->last_alloc = NULL; /* it was adopted */

			/* Sect 5, RFC4252
			 *
			 * The 'user name' and 'service name' are repeated in
			 * every new authentication attempt, and MAY change.
			 *
			 * The server implementation MUST carefully check them
			 * in every message, and MUST flush any accumulated
			 * authentication states if they change.  If it is
			 * unable to flush an authentication state, it MUST
			 * disconnect if the 'user name' or 'service name'
			 * changes.
			 */

			if (pss->seen_auth_req_before && (
			     strcmp(pss->ua->username,
				    pss->last_auth_req_username) ||
			     strcmp(pss->ua->service,
				    pss->last_auth_req_service))) {
				lwsl_notice("username / svc changed\n");

				goto bail;
			}

			pss->seen_auth_req_before = 1;
			lws_strncpy(pss->last_auth_req_username,
				    pss->ua->username,
				    sizeof(pss->last_auth_req_username));
			lws_strncpy(pss->last_auth_req_service,
				    pss->ua->service,
				    sizeof(pss->last_auth_req_service));

			if (strcmp(pss->ua->service, "ssh-connection"))
				goto ua_fail;
			state_get_string(pss, SSHS_NVC_DO_UAR_CHECK_PUBLICKEY);
			break;

		case SSHS_NVC_DO_UAR_CHECK_PUBLICKEY:
			if (!strcmp(pss->name, "none")) {
				/* we must fail it */
				lwsl_info("got 'none' req, refusing\n");
				goto ua_fail;
			}

			if (strcmp(pss->name, "publickey")) {
				lwsl_notice("expected 'publickey' got '%s'\n",
					    pss->name);
				goto ua_fail;
			}
			pss->parser_state = SSHS_DO_UAR_SIG_PRESENT;
			break;

		case SSHS_DO_UAR_SIG_PRESENT:
			lwsl_info("SSHS_DO_UAR_SIG_PRESENT\n");
			pss->ua->sig_present = (char)*p++;
			state_get_string_alloc(pss, SSHS_NVC_DO_UAR_ALG);
			/* destroyed with UA struct */
			break;

		case SSHS_NVC_DO_UAR_ALG:
			pss->ua->alg = (char *)pss->last_alloc;
			pss->last_alloc = NULL; /* it was adopted */
			if (rsa_hash_alg_from_ident(pss->ua->alg) < 0) {
				lwsl_notice("unknown alg\n");
				goto ua_fail;
			}
			state_get_string_alloc(pss, SSHS_NVC_DO_UAR_PUBKEY_BLOB);
			/* destroyed with UA struct */
			break;

		case SSHS_NVC_DO_UAR_PUBKEY_BLOB:
			pss->ua->pubkey = pss->last_alloc;
			pss->last_alloc = NULL; /* it was adopted */
			pss->ua->pubkey_len = pss->npos;
			/*
			 * RFC4253
			 *
			 * ssh-rsa
			 *
			 * The structure inside the blob is
			 *
			 *   mpint e
			 *   mpint n
			 *
			 * Let's see if this key is authorized 
			 */
			
			n = 1;
			if (pss->vhd->ops && pss->vhd->ops->is_pubkey_authorized)
				n = pss->vhd->ops->is_pubkey_authorized(
					pss->ua->username, pss->ua->alg,
					pss->ua->pubkey, (int)pss->ua->pubkey_len);
			if (n) {
				lwsl_info("rejecting peer pubkey\n");
				goto ua_fail;
			}

			if (pss->ua->sig_present) {
				state_get_string_alloc(pss, SSHS_NVC_DO_UAR_SIG);
				/* destroyed with UA struct */
				break;
			}

			/*
			 * This key is at least one we would be prepared
			 * to accept if he really has it... since no sig
			 * client should resend everything with a sig
			 * appended.  OK it and delete this initial UA
			 */
			write_task(pss, NULL, SSH_WT_UA_PK_OK);
			pss->parser_state = SSHS_MSG_EAT_PADDING;
			break;

		case SSHS_NVC_DO_UAR_SIG:
			/*
			 * Now the pubkey is coming with a sig
			 */
			/* Sect 5.1 RFC4252
			 *
			 * SSH_MSG_USERAUTH_SUCCESS MUST be sent only once.
			 * When SSH_MSG_USERAUTH_SUCCESS has been sent, any
			 * further authentication requests received after that
			 * SHOULD be silently ignored.
			 */
			if (pss->ssh_auth_state == SSH_AUTH_STATE_GAVE_AUTH_IGNORE_REQS) {
				lwsl_info("Silently ignoring auth req after accepted\n");
				goto ua_fail_silently;
			}
			lwsl_info("SSHS_DO_UAR_SIG\n");
			pss->ua->sig = pss->last_alloc;
			pss->last_alloc = NULL; /* it was adopted */
			pss->ua->sig_len = pss->npos;
			pss->parser_state = SSHS_MSG_EAT_PADDING;

			/*
			 *   RFC 4252 p9
			 *
			 *   The value of 'signature' is a signature with
			 *   the private host key of the following data, in
			 *   this order:
			 *
			 *      string    session identifier
			 *      byte      SSH_MSG_USERAUTH_REQUEST
			 *      string    user name
			 *      string    service name
			 *      string    "publickey"
			 *      boolean   TRUE
			 *      string    public key algorithm name
			 *      string    public key to be used for auth
			 *
			 * We reproduce the signature plaintext and the
			 * hash, and then decrypt the incoming signed block.
			 * What comes out is some ASN1, in there is the
			 * hash decrypted.  We find it and confirm it
			 * matches the hash we computed ourselves.
			 *
			 * First step is generate the sig plaintext
			 */
			n = 4 + 32 +
			    1 +
			    4 + (int)strlen(pss->ua->username) +
			    4 + (int)strlen(pss->ua->service) +
			    4 + 9 +
			    1 +
			    4 + (int)strlen(pss->ua->alg) +
			    4 + (int)pss->ua->pubkey_len;

			ps = sshd_zalloc((unsigned int)n);
			if (!ps) {
				lwsl_notice("OOM 4\n");
				goto ua_fail;
			}

			pp = ps;
			lws_buf(&pp, pss->session_id, 32);
			*pp++ = SSH_MSG_USERAUTH_REQUEST;
			lws_cstr(&pp, pss->ua->username, 64);
			lws_cstr(&pp, pss->ua->service, 64);
			lws_cstr(&pp, "publickey", 64);
			*pp++ = 1;
			lws_cstr(&pp, pss->ua->alg, 64);
			lws_buf(&pp, pss->ua->pubkey, pss->ua->pubkey_len);

			/* Next hash the plaintext */

			if (lws_genhash_init(&pss->ua->hash_ctx,
				(enum lws_genhash_types)rsa_hash_alg_from_ident(pss->ua->alg))) {
				lwsl_notice("genhash init failed\n");
				free(ps);
				goto ua_fail;
			}

			if (lws_genhash_update(&pss->ua->hash_ctx, ps, lws_ptr_diff_size_t(pp, ps))) {
				lwsl_notice("genhash update failed\n");
				free(ps);
				goto ua_fail;
			}
			lws_genhash_destroy(&pss->ua->hash_ctx, hash);
			free(ps);

			/*
			 * Prepare the RSA decryption context: load in
			 * the E and N factors
			 */

			memset(e, 0, sizeof(e));
			pp = pss->ua->pubkey;
			m = lws_g32(&pp);
			pp += m;
			m = lws_g32(&pp);
			e[LWS_GENCRYPTO_RSA_KEYEL_E].buf = pp;
			e[LWS_GENCRYPTO_RSA_KEYEL_E].len = m;
			pp += m;
			m = lws_g32(&pp);
			e[LWS_GENCRYPTO_RSA_KEYEL_N].buf = pp;
			e[LWS_GENCRYPTO_RSA_KEYEL_N].len = m;

			if (lws_genrsa_create(&ctx, e, pss->vhd->context,
					      LGRSAM_PKCS1_1_5,
					      LWS_GENHASH_TYPE_UNKNOWN))
				goto ua_fail;
			/*
			 * point to the encrypted signature payload we
			 * were sent
			 */
			pp = pss->ua->sig;
			m = lws_g32(&pp);
			pp += m;
			m = lws_g32(&pp);
#if !defined(MBEDTLS_VERSION_NUMBER) || MBEDTLS_VERSION_NUMBER < 0x03000000

			/*
			 * decrypt it, resulting in an error, or some ASN1
			 * including the decrypted signature
			 */
			otmp = sshd_zalloc(m);
			if (!otmp)
				/* ua_fail1 frees bn_e, bn_n and rsa */
				goto ua_fail1;

			n = lws_genrsa_public_decrypt(&ctx, pp, m, otmp, m);
			if (n > 0) {
				/* the decrypted sig is in ASN1 format */
				m = 0;
				while ((int)m < n) {
					/* sig payload */
					if (otmp[m] == 0x04 &&
					    otmp[m + 1] == lws_genhash_size(
						  pss->ua->hash_ctx.type)) {
						m = (uint32_t)memcmp(&otmp[m + 2], hash,
							(unsigned int)lws_genhash_size(pss->ua->hash_ctx.type));
						break;
					}
					/* go into these */
					if (otmp[m] == 0x30) {
						m += 2;
						continue;
					}
					/* otherwise skip payloads */
					m += (uint32_t)(otmp[m + 1] + 2);
				}
			}

			free(otmp);
		#else
			ctx.ctx->MBEDTLS_PRIVATE(len) = m;
			n = lws_genrsa_hash_sig_verify(&ctx, hash,
				(enum lws_genhash_types)rsa_hash_alg_from_ident(pss->ua->alg),
				pp, m) == 0 ? 1 : 0;
		#endif
			lws_genrsa_destroy(&ctx);

			/*
			 * if no good, m is nonzero and inform peer
			 */
			if (n <= 0) {
				lwsl_notice("hash sig verify fail: %d\n", m);
				goto ua_fail;
			}

			/* if it checks out, inform peer */

			lwsl_info("sig check OK\n");

			/* Sect 5.1 RFC4252
			 *
			 * SSH_MSG_USERAUTH_SUCCESS MUST be sent only once.
			 * When SSH_MSG_USERAUTH_SUCCESS has been sent, any
			 * further authentication requests received after that
			 * SHOULD be silently ignored.
			 */
			pss->ssh_auth_state = SSH_AUTH_STATE_GAVE_AUTH_IGNORE_REQS;

			write_task(pss, NULL, SSH_WT_UA_SUCCESS);
			lws_ua_destroy(pss);
			break;

			/*
			 * Channels
			 */

		case SSHS_GET_U32:
			pss->len = (pss->len << 8) | *p++;
                        if (++pss->ctr != 4)
                                break;
                        pss->ctr = 0;
			pss->parser_state = pss->state_after_string;
			goto again;

			/*
			 * Channel: Disconnect
			 */

		case SSHS_NVC_DISCONNECT_REASON:
			pss->disconnect_reason = pss->len;
			state_get_string_alloc(pss, SSHS_NVC_DISCONNECT_DESC);
			break;

		case SSHS_NVC_DISCONNECT_DESC:
			pss->disconnect_desc = (char *)pss->last_alloc;
			pss->last_alloc = NULL; /* it was adopted */
			state_get_string(pss, SSHS_NVC_DISCONNECT_LANG);
			break;

		case SSHS_NVC_DISCONNECT_LANG:
			lwsl_notice("SSHS_NVC_DISCONNECT_LANG\n");
			if (pss->vhd->ops && pss->vhd->ops->disconnect_reason)
				pss->vhd->ops->disconnect_reason(
					pss->disconnect_reason,
					pss->disconnect_desc, pss->name);
			ssh_free_set_NULL(pss->last_alloc);
			break;

			/*
			 * Channel: Open
			 */

		case SSHS_NVC_CHOPEN_TYPE:
			/* channel open */
			if (strcmp(pss->name, "session")) {
				lwsl_notice("Failing on not session\n");
				pss->reason = 3;
				goto ch_fail;
			}
			lwsl_info("SSHS_NVC_CHOPEN_TYPE: creating session\n");
			pss->ch_temp = sshd_zalloc(sizeof(*pss->ch_temp));
			if (!pss->ch_temp)
				return -1;

			pss->ch_temp->type = SSH_CH_TYPE_SESSION;
			pss->ch_temp->pss = pss;
			state_get_u32(pss, SSHS_NVC_CHOPEN_SENDER_CH);
			break;

		case SSHS_NVC_CHOPEN_SENDER_CH:
			pss->ch_temp->sender_ch = pss->len;
			state_get_u32(pss, SSHS_NVC_CHOPEN_WINSIZE);
			break;
		case SSHS_NVC_CHOPEN_WINSIZE:
			lwsl_info("Initial window set to %d\n", pss->len);
			pss->ch_temp->window = (int32_t)pss->len;
			state_get_u32(pss, SSHS_NVC_CHOPEN_PKTSIZE);
			break;
		case SSHS_NVC_CHOPEN_PKTSIZE:
			pss->ch_temp->max_pkt = pss->len;
			pss->ch_temp->peer_window_est = LWS_SSH_INITIAL_WINDOW;
			pss->ch_temp->server_ch = (uint32_t)pss->next_ch_num++;
			/*
			 * add us to channel list... leave as ch_temp
			 * as write task needs it and will NULL down
			 */
			lwsl_info("creating new session ch\n");
			pss->ch_temp->next = pss->ch_list;
			pss->ch_list = pss->ch_temp;
			if (pss->vhd->ops && pss->vhd->ops->channel_create)
				pss->vhd->ops->channel_create(pss->wsi,
						&pss->ch_temp->priv);
			write_task(pss, pss->ch_temp, SSH_WT_CH_OPEN_CONF);
			pss->parser_state = SSHS_MSG_EAT_PADDING;
			break;

		/*
		 * SSH_MSG_CHANNEL_REQUEST
		 */

		case SSHS_NVC_CHRQ_RECIP:
			pss->ch_recip = pss->len;
			state_get_string(pss, SSHS_NVC_CHRQ_TYPE);
			break;

		case SSHS_NVC_CHRQ_TYPE:
			pss->parser_state = SSHS_CHRQ_WANT_REPLY;
			break;

		case SSHS_CHRQ_WANT_REPLY:
			pss->rq_want_reply = *p++;
			lwsl_info("SSHS_CHRQ_WANT_REPLY: %s, wantrep: %d\n",
					pss->name, pss->rq_want_reply);

			pss->ch_temp = ssh_get_server_ch(pss, pss->ch_recip);

			/* after this they differ by the request */

			/*
			 * a PTY for a shell
			 */
			if (!strcmp(pss->name, "pty-req")) {
				state_get_string(pss, SSHS_NVC_CHRQ_TERM);
				break;
			}
			/*
			 * a shell
			 */
			if (!strcmp(pss->name, "shell")) {
				pss->channel_doing_spawn = pss->ch_temp->server_ch;
				if (pss->vhd->ops && pss->vhd->ops->shell &&
				    !pss->vhd->ops->shell(pss->ch_temp->priv,
						          pss->wsi,
						 lws_ssh_exec_finish, pss->ch_temp)) {

					if (pss->rq_want_reply)
						write_task_insert(pss, pss->ch_temp,
							   SSH_WT_CHRQ_SUCC);
					pss->parser_state = SSHS_MSG_EAT_PADDING;
					break;
				}

				goto chrq_fail;
			}
			/*
			 * env vars to be set in the shell
			 */
			if (!strcmp(pss->name, "env")) {
				state_get_string(pss, SSHS_NVC_CHRQ_ENV_NAME);
				break;
			}

			/*
			 * exec something
			 */
			if (!strcmp(pss->name, "exec")) {
				state_get_string_alloc(pss, SSHS_NVC_CHRQ_EXEC_CMD);
				break;
			}

			/*
			 * spawn a subsystem
			 */
			if (!strcmp(pss->name, "subsystem")) {
				lwsl_notice("subsystem\n");
				state_get_string_alloc(pss,
						       SSHS_NVC_CHRQ_SUBSYSTEM);
				break;
			}
			if (!strcmp(pss->name, "window-change")) {
				lwsl_info("%s: window-change\n", __func__);
				state_get_u32(pss,
					      SSHS_NVC_CHRQ_WNDCHANGE_TW);
				break;
			}

			if (pss->rq_want_reply)
				goto chrq_fail;

			pss->parser_state = SSH_KEX_STATE_SKIP;
			break;

		/* CHRQ pty-req */

		case SSHS_NVC_CHRQ_TERM:
			memcpy(pss->args.pty.term, pss->name,
				sizeof(pss->args.pty.term) - 1);
			state_get_u32(pss, SSHS_NVC_CHRQ_TW);
			break;
		case SSHS_NVC_CHRQ_TW:
			pss->args.pty.width_ch = pss->len;
			state_get_u32(pss, SSHS_NVC_CHRQ_TH);
			break;
		case SSHS_NVC_CHRQ_TH:
			pss->args.pty.height_ch = pss->len;
			state_get_u32(pss, SSHS_NVC_CHRQ_TWP);
			break;
		case SSHS_NVC_CHRQ_TWP:
			pss->args.pty.width_px = pss->len;
			state_get_u32(pss, SSHS_NVC_CHRQ_THP);
			break;
		case SSHS_NVC_CHRQ_THP:
			pss->args.pty.height_px = pss->len;
			state_get_string_alloc(pss, SSHS_NVC_CHRQ_MODES);
			break;
		case SSHS_NVC_CHRQ_MODES:
			/* modes is a stream of byte-pairs, not a string */
			pss->args.pty.modes = (char *)pss->last_alloc;
			pss->last_alloc = NULL; /* it was adopted */
			pss->args.pty.modes_len = pss->npos;
			n = 0;
			if (pss->vhd->ops && pss->vhd->ops->pty_req)
				n = pss->vhd->ops->pty_req(pss->ch_temp->priv,
							&pss->args.pty);
			ssh_free_set_NULL(pss->args.pty.modes);
			if (n)
				goto chrq_fail;
			if (pss->rq_want_reply)
				write_task(pss, pss->ch_temp, SSH_WT_CHRQ_SUCC);
			pss->parser_state = SSHS_MSG_EAT_PADDING;
			break;

		/* CHRQ env */

		case SSHS_NVC_CHRQ_ENV_NAME:
			strcpy(pss->args.aux, pss->name);
			state_get_string(pss, SSHS_NVC_CHRQ_ENV_VALUE);
			break;

		case SSHS_NVC_CHRQ_ENV_VALUE:
			if (pss->vhd->ops && pss->vhd->ops->set_env)
				if (pss->vhd->ops->set_env(pss->ch_temp->priv,
						pss->args.aux, pss->name))
					goto chrq_fail;
			pss->parser_state = SSHS_MSG_EAT_PADDING;
			break;

		/* CHRQ exec */

		case SSHS_NVC_CHRQ_EXEC_CMD:
			/*
			 * byte      SSH_MSG_CHANNEL_REQUEST
			 * uint32    recipient channel
			 * string    "exec"
			 * boolean   want reply
			 * string    command
			 *
			 * This message will request that the server start the
			 * execution of the given command.  The 'command' string
			 * may contain a path.  Normal precautions MUST be taken
			 * to prevent the execution of unauthorized commands.
			 *
			 * scp sends "scp -t /path/..."
			 */
			lwsl_info("exec cmd: %s %02X\n", pss->last_alloc, *p);

			pss->channel_doing_spawn = pss->ch_temp->server_ch;

			if (pss->vhd->ops && pss->vhd->ops->exec &&
			    !pss->vhd->ops->exec(pss->ch_temp->priv, pss->wsi,
					    	 (const char *)pss->last_alloc,
						 lws_ssh_exec_finish, pss->ch_temp)) {
				ssh_free_set_NULL(pss->last_alloc);
				if (pss->rq_want_reply)
					write_task_insert(pss, pss->ch_temp,
						   SSH_WT_CHRQ_SUCC);

				pss->parser_state = SSHS_MSG_EAT_PADDING;
				break;
			}

			/*
			 * even if he doesn't want to exec it, we know how to
			 * fake scp
			 */

			/* we only alloc "exec" of scp for scp destination */
			n = 1;
			if (pss->last_alloc[0] != 's' ||
			    pss->last_alloc[1] != 'c' ||
			    pss->last_alloc[2] != 'p' ||
			    pss->last_alloc[3] != ' ')
				/* disallow it */
				n = 0;

			ssh_free_set_NULL(pss->last_alloc);
			if (!n)
				goto chrq_fail;

			/* our channel speaks SCP protocol now */

			scp = sshd_zalloc(sizeof(*scp));
			if (!scp)
				return -1;

			pss->ch_temp->type = SSH_CH_TYPE_SCP;
			pss->ch_temp->sub = (lws_subprotocol *)scp;

			scp->ips = SSHS_SCP_COLLECTSTR;

			if (pss->rq_want_reply)
				write_task(pss, pss->ch_temp, SSH_WT_CHRQ_SUCC);

			/* we start the scp protocol first by sending an ACK */
			write_task(pss, pss->ch_temp, SSH_WT_SCP_ACK_OKAY);

			pss->parser_state = SSHS_MSG_EAT_PADDING;
			break;

		case SSHS_NVC_CHRQ_SUBSYSTEM:
			lwsl_notice("subsystem: %s", pss->last_alloc);
			n = 0;
#if 0
			if (!strcmp(pss->name, "sftp")) {
				lwsl_notice("SFTP session\n");
				pss->ch_temp->type = SSH_CH_TYPE_SFTP;
				n = 1;
			}
#endif
			ssh_free_set_NULL(pss->last_alloc);
//			if (!n)
				goto ch_fail;
#if 0
			if (pss->rq_want_reply)
				write_task(pss, ssh_get_server_ch(pss,
					pss->ch_recip), SSH_WT_CHRQ_SUCC);
			pss->parser_state = SSHS_MSG_EAT_PADDING;
			break;
#endif

		/* CHRQ window-change */

	case SSHS_NVC_CHRQ_WNDCHANGE_TW:
		pss->args.pty.width_ch = pss->len;
		state_get_u32(pss, SSHS_NVC_CHRQ_WNDCHANGE_TH);
		break;
        case SSHS_NVC_CHRQ_WNDCHANGE_TH:
		pss->args.pty.height_ch = pss->len;
		state_get_u32(pss, SSHS_NVC_CHRQ_WNDCHANGE_TWP);
		break;
        case SSHS_NVC_CHRQ_WNDCHANGE_TWP:
		pss->args.pty.width_px = pss->len;
		state_get_u32(pss, SSHS_NVC_CHRQ_WNDCHANGE_THP);
		break;
        case SSHS_NVC_CHRQ_WNDCHANGE_THP:
		pss->args.pty.height_px = pss->len;
		pss->args.pty.term[0] = 0;
		pss->args.pty.modes = NULL;
		pss->args.pty.modes_len = 0;
		n = 0;
		if (pss->vhd->ops && pss->vhd->ops->pty_req)
			n = pss->vhd->ops->pty_req(pss->ch_temp->priv,
						   &pss->args.pty);
		if (n)
			goto chrq_fail;
		pss->parser_state = SSHS_MSG_EAT_PADDING;
		break;

		/* SSH_MSG_CHANNEL_DATA */

		case SSHS_NVC_CD_RECIP:
			pss->ch_recip = pss->len;

			ch = ssh_get_server_ch(pss, pss->ch_recip);
			ch->peer_window_est -= (int32_t)pss->msg_len;

			if (pss->msg_len < sizeof(pss->name))
				state_get_string(pss, SSHS_NVC_CD_DATA);
			else
				state_get_string_alloc(pss,
					SSHS_NVC_CD_DATA_ALLOC);
			break;

		case SSHS_NVC_CD_DATA_ALLOC:
		case SSHS_NVC_CD_DATA:
			/*
			 * Actual protocol incoming payload
			 */
			if (pss->parser_state == SSHS_NVC_CD_DATA_ALLOC)
				pp = pss->last_alloc;
			else
				pp = (uint8_t *)pss->name;
			lwsl_info("SSHS_NVC_CD_DATA\n");

			ch = ssh_get_server_ch(pss, pss->ch_recip);
			switch (ch->type) {
			case SSH_CH_TYPE_SCP:
				scp = &ch->sub->scp;
				switch (scp->ips) {
				case SSHS_SCP_COLLECTSTR:
					/* gather the ascii-coded headers */
					for (n = 0; n < (int)pss->npos; n++)
						lwsl_notice("0x%02X %c\n",
							    pp[n], pp[n]);

					/* Header triggers the transfer? */
					if (pp[0] == 'C' && pp[pss->npos - 1] == '\x0a') {
						while (*pp != ' ' && *pp != '\x0a')
							pp++;
						if (*pp++ != ' ') {
							write_task(pss, ch,
							   SSH_WT_SCP_ACK_ERROR);
							pss->parser_state = SSHS_MSG_EAT_PADDING;
							break;
						}
						scp->len = (uint64_t)atoll((const char *)pp);
						lwsl_notice("scp payload %llu expected\n",
							    (unsigned long long)scp->len);
						scp->ips = SSHS_SCP_PAYLOADIN;
					}
					/* ack it */
					write_task(pss, pss->ch_temp,
						   SSH_WT_SCP_ACK_OKAY);
					break;
				case SSHS_SCP_PAYLOADIN:
					/* the scp file payload */
					if (pss->vhd->ops)
						pss->vhd->ops->rx(ch->priv,
							pss->wsi, pp, pss->npos);
					if (scp->len >= pss->npos)
						scp->len -= pss->npos;
					else
						scp->len = 0;
					if (!scp->len) {
						lwsl_notice("scp txfer completed\n");
						scp->ips = SSHS_SCP_COLLECTSTR;
						break;
					}
					break;
				}
				break;
			default: /* scp payload */
				if (pss->vhd->ops)
					pss->vhd->ops->rx(ch->priv, pss->wsi,
							  pp, pss->npos);
				break;
			}
			if (pss->parser_state == SSHS_NVC_CD_DATA_ALLOC)
				ssh_free_set_NULL(pss->last_alloc);

			if (ch->peer_window_est < 32768) {
				write_task(pss, ch, SSH_WT_WINDOW_ADJUST);
				ch->peer_window_est += 32768;
				lwsl_info("extra peer WINDOW_ADJUST (~ %d)\n",
					    ch->peer_window_est);
			}

			pss->parser_state = SSHS_MSG_EAT_PADDING;
			break;

		case SSHS_NVC_WA_RECIP:
			pss->ch_recip = pss->len;
			state_get_u32(pss, SSHS_NVC_WA_ADD);
			break;

		case SSHS_NVC_WA_ADD:
			ch = ssh_get_server_ch(pss, pss->ch_recip);
			if (ch) {
				ch->window += (int32_t)pss->len;
				lwsl_notice("got additional window %d (now %d)\n",
						pss->len, ch->window);
			}
			pss->parser_state = SSHS_MSG_EAT_PADDING;
			break;

			/*
			 *  channel close
			 */

		case SSHS_NVC_CH_EOF:
			/*
			 * No explicit response is sent to this
			 * message.  However, the application may send
			 * EOF to whatever is at the other end of the
			 * channel.  Note that the channel remains open
			 * after this message, and more data may still
			 * be sent in the other direction.  This message
			 * does not consume window space and can be sent
			 * even if no window space is available.
			 */
			lwsl_notice("SSH_MSG_CHANNEL_EOF: %d\n", pss->ch_recip);
			ch = ssh_get_server_ch(pss, pss->ch_recip);
			if (!ch) {
				lwsl_notice("unknown ch %d\n", pss->ch_recip);
				return -1;
			}

			if (!ch->scheduled_close) {
				lwsl_notice("scheduling CLOSE\n");
				ch->scheduled_close = 1;
				write_task(pss, ch, SSH_WT_CH_CLOSE);
			}
			pss->parser_state = SSHS_MSG_EAT_PADDING;
			break;

		case SSHS_NVC_CH_CLOSE:
			/*
			 * When either party wishes to terminate the
			 * channel, it sends SSH_MSG_CHANNEL_CLOSE.
			 * Upon receiving this message, a party MUST
			 * send back an SSH_MSG_CHANNEL_CLOSE unless it
			 * has already sent this message for the
			 * channel.  The channel is considered closed
			 * for a party when it has both sent and
			 * received SSH_MSG_CHANNEL_CLOSE, and the
			 * party may then reuse the channel number.
			 * A party MAY send SSH_MSG_CHANNEL_CLOSE
			 * without having sent or received
			 * SSH_MSG_CHANNEL_EOF.
			 */
			lwsl_notice("SSH_MSG_CHANNEL_CLOSE ch %d\n",
				    pss->ch_recip);
			ch = ssh_get_server_ch(pss, pss->ch_recip);
			if (!ch)
				goto bail;

			pss->parser_state = SSHS_MSG_EAT_PADDING;

			if (ch->sent_close) {
				/*
				 * This is acking our sent close...
				 * we can destroy the channel with no
				 * further communication.
				 */
				ssh_destroy_channel(pss, ch);
				break;
			}

			ch->received_close = 1;
			ch->scheduled_close = 1;
			write_task(pss, ch, SSH_WT_CH_CLOSE);
			break;

		default:
			break;

chrq_fail:
			lwsl_notice("chrq_fail\n");
			write_task(pss, pss->ch_temp, SSH_WT_CHRQ_FAILURE);
			pss->parser_state = SSH_KEX_STATE_SKIP;
			break;

ch_fail:
			if (pss->ch_temp) {
				free(pss->ch_temp);
				pss->ch_temp = NULL;
			}
			write_task(pss, pss->ch_temp, SSH_WT_CH_FAILURE);
			pss->parser_state = SSH_KEX_STATE_SKIP;
			break;

#if !defined(MBEDTLS_VERSION_NUMBER) || MBEDTLS_VERSION_NUMBER < 0x03000000
ua_fail1:
#endif
			lws_genrsa_destroy(&ctx);
ua_fail:
			write_task(pss, NULL, SSH_WT_UA_FAILURE);
ua_fail_silently:
			lws_ua_destroy(pss);
			/* Sect 4, RFC4252
			 *
			 * Additionally, the implementation SHOULD limit the
			 * number of failed authentication attempts a client
			 * may perform in a single session (the RECOMMENDED
			 * limit is 20 attempts).  If the threshold is
			 * exceeded, the server SHOULD disconnect.
			 */
			if (pss->count_auth_attempts++ > 20)
				goto bail;

			pss->parser_state = SSH_KEX_STATE_SKIP;
			break;
		}

		pss->pos++;
	}

	return 0;	
bail:
	lws_kex_destroy(pss);
	lws_ua_destroy(pss);

	return SSH_DISCONNECT_KEY_EXCHANGE_FAILED;
}

static int
parse(struct per_session_data__sshd *pss, uint8_t *p, size_t len)
{
	while (len--) {

		if (pss->copy_to_I_C && pss->kex->I_C_payload_len <
				pss->kex->I_C_alloc_len &&
				pss->parser_state != SSHS_MSG_EAT_PADDING)
			pss->kex->I_C[pss->kex->I_C_payload_len++] = *p;

		if (pss->active_keys_cts.valid &&
		    pss->parser_state == SSHS_MSG_LEN)
			/* take a copy for full decrypt */
			pss->packet_assembly[pss->pa_pos++] = *p;

		if (pss->active_keys_cts.valid &&
		    pss->parser_state == SSHS_MSG_PADDING &&
		    pss->msg_len) {
			/* we are going to have to decrypt it */
			uint32_t cp, l = pss->msg_len + 4 +
				pss->active_keys_cts.MAC_length;
			uint8_t pt[2048];

			len++;
			cp = (uint32_t)len;

			if (cp > l - pss->pa_pos)
				cp = l - pss->pa_pos;

			if (cp > sizeof(pss->packet_assembly) -
					pss->pa_pos) {
				lwsl_err("Packet is too big to decrypt\n");

				goto bail;
			}
			if (pss->msg_len < 2 + 4) {
				lwsl_err("packet too small\n");

				goto bail;
			}

			memcpy(&pss->packet_assembly[pss->pa_pos], p, cp);
			pss->pa_pos += cp;
			len -= cp;
			p += cp;

			if (pss->pa_pos != l)
				return 0;

			/* decrypt it */
			cp = (uint32_t)lws_chacha_decrypt(&pss->active_keys_cts,
					        pss->ssh_sequence_ctr_cts++,
					        pss->packet_assembly,
					        pss->pa_pos, pt);
			if (cp) {
				lwsl_notice("Decryption failed: %d\n", cp);
				goto bail;
			}

			if (lws_ssh_parse_plaintext(pss, pt + 4, pss->msg_len))
				goto bail;

			pss->pa_pos = 0;
			pss->ctr = 0;
			continue;
		}

		if (lws_ssh_parse_plaintext(pss, p, 1))
			goto bail;

		p++;
	}

	return 0;

bail:
	lws_kex_destroy(pss);
	lws_ua_destroy(pss);

	return SSH_DISCONNECT_KEY_EXCHANGE_FAILED;
}

static uint32_t
pad_and_encrypt(uint8_t *dest, void *ps, uint8_t *pp,
		struct per_session_data__sshd *pss, int skip_pad)
{
	uint32_t n;

	if (!skip_pad)
		lws_pad_set_length(pss, ps, &pp, &pss->active_keys_stc);
	n = (uint32_t)lws_ptr_diff(pp, ps);

	if (!pss->active_keys_stc.valid) {
		memcpy(dest, ps, n);
		return n;
	}

	lws_chacha_encrypt(&pss->active_keys_stc, pss->ssh_sequence_ctr_stc,
			   ps, n, dest);
	n += pss->active_keys_stc.MAC_length;

	return n;
}

static int
lws_callback_raw_sshd(struct lws *wsi, enum lws_callback_reasons reason,
		      void *user, void *in, size_t len)
{
	struct per_session_data__sshd *pss =
			(struct per_session_data__sshd *)user, **p;
	struct per_vhost_data__sshd *vhd = NULL;
	uint8_t buf[LWS_PRE + 1024], *pp, *ps = &buf[LWS_PRE + 512], *ps1 = NULL;
	const struct lws_protocol_vhost_options *pvo;
	const struct lws_protocols *prot;
	struct lws_ssh_channel *ch;
	char lang[10] = "";
	int n, m, o;

	/*
	 * Because we are an abstract protocol plugin, we will get called by
	 * wsi that actually bind to a plugin "on top of us" that calls thru
	 * to our callback.
	 *
	 * Under those circumstances, we can't simply get a pointer to our own
	 * protocol from the wsi.  If there's a pss already, we can get it from
	 * there, but the first time for each connection we have to look it up.
	 */
	if (pss && pss->vhd)
		vhd = (struct per_vhost_data__sshd *)
			lws_protocol_vh_priv_get(lws_get_vhost(wsi),
				pss->vhd->protocol);
	else
		if (lws_get_vhost(wsi))
			vhd = (struct per_vhost_data__sshd *)
				lws_protocol_vh_priv_get(lws_get_vhost(wsi),
				lws_vhost_name_to_protocol(
					lws_get_vhost(wsi), "lws-ssh-base"));

	switch ((int)reason) {
	case LWS_CALLBACK_PROTOCOL_INIT:
		vhd = lws_protocol_vh_priv_zalloc(lws_get_vhost(wsi),
						  lws_get_protocol(wsi),
						  sizeof(struct per_vhost_data__sshd));
		if (!vhd)
			return 0;
		vhd->context = lws_get_context(wsi);
		vhd->protocol = lws_get_protocol(wsi);
		vhd->vhost = lws_get_vhost(wsi);

		pvo = (const struct lws_protocol_vhost_options *)in;
		while (pvo) {
			/*
			 * the user code passes the ops struct address to us
			 * using a pvo (per-vhost option)
			 */
			if (!strcmp(pvo->name, "ops"))
				vhd->ops = (const struct lws_ssh_ops *)pvo->value;

			/*
			 * the user code is telling us to get the ops struct
			 * from another protocol's protocol.user pointer
			 */
			if (!strcmp(pvo->name, "ops-from")) {
				prot = lws_vhost_name_to_protocol(vhd->vhost,
								  pvo->value);
				if (prot)
					vhd->ops = (const struct lws_ssh_ops *)prot->user;
				else
					lwsl_err("%s: can't find protocol %s\n",
						    __func__, pvo->value);
			}

			pvo = pvo->next;
		}

		if (!vhd->ops) {
			lwsl_warn("ssh pvo \"ops\" is mandatory\n");
			return 0;
		}
		/*
		 * The user code ops api_version has to be current
		 */
		if (vhd->ops->api_version != LWS_SSH_OPS_VERSION) {
			lwsl_err("FATAL ops is api_version v%d but code is v%d\n",
				vhd->ops->api_version, LWS_SSH_OPS_VERSION);
			return 1;
		}
		break;

        case LWS_CALLBACK_RAW_ADOPT:
		lwsl_info("LWS_CALLBACK_RAW_ADOPT\n");
		if (!vhd || !pss)
			return -1;
		pss->next = vhd->live_pss_list;
		vhd->live_pss_list = pss;
		pss->parser_state = SSH_INITIALIZE_TRANSIENT;
		pss->wsi = wsi;
		pss->vhd = vhd;
		pss->kex_state = KEX_STATE_EXPECTING_CLIENT_OFFER;
		pss->active_keys_cts.padding_alignment = 8;
		pss->active_keys_stc.padding_alignment = 8;
		if (lws_kex_create(pss))
			return -1;
		write_task(pss, NULL, SSH_WT_VERSION);

		/* sect 4  RFC4252
		 *
		 * The server SHOULD have a timeout for authentication and
		 * disconnect if the authentication has not been accepted
		 * within the timeout period.
		 *
		 * The RECOMMENDED timeout period is 10 minutes.
		 */
		lws_set_timeout(wsi, (enum pending_timeout)
		       SSH_PENDING_TIMEOUT_CONNECT_TO_SUCCESSFUL_AUTH, 10 * 60);
                break;

	case LWS_CALLBACK_RAW_CLOSE:
		if (!pss)
			return -1;
		lwsl_info("LWS_CALLBACK_RAW_CLOSE\n");
		lws_kex_destroy(pss);
		lws_ua_destroy(pss);

		ssh_free_set_NULL(pss->last_alloc);

		while (pss->ch_list)
			ssh_destroy_channel(pss, pss->ch_list);

		lws_chacha_destroy(&pss->active_keys_cts);
		lws_chacha_destroy(&pss->active_keys_stc);

		p = &vhd->live_pss_list;

		while (*p) {
			if ((*p) == pss) {
				*p = pss->next;
				continue;
			}
			p = &((*p)->next);
		}
		break;

	case LWS_CALLBACK_RAW_RX:
		if (!pss)
			return -1;
		if (parse(pss, in, len))
			return -1;
		break;

	case LWS_CALLBACK_RAW_WRITEABLE:
		if (!pss)
			break;
		n = 0;
		o = pss->write_task[pss->wt_tail];
		ch = pss->write_channel[pss->wt_tail];

		if (pss->wt_head == pss->wt_tail)
			o = SSH_WT_NONE;

		switch (o) {
		case SSH_WT_VERSION:
			if (!pss->vhd)
				break;
			n = lws_snprintf((char *)buf + LWS_PRE,
					 sizeof(buf) - LWS_PRE - 1, "%s\r\n",
					 pss->vhd->ops->server_string);
			write_task(pss, NULL, SSH_WT_OFFER);
			break;

		case SSH_WT_OFFER:
			if (!pss->vhd)
				break;
			m = 0;
			n = (int)offer(pss, buf + LWS_PRE,
				  sizeof(buf) - LWS_PRE, 0, &m);
			if (n == 0) {
				lwsl_notice("Too small\n");

				return -1;
			}

			if (!pss->kex) {
				lwsl_notice("%s: SSH_WT_OFFER: pss->kex is NULL\n",
					    __func__);
				return -1;
			}

			/* we need a copy of it to generate the hash later */
			if (pss->kex->I_S)
				free(pss->kex->I_S);
			pss->kex->I_S = sshd_zalloc((unsigned int)m);
			if (!pss->kex->I_S) {
				lwsl_notice("OOM 5: %d\n", m);

				return -1;
			}
			/* without length + padcount part */
			memcpy(pss->kex->I_S, buf + LWS_PRE + 5, (unsigned int)m);
			pss->kex->I_S_payload_len = (uint32_t)m; /* without padding */
			break;

		case SSH_WT_OFFER_REPLY:
			memcpy(ps, pss->kex->kex_r, pss->kex->kex_r_len);
			n = (int)pad_and_encrypt(&buf[LWS_PRE], ps,
					    ps + pss->kex->kex_r_len, pss, 1);
			pss->kex_state = KEX_STATE_REPLIED_TO_OFFER;
			/* afterwards, must do newkeys */
			write_task(pss, NULL, SSH_WT_SEND_NEWKEYS);
			break;

		case SSH_WT_SEND_NEWKEYS:
			pp = ps + 5;
			*pp++ = SSH_MSG_NEWKEYS;
			goto pac;

		case SSH_WT_UA_ACCEPT:
			/*
			 *  If the server supports the service (and permits
			 *  the client to use it), it MUST respond with the
			 *  following:
			 *
			 *      byte      SSH_MSG_SERVICE_ACCEPT
			 *      string    service name
			 */
			pp = ps + 5;
			*pp++ = SSH_MSG_SERVICE_ACCEPT;
			lws_p32(pp, pss->npos);
			pp += 4;
			strcpy((char *)pp, pss->name);
			pp += pss->npos;
			goto pac;

		case SSH_WT_UA_FAILURE:
			pp = ps + 5;
			*pp++ = SSH_MSG_USERAUTH_FAILURE;
			lws_p32(pp, 9);
			pp += 4;
			strcpy((char *)pp, "publickey");
			pp += 9;
			*pp++ = 0;
			goto pac;

		case SSH_WT_UA_BANNER:
			pp = ps + 5;
			*pp++ = SSH_MSG_USERAUTH_BANNER;
			if (pss->vhd && pss->vhd->ops->banner)
				n = (int)pss->vhd->ops->banner((char *)&buf[650],
							  150 - 1,
							  lang, (int)sizeof(lang));
			lws_p32(pp, (uint32_t)n);
			pp += 4;
			strcpy((char *)pp, (char *)&buf[650]);
			pp += n;
			if (lws_cstr(&pp, lang, sizeof(lang)))
				goto bail;
			goto pac;

		case SSH_WT_UA_PK_OK:
			/*
			 *  The server MUST respond to this message with
			 *  either SSH_MSG_USERAUTH_FAILURE or with the
			 *  following:
			 *
			 *    byte      SSH_MSG_USERAUTH_PK_OK
			 *    string    public key alg name from the request
			 *    string    public key blob from the request
      			 */
			n = 74 + (int)pss->ua->pubkey_len;
			if (n > (int)sizeof(buf) - LWS_PRE) {
				lwsl_notice("pubkey too large\n");
				goto bail;
			}
			ps1 = sshd_zalloc((unsigned int)n);
			if (!ps1)
				goto bail;
			ps = ps1;
			pp = ps1 + 5;
			*pp++ = SSH_MSG_USERAUTH_PK_OK;
			if (lws_cstr(&pp, pss->ua->alg, 64)) {
				free(ps1);
				goto bail;
			}
			lws_p32(pp, pss->ua->pubkey_len);
			pp += 4;
			memcpy(pp, pss->ua->pubkey, pss->ua->pubkey_len);
			pp += pss->ua->pubkey_len;

			/* we no longer need the UA now we judged it */
			lws_ua_destroy(pss);

			goto pac;

		case SSH_WT_UA_SUCCESS:
			pp = ps + 5;
			*pp++ = SSH_MSG_USERAUTH_SUCCESS;
			/* end SSH_PENDING_TIMEOUT_CONNECT_TO_SUCCESSFUL_AUTH */
			lws_set_timeout(wsi, NO_PENDING_TIMEOUT, 0);
			goto pac;

		case SSH_WT_CH_OPEN_CONF:
			pp = ps + 5;
			*pp++ = SSH_MSG_CHANNEL_OPEN_CONFIRMATION;
			lws_p32(pp, pss->ch_temp->sender_ch);
			pp += 4;
			lws_p32(pp, pss->ch_temp->server_ch);
			pp += 4;
			/* tx initial window size towards us */
			lws_p32(pp, LWS_SSH_INITIAL_WINDOW);
			pp += 4;
			/* maximum packet size towards us */
			lws_p32(pp, 800);
			pp += 4;
			lwsl_info("SSH_WT_CH_OPEN_CONF\n");
			/* it's on the linked-list */
			pss->ch_temp = NULL;
			goto pac;

		case SSH_WT_CH_FAILURE:
			pp = ps + 5;
			*pp++ = SSH_MSG_CHANNEL_OPEN_FAILURE;
			lws_p32(pp, ch->sender_ch);
			pp += 4;
			lws_p32(pp, ch->server_ch);
			pp += 4;
			lws_cstr(&pp, "reason", 64);
			lws_cstr(&pp, "en/US", 64);
			lwsl_info("SSH_WT_CH_FAILURE\n");
			goto pac;

		case SSH_WT_CHRQ_SUCC:
			pp = ps + 5;
			*pp++ = SSH_MSG_CHANNEL_SUCCESS;
			lws_p32(pp, ch->sender_ch);
			lwsl_info("SSH_WT_CHRQ_SUCC\n");
			pp += 4;
			goto pac;

		case SSH_WT_CHRQ_FAILURE:
			pp = ps + 5;
			*pp++ = SSH_MSG_CHANNEL_FAILURE;
			lws_p32(pp, ch->sender_ch);
			pp += 4;
			lwsl_info("SSH_WT_CHRQ_FAILURE\n");
			goto pac;

		case SSH_WT_CH_CLOSE:
			pp = ps + 5;
			*pp++ = SSH_MSG_CHANNEL_CLOSE;
			lws_p32(pp, ch->sender_ch);
			lwsl_info("SSH_WT_CH_CLOSE\n");
			pp += 4;
			goto pac;

		case SSH_WT_CH_EOF:
			pp = ps + 5;
			*pp++ = SSH_MSG_CHANNEL_EOF;
			lws_p32(pp, ch->sender_ch);
			lwsl_info("SSH_WT_CH_EOF\n");
			pp += 4;
			goto pac;

		case SSH_WT_SCP_ACK_ERROR:
		case SSH_WT_SCP_ACK_OKAY:
			pp = ps + 5;
			*pp++ = SSH_MSG_CHANNEL_DATA;
			/* ps + 6 */
			lws_p32(pp, ch->sender_ch);
			pp += 4;
			lws_p32(pp, 1);
			pp += 4;
			if (o == SSH_WT_SCP_ACK_ERROR)
				*pp++ = 2;
			else
				*pp++ = 0;
			lwsl_info("SSH_WT_SCP_ACK_OKAY\n");
			goto pac;

		case SSH_WT_WINDOW_ADJUST:
			pp = ps + 5;
			*pp++ = SSH_MSG_CHANNEL_WINDOW_ADJUST;
			/* ps + 6 */
			lws_p32(pp, ch->sender_ch);
			pp += 4;
			lws_p32(pp, 32768);
			pp += 4;
			lwsl_info("send SSH_MSG_CHANNEL_WINDOW_ADJUST\n");
			goto pac;

		case SSH_WT_EXIT_STATUS:
			pp = ps + 5;
			*pp++ = SSH_MSG_CHANNEL_REQUEST;
			lws_p32(pp, ch->sender_ch);
			pp += 4;
			lws_p32(pp, 11);
			pp += 4;
			strcpy((char *)pp, "exit-status");
			pp += 11;
			*pp++ = 0;
			lws_p32(pp, (uint32_t)ch->retcode);
			pp += 4;
			lwsl_info("send SSH_MSG_CHANNEL_EXIT_STATUS\n");
			goto pac;

		case SSH_WT_NONE:
		default:
			/* sending payload */

			ch = ssh_get_server_ch(pss, 0);
			/* have a channel up to send on? */
			if (!ch)
				break;

			if (!pss->vhd || !pss->vhd->ops)
				break;
			n = pss->vhd->ops->tx_waiting(ch->priv);
			if (n < 0)
				return -1;
			if (!n)
				/* nothing to send */
				break;

			if (n == (LWS_STDOUT | LWS_STDERR)) {
				/* pick one using round-robin */
				if (pss->serviced_stderr_last)
					n = LWS_STDOUT;
				else
					n = LWS_STDERR;
			}

			pss->serviced_stderr_last = !!(n & LWS_STDERR);

			/* stdout or stderr */
			pp = ps + 5;
			if (n == LWS_STDOUT)
				*pp++ = SSH_MSG_CHANNEL_DATA;
			else
				*pp++ = SSH_MSG_CHANNEL_EXTENDED_DATA;
			/* ps + 6 */
			lws_p32(pp, pss->ch_list->sender_ch);
			m = 14;
			if (n == LWS_STDERR) {
				pp += 4;
				/* data type code... 1 for stderr payload */
				lws_p32(pp, SSH_EXTENDED_DATA_STDERR);
				m = 18;
			}
			/* also skip another strlen u32 at + 10 / +14 */
			pp += 8;
			/* ps + 14 / + 18 */

			pp += pss->vhd->ops->tx(ch->priv, n, pp,
						lws_ptr_diff_size_t(
							&buf[sizeof(buf) - 1], pp));

			lws_p32(ps + m - 4, (uint32_t)lws_ptr_diff(pp, (ps + m)));

			if (pss->vhd->ops->tx_waiting(ch->priv) > 0)
				lws_callback_on_writable(wsi);

			ch->window -= lws_ptr_diff(pp, ps) - m;
			//lwsl_debug("our send window: %d\n", ch->window);

			/* fallthru */
pac:
			if (!pss->vhd)
				break;
			n = (int)pad_and_encrypt(&buf[LWS_PRE], ps, pp, pss, 0);
			break;

bail:
			lws_ua_destroy(pss);
			lws_kex_destroy(pss);

			return 1;

		}

		if (n > 0) {
			m = lws_write(wsi, (unsigned char *)buf + LWS_PRE, (unsigned int)n,
				      LWS_WRITE_HTTP);

			switch(o) {
			case SSH_WT_SEND_NEWKEYS:
				lwsl_info("Activating STC keys\n");
				pss->active_keys_stc = pss->kex->keys_next_stc;
				lws_chacha_activate(&pss->active_keys_stc);
				pss->kex_state = KEX_STATE_CRYPTO_INITIALIZED;
				pss->kex->newkeys |= 1;
				if (pss->kex->newkeys == 3)
					lws_kex_destroy(pss);
				break;
			case SSH_WT_UA_PK_OK:
				free(ps1);
				break;
			case SSH_WT_CH_CLOSE:
				if (ch->received_close) {
					/*
					 * We are sending this at the behest of
					 * the remote peer...
					 * we can destroy the channel with no
					 * further communication.
					 */
					ssh_destroy_channel(pss, ch);
					break;
				}
				ch->sent_close = 1;
				break;
			}
	                if (m < 0) {
	                        lwsl_err("ERR %d from write\n", m);
	                        goto bail;
	                }

			if (o != SSH_WT_VERSION)
				pss->ssh_sequence_ctr_stc++;

			if (o != SSH_WT_NONE)
				pss->wt_tail =
					(pss->wt_tail + 1) & 7;
		} else
			if (o == SSH_WT_UA_PK_OK) /* free it either way */
				free(ps1);

		ch = ssh_get_server_ch(pss, 0);

		if (pss->wt_head != pss->wt_tail ||
		    (ch && ch->priv && pss->vhd &&
		     pss->vhd->ops->tx_waiting(ch->priv)))
		       lws_callback_on_writable(wsi);

		break;

	case LWS_CALLBACK_SSH_UART_SET_RXFLOW:
		/*
		 * this is sent to set rxflow state on any connections that
		 * sink on a particular sink.  The sink index affected is in len
		 *
		 * More than one protocol may sink to the same uart, and the
		 * protocol may select the sink itself, eg, in the URL used
		 * to set up the connection.
		 */
		lwsl_notice("sshd LWS_CALLBACK_SSH_UART_SET_RXFLOW: wsi %p, %d\n",
				wsi, (int)len & 1);
		lws_rx_flow_control(wsi, len & 1);
		break;

	case LWS_CALLBACK_CGI:
		if (!pss)
			break;
		if (pss->vhd && pss->vhd->ops &&
		    pss->vhd->ops->child_process_io &&
		    pss->vhd->ops->child_process_io(pss->ch_temp->priv,
					pss->wsi, (struct lws_cgi_args *)in))
			return -1;
		break;

	case LWS_CALLBACK_CGI_PROCESS_ATTACH:
		if (!pss)
			break;
		ch = ssh_get_server_ch(pss, pss->channel_doing_spawn);
		if (ch) {
			ch->spawn_pid = (uint32_t)len; /* child process PID */
			lwsl_notice("associated PID %d to ch %d\n", (int)len,
				    pss->channel_doing_spawn);
		}
		break;

	case LWS_CALLBACK_CGI_TERMINATED:
		if (!pss)
			break;
		if (pss->vhd && pss->vhd->ops &&
		    pss->vhd->ops->child_process_terminated)
		    pss->vhd->ops->child_process_terminated(pss->ch_temp->priv,
							    pss->wsi);
		/*
		 * we have the child PID in len... we need to match it to a
		 * channel that is on the wsi
		 */
		ch = pss->ch_list;

		while (ch) {
			if (ch->spawn_pid == len) {
				lwsl_notice("starting close of ch with PID %d\n",
					    (int)len);
				ch->scheduled_close = 1;
				write_task(pss, ch, SSH_WT_CH_CLOSE);
				break;
			}
			ch = ch->next;
		}
		break;

	default:
		break;
	}

	return 0;
}

#define LWS_PLUGIN_PROTOCOL_LWS_RAW_SSHD { \
		"lws-ssh-base",	\
		lws_callback_raw_sshd,	\
		sizeof(struct per_session_data__sshd),	\
		1024, 0, NULL, 900	\
	}

LWS_VISIBLE const struct lws_protocols lws_ssh_base_protocols[] = {
	LWS_PLUGIN_PROTOCOL_LWS_RAW_SSHD,
	{ NULL, NULL, 0, 0, 0, NULL, 0 } /* terminator */
};

#if !defined (LWS_PLUGIN_STATIC)

LWS_VISIBLE const lws_plugin_protocol_t lws_ssh_base = {
	.hdr = {
		"ssh base",
		"lws_protocol_plugin",
		LWS_BUILD_HASH,
		LWS_PLUGIN_API_MAGIC
	},

	.protocols = lws_ssh_base_protocols,
	.count_protocols = LWS_ARRAY_SIZE(lws_ssh_base_protocols),
	.extensions = NULL,
	.count_extensions = 0,
};

#endif
