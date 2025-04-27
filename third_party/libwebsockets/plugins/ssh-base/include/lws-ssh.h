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

#if !defined(__LWS_SSH_H__)
#define __LWS_SSH_H__

#if defined(LWS_HAVE_SYS_TYPES_H)
#include <sys/types.h>
#endif

#if defined(LWS_WITH_MBEDTLS)
#include "mbedtls/sha1.h"
#include "mbedtls/sha256.h"
#include "mbedtls/sha512.h"
#include "mbedtls/rsa.h"
#endif

#include "lws-plugin-ssh.h"

#define LWS_SIZE_EC25519	32
#define LWS_SIZE_EC25519_PUBKEY 32
#define LWS_SIZE_EC25519_PRIKEY 64

#define LWS_SIZE_SHA256		32
#define LWS_SIZE_SHA512		64

#define LWS_SIZE_AES256_KEY	32
#define LWS_SIZE_AES256_IV	12
#define LWS_SIZE_AES256_MAC	16
#define LWS_SIZE_AES256_BLOCK	16

#define LWS_SIZE_CHACHA256_KEY	(2 * 32)
#define POLY1305_TAGLEN		16
#define POLY1305_KEYLEN		32

#define crypto_hash_sha512_BYTES 64U

#define PEEK_U64(p) \
        (((uint64_t)(((const uint8_t *)(p))[0]) << 56) | \
         ((uint64_t)(((const uint8_t *)(p))[1]) << 48) | \
         ((uint64_t)(((const uint8_t *)(p))[2]) << 40) | \
         ((uint64_t)(((const uint8_t *)(p))[3]) << 32) | \
         ((uint64_t)(((const uint8_t *)(p))[4]) << 24) | \
         ((uint64_t)(((const uint8_t *)(p))[5]) << 16) | \
         ((uint64_t)(((const uint8_t *)(p))[6]) << 8) | \
          (uint64_t)(((const uint8_t *)(p))[7]))
#define PEEK_U32(p) \
        (((uint32_t)(((const uint8_t *)(p))[0]) << 24) | \
         ((uint32_t)(((const uint8_t *)(p))[1]) << 16) | \
         ((uint32_t)(((const uint8_t *)(p))[2]) << 8) | \
          (uint32_t)(((const uint8_t *)(p))[3]))
#define PEEK_U16(p) \
        (((uint16_t)(((const uint8_t *)(p))[0]) << 8) | \
          (uint16_t)(((const uint8_t *)(p))[1]))

#define POKE_U64(p, v) \
        do { \
                const uint64_t __v = (v); \
                ((uint8_t *)(p))[0] = (uint8_t)((__v >> 56) & 0xff); \
                ((uint8_t *)(p))[1] = (uint8_t)((__v >> 48) & 0xff); \
                ((uint8_t *)(p))[2] = (uint8_t)((__v >> 40) & 0xff); \
                ((uint8_t *)(p))[3] = (uint8_t)((__v >> 32) & 0xff); \
                ((uint8_t *)(p))[4] = (uint8_t)((__v >> 24) & 0xff); \
                ((uint8_t *)(p))[5] = (uint8_t)((__v >> 16) & 0xff); \
                ((uint8_t *)(p))[6] = (uint8_t)((__v >> 8) & 0xff); \
                ((uint8_t *)(p))[7] = (uint8_t)(__v & 0xff); \
        } while (0)
#define POKE_U32(p, v) \
        do { \
                const uint32_t __v = (v); \
                ((uint8_t *)(p))[0] = (uint8_t)((__v >> 24) & 0xff); \
                ((uint8_t *)(p))[1] = (uint8_t)((__v >> 16) & 0xff); \
                ((uint8_t *)(p))[2] = (uint8_t)((__v >> 8) & 0xff); \
                ((uint8_t *)(p))[3] = (uint8_t)(__v & 0xff); \
        } while (0)
#define POKE_U16(p, v) \
        do { \
                const uint16_t __v = (v); \
                ((uint8_t *)(p))[0] = (__v >> 8) & 0xff; \
                ((uint8_t *)(p))[1] = __v & 0xff; \
        } while (0)


enum {
	SSH_MSG_DISCONNECT					= 1,
	SSH_MSG_IGNORE						= 2,
	SSH_MSG_UNIMPLEMENTED					= 3,
	SSH_MSG_DEBUG						= 4,
	SSH_MSG_SERVICE_REQUEST					= 5,
	SSH_MSG_SERVICE_ACCEPT					= 6,
	SSH_MSG_KEXINIT						= 20,
	SSH_MSG_NEWKEYS						= 21,

	/* 30 .. 49: KEX messages specific to KEX protocol */
	SSH_MSG_KEX_ECDH_INIT					= 30,
	SSH_MSG_KEX_ECDH_REPLY					= 31,

	/* 50... userauth */

	SSH_MSG_USERAUTH_REQUEST				= 50,
	SSH_MSG_USERAUTH_FAILURE				= 51,
	SSH_MSG_USERAUTH_SUCCESS				= 52,
	SSH_MSG_USERAUTH_BANNER					= 53,

	/* 60... publickey */

	SSH_MSG_USERAUTH_PK_OK					= 60,

	/* 80... connection */

	SSH_MSG_GLOBAL_REQUEST					= 80,
	SSH_MSG_REQUEST_SUCCESS					= 81,
	SSH_MSG_REQUEST_FAILURE					= 82,

	SSH_MSG_CHANNEL_OPEN					= 90,
	SSH_MSG_CHANNEL_OPEN_CONFIRMATION			= 91,
	SSH_MSG_CHANNEL_OPEN_FAILURE				= 92,
	SSH_MSG_CHANNEL_WINDOW_ADJUST				= 93,
	SSH_MSG_CHANNEL_DATA					= 94,
	SSH_MSG_CHANNEL_EXTENDED_DATA				= 95,
	SSH_MSG_CHANNEL_EOF					= 96,
	SSH_MSG_CHANNEL_CLOSE					= 97,
	SSH_MSG_CHANNEL_REQUEST					= 98,
	SSH_MSG_CHANNEL_SUCCESS					= 99,
	SSH_MSG_CHANNEL_FAILURE					= 100,

	SSH_EXTENDED_DATA_STDERR				= 1,

	SSH_CH_TYPE_SESSION					= 1,
	SSH_CH_TYPE_SCP						= 2,
	SSH_CH_TYPE_SFTP					= 3,

	SSH_DISCONNECT_HOST_NOT_ALLOWED_TO_CONNECT		= 1,
	SSH_DISCONNECT_PROTOCOL_ERROR				= 2,
	SSH_DISCONNECT_KEY_EXCHANGE_FAILED			= 3,
	SSH_DISCONNECT_RESERVED					= 4,
	SSH_DISCONNECT_MAC_ERROR				= 5,
	SSH_DISCONNECT_COMPRESSION_ERROR			= 6,
	SSH_DISCONNECT_SERVICE_NOT_AVAILABLE			= 7,
	SSH_DISCONNECT_PROTOCOL_VERSION_NOT_SUPPORTED		= 8,
	SSH_DISCONNECT_HOST_KEY_NOT_VERIFIABLE			= 9,
	SSH_DISCONNECT_CONNECTION_LOST				= 10,
	SSH_DISCONNECT_BY_APPLICATION				= 11,
	SSH_DISCONNECT_TOO_MANY_CONNECTIONS			= 12,
	SSH_DISCONNECT_AUTH_CANCELLED_BY_USER			= 13,
	SSH_DISCONNECT_NO_MORE_AUTH_METHODS_AVAILABLE		= 14,
	SSH_DISCONNECT_ILLEGAL_USER_NAME			= 15,
	
	SSH_OPEN_ADMINISTRATIVELY_PROHIBITED			= 1,
	SSH_OPEN_CONNECT_FAILED					= 2,
	SSH_OPEN_UNKNOWN_CHANNEL_TYPE				= 3,
	SSH_OPEN_RESOURCE_SHORTAGE				= 4,

	KEX_STATE_EXPECTING_CLIENT_OFFER			= 0,
	KEX_STATE_REPLIED_TO_OFFER,
	KEX_STATE_CRYPTO_INITIALIZED,

	SSH_KEYIDX_IV						= 0,
	SSH_KEYIDX_ENC,
	SSH_KEYIDX_INTEG,

	/* things we may write on the connection */

	SSH_WT_NONE						= 0,
	SSH_WT_VERSION,
	SSH_WT_OFFER,
	SSH_WT_OFFER_REPLY,
	SSH_WT_SEND_NEWKEYS,
	SSH_WT_UA_ACCEPT,
	SSH_WT_UA_FAILURE,
	SSH_WT_UA_BANNER,
	SSH_WT_UA_PK_OK,
	SSH_WT_UA_SUCCESS,
	SSH_WT_CH_OPEN_CONF,
	SSH_WT_CH_FAILURE,
	SSH_WT_CHRQ_SUCC,
	SSH_WT_CHRQ_FAILURE,
	SSH_WT_SCP_ACK_OKAY,
	SSH_WT_SCP_ACK_ERROR,
	SSH_WT_CH_CLOSE,
	SSH_WT_CH_EOF,
	SSH_WT_WINDOW_ADJUST,
	SSH_WT_EXIT_STATUS,

	/* RX parser states */

	SSH_INITIALIZE_TRANSIENT				= 0,
	SSHS_IDSTRING,
	SSHS_IDSTRING_CR,
	SSHS_MSG_LEN,
	SSHS_MSG_PADDING,
	SSHS_MSG_ID,
	SSH_KEX_STATE_COOKIE,
	SSH_KEX_NL_KEX_ALGS_LEN,
	SSH_KEX_NL_KEX_ALGS,
	SSH_KEX_NL_SHK_ALGS_LEN,
	SSH_KEX_NL_SHK_ALGS,
	SSH_KEX_NL_EACTS_ALGS_LEN,
	SSH_KEX_NL_EACTS_ALGS,
	SSH_KEX_NL_EASTC_ALGS_LEN,
	SSH_KEX_NL_EASTC_ALGS,
	SSH_KEX_NL_MACTS_ALGS_LEN,
	SSH_KEX_NL_MACTS_ALGS,
	SSH_KEX_NL_MASTC_ALGS_LEN,
	SSH_KEX_NL_MASTC_ALGS,
	SSH_KEX_NL_CACTS_ALGS_LEN,
	SSH_KEX_NL_CACTS_ALGS,
	SSH_KEX_NL_CASTC_ALGS_LEN,
	SSH_KEX_NL_CASTC_ALGS,
	SSH_KEX_NL_LCTS_ALGS_LEN,
	SSH_KEX_NL_LCTS_ALGS,
	SSH_KEX_NL_LSTC_ALGS_LEN,
	SSH_KEX_NL_LSTC_ALGS,
	SSH_KEX_FIRST_PKT,
	SSH_KEX_RESERVED,

	SSH_KEX_STATE_ECDH_KEYLEN,
	SSH_KEX_STATE_ECDH_Q_C,

	SSHS_MSG_EAT_PADDING,
	SSH_KEX_STATE_SKIP,

	SSHS_GET_STRING_LEN,
	SSHS_GET_STRING,
	SSHS_GET_STRING_LEN_ALLOC,
	SSHS_GET_STRING_ALLOC,
	SSHS_DO_SERVICE_REQUEST,

	SSHS_DO_UAR_SVC,
	SSHS_DO_UAR_PUBLICKEY,
	SSHS_NVC_DO_UAR_CHECK_PUBLICKEY,
	SSHS_DO_UAR_SIG_PRESENT,
	SSHS_NVC_DO_UAR_ALG,
	SSHS_NVC_DO_UAR_PUBKEY_BLOB,
	SSHS_NVC_DO_UAR_SIG,

	SSHS_GET_U32,

	SSHS_NVC_CHOPEN_TYPE,
	SSHS_NVC_CHOPEN_SENDER_CH,
	SSHS_NVC_CHOPEN_WINSIZE,
	SSHS_NVC_CHOPEN_PKTSIZE,

	SSHS_NVC_CHRQ_RECIP,
	SSHS_NVC_CHRQ_TYPE,
	SSHS_CHRQ_WANT_REPLY,
        SSHS_NVC_CHRQ_TERM,
        SSHS_NVC_CHRQ_TW,
        SSHS_NVC_CHRQ_TH,
	SSHS_NVC_CHRQ_TWP,
        SSHS_NVC_CHRQ_THP,
        SSHS_NVC_CHRQ_MODES,

	SSHS_NVC_CHRQ_ENV_NAME,
	SSHS_NVC_CHRQ_ENV_VALUE,

	SSHS_NVC_CHRQ_EXEC_CMD,

	SSHS_NVC_CHRQ_SUBSYSTEM,

	SSHS_NVC_CHRQ_WNDCHANGE_TW,
	SSHS_NVC_CHRQ_WNDCHANGE_TH,
	SSHS_NVC_CHRQ_WNDCHANGE_TWP,
	SSHS_NVC_CHRQ_WNDCHANGE_THP,

	SSHS_NVC_CH_EOF,
	SSHS_NVC_CH_CLOSE,

	SSHS_NVC_CD_RECIP,
	SSHS_NVC_CD_DATA,
	SSHS_NVC_CD_DATA_ALLOC,

	SSHS_NVC_WA_RECIP,
	SSHS_NVC_WA_ADD,

	SSHS_NVC_DISCONNECT_REASON,
	SSHS_NVC_DISCONNECT_DESC,
	SSHS_NVC_DISCONNECT_LANG,

	SSHS_SCP_COLLECTSTR			= 0,
	SSHS_SCP_PAYLOADIN			= 1,


	/* from https://tools.ietf.org/html/draft-ietf-secsh-filexfer-13 */

	SECSH_FILEXFER_VERSION			= 6,

	/* sftp packet types */

	SSH_FXP_INIT				= 1,
	SSH_FXP_VERSION				= 2,
	SSH_FXP_OPEN				= 3,
	SSH_FXP_CLOSE				= 4,
	SSH_FXP_READ				= 5,
	SSH_FXP_WRITE				= 6,
	SSH_FXP_LSTAT				= 7,
	SSH_FXP_FSTAT				= 8,
	SSH_FXP_SETSTAT				= 9,
	SSH_FXP_FSETSTAT			= 10,
	SSH_FXP_OPENDIR				= 11,
	SSH_FXP_READDIR				= 12,
	SSH_FXP_REMOVE				= 13,
	SSH_FXP_MKDIR				= 14,
	SSH_FXP_RMDIR				= 15,
	SSH_FXP_REALPATH			= 16,
	SSH_FXP_STAT				= 17,
	SSH_FXP_RENAME				= 18,
	SSH_FXP_READLINK			= 19,
	SSH_FXP_LINK				= 21,
	SSH_FXP_BLOCK				= 22,
	SSH_FXP_UNBLOCK				= 23,
	SSH_FXP_STATUS				= 101,
	SSH_FXP_HANDLE				= 102,
	SSH_FXP_DATA				= 103,
	SSH_FXP_NAME				= 104,
	SSH_FXP_ATTRS				= 105,
	SSH_FXP_EXTENDED			= 200,
	SSH_FXP_EXTENDED_REPLY			= 201,

	/* sftp return codes */

	SSH_FX_OK				= 0,
	SSH_FX_EOF				= 1,
	SSH_FX_NO_SUCH_FILE			= 2,
	SSH_FX_PERMISSION_DENIED		= 3,
	SSH_FX_FAILURE				= 4,
	SSH_FX_BAD_MESSAGE			= 5,
	SSH_FX_NO_CONNECTION			= 6,
	SSH_FX_CONNECTION_LOST			= 7,
	SSH_FX_OP_UNSUPPORTED			= 8,
	SSH_FX_INVALID_HANDLE			= 9,
	SSH_FX_NO_SUCH_PATH			= 10,
	SSH_FX_FILE_ALREADY_EXISTS		= 11,
	SSH_FX_WRITE_PROTECT			= 12,
	SSH_FX_NO_MEDIA				= 13,
	SSH_FX_NO_SPACE_ON_FILESYSTEM		= 14,
	SSH_FX_QUOTA_EXCEEDED			= 15,
	SSH_FX_UNKNOWN_PRINCIPAL		= 16,
	SSH_FX_LOCK_CONFLICT			= 17,
	SSH_FX_DIR_NOT_EMPTY			= 18,
	SSH_FX_NOT_A_DIRECTORY			= 19,
	SSH_FX_INVALID_FILENAME			= 20,
	SSH_FX_LINK_LOOP			= 21,
	SSH_FX_CANNOT_DELETE			= 22,
	SSH_FX_INVALID_PARAMETER		= 23,
	SSH_FX_FILE_IS_A_DIRECTORY		= 24,
	SSH_FX_BYTE_RANGE_LOCK_CONFLICT		= 25,
	SSH_FX_BYTE_RANGE_LOCK_REFUSED		= 26,
	SSH_FX_DELETE_PENDING			= 27,
	SSH_FX_FILE_CORRUPT			= 28,
	SSH_FX_OWNER_INVALID			= 29,
	SSH_FX_GROUP_INVALID			= 30,
	SSH_FX_NO_MATCHING_BYTE_RANGE_LOCK	= 31,


	SSH_PENDING_TIMEOUT_CONNECT_TO_SUCCESSFUL_AUTH =
			PENDING_TIMEOUT_USER_REASON_BASE + 0,

	SSH_AUTH_STATE_NO_AUTH			= 0,
	SSH_AUTH_STATE_GAVE_AUTH_IGNORE_REQS	= 1,
};

#define LWS_SSH_INITIAL_WINDOW 16384

struct lws_ssh_userauth {
	struct lws_genhash_ctx hash_ctx;
	char *username;
	char *service;
	char *alg;
	uint8_t *pubkey;
	uint32_t pubkey_len;
	uint8_t *sig;
	uint32_t sig_len;
	char sig_present;
};

struct lws_ssh_keys {
	/* 3 == SSH_KEYIDX_IV (len=4), SSH_KEYIDX_ENC, SSH_KEYIDX_INTEG */
	uint8_t key[3][LWS_SIZE_CHACHA256_KEY];

	/* opaque allocation made when cipher activated */
	void *cipher;

	uint8_t MAC_length;
	uint8_t padding_alignment; /* block size */
	uint8_t valid:1;
	uint8_t full_length:1;
};

struct lws_kex {
	uint8_t kex_r[256];
	uint8_t Q_C[LWS_SIZE_EC25519]; /* client eph public key aka 'e' */
	uint8_t eph_pri_key[LWS_SIZE_EC25519]; /* server eph private key */
	uint8_t Q_S[LWS_SIZE_EC25519]; /* server ephemeral public key */
	uint8_t kex_cookie[16];
	uint8_t *I_C; /* malloc'd copy of client KEXINIT payload */
	uint8_t *I_S; /* malloc'd copy of server KEXINIT payload */
	uint32_t I_C_payload_len;
	uint32_t I_C_alloc_len;
	uint32_t I_S_payload_len;
	uint32_t kex_r_len;
	uint8_t match_bitfield;
	uint8_t newkeys; /* which sides newkeys have been applied */

	struct lws_ssh_keys keys_next_cts;
	struct lws_ssh_keys keys_next_stc;
};

struct lws_subprotocol_scp {
	char fp[128];
	uint64_t len;
	uint32_t attr;
	char cmd;
	char ips;
};

typedef union {
	struct lws_subprotocol_scp scp;
} lws_subprotocol;

struct per_session_data__sshd;

struct lws_ssh_channel {
	struct lws_ssh_channel *next;

	struct per_session_data__sshd *pss;

	lws_subprotocol *sub; /* NULL, or allocated subprotocol state */
	void *priv; /* owned by user code */
	int type;
	uint32_t server_ch;
	uint32_t sender_ch;
	int32_t window;
	int32_t peer_window_est;
	uint32_t max_pkt;

	uint32_t spawn_pid;
	int retcode;

	uint8_t scheduled_close:1;
	uint8_t sent_close:1;
	uint8_t received_close:1;
};

struct per_vhost_data__sshd;

struct per_session_data__sshd {
	struct per_session_data__sshd *next;
	struct per_vhost_data__sshd *vhd;
	struct lws *wsi;

	struct lws_kex *kex;
	char *disconnect_desc;

	uint8_t K[LWS_SIZE_EC25519]; /* shared secret */
	uint8_t session_id[LWS_SIZE_SHA256]; /* H from first working KEX */
	char name[64];
	char last_auth_req_username[32];
	char last_auth_req_service[32];

	struct lws_ssh_keys active_keys_cts;
	struct lws_ssh_keys active_keys_stc;
	struct lws_ssh_userauth *ua;
	struct lws_ssh_channel *ch_list;
	struct lws_ssh_channel *ch_temp;

	uint8_t *last_alloc;

	union {
		struct lws_ssh_pty pty;
		char aux[64];
	} args;

	uint32_t ssh_sequence_ctr_cts;
	uint32_t ssh_sequence_ctr_stc;

	uint64_t payload_bytes_cts;
	uint64_t payload_bytes_stc;

	uint32_t disconnect_reason;

	char V_C[64]; /* Client version String */
	uint8_t packet_assembly[2048];
	uint32_t pa_pos;

	uint32_t msg_len;
	uint32_t pos;
	uint32_t len;
	uint32_t ctr;
	uint32_t npos;
	uint32_t reason;
	uint32_t channel_doing_spawn;
	int next_ch_num;

	uint8_t K_S[LWS_SIZE_EC25519]; /* server public key */

	uint32_t copy_to_I_C:1;
	uint32_t okayed_userauth:1;
	uint32_t sent_banner:1;
	uint32_t seen_auth_req_before:1;
	uint32_t serviced_stderr_last:1;
	uint32_t kex_state;
	uint32_t chrq_server_port;
	uint32_t ch_recip;
	uint32_t count_auth_attempts;

	char parser_state;
	char state_after_string;
	char first_coming;
	uint8_t rq_want_reply;
	uint8_t ssh_auth_state;

	uint8_t msg_id;
	uint8_t msg_padding;
	uint8_t write_task[8];
	struct lws_ssh_channel *write_channel[8];
	uint8_t wt_head, wt_tail;
};

struct per_vhost_data__sshd {
	struct lws_context *context;
	struct lws_vhost *vhost;
	const struct lws_protocols *protocol;
	struct per_session_data__sshd *live_pss_list;
	const struct lws_ssh_ops *ops;
};


struct host_keys {
	uint8_t *data;
	uint32_t len;
};

extern struct host_keys host_keys[];

extern int
crypto_scalarmult_curve25519(unsigned char *q, const unsigned char *n,
			     const unsigned char *p);

extern int
ed25519_key_parse(uint8_t *p, size_t len, char *type, size_t type_len,
                  uint8_t *pub, uint8_t *pri);

extern int
kex_ecdh(struct per_session_data__sshd *pss, uint8_t *result, uint32_t *plen);

extern uint32_t
lws_g32(uint8_t **p);

extern uint32_t
lws_p32(uint8_t *p, uint32_t v);

extern int
lws_timingsafe_bcmp(const void *a, const void *b, uint32_t len);

extern const char *lws_V_S;

extern int
lws_chacha_activate(struct lws_ssh_keys *keys);

extern void
lws_chacha_destroy(struct lws_ssh_keys *keys);

extern uint32_t
lws_chachapoly_get_length(struct lws_ssh_keys *keys, uint32_t seq,
			  const uint8_t *in4);

extern void
poly1305_auth(u_char out[POLY1305_TAGLEN], const u_char *m, size_t inlen,
    const u_char key[POLY1305_KEYLEN]);

extern int
lws_chacha_decrypt(struct lws_ssh_keys *keys, uint32_t seq,
		   const uint8_t *ct, uint32_t len, uint8_t *pt);
extern int
lws_chacha_encrypt(struct lws_ssh_keys *keys, uint32_t seq,
		   const uint8_t *ct, uint32_t len, uint8_t *pt);

extern void
lws_pad_set_length(struct per_session_data__sshd *pss, void *start, uint8_t **p,
		   struct lws_ssh_keys *keys);

extern size_t
get_gen_server_key_25519(struct per_session_data__sshd *pss, uint8_t *b, size_t len);

extern int
crypto_sign_ed25519(unsigned char *sm, unsigned long long *smlen,
		    const unsigned char *m, size_t mlen,
		    const unsigned char *sk);

extern int
crypto_sign_ed25519_keypair(struct lws_context *context, uint8_t *pk,
			    uint8_t *sk);

#endif
