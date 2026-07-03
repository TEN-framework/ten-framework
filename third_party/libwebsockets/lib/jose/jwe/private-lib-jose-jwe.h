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

#define LWS_AESGCM_IV 12
#define LWS_AESGCM_TAG 16

/* jwe-rsa-aescbc.c */

int
lws_jwe_auth_and_decrypt_rsa_aes_cbc_hs(struct lws_jwe *jwe);


int
lws_jwe_encrypt_rsa_aes_cbc_hs(struct lws_jwe *jwe,
			       char *temp, int *temp_len);

int
lws_jwe_auth_and_decrypt_cbc_hs(struct lws_jwe *jwe, uint8_t *enc_cek,
				uint8_t *aad, int aad_len);


/* jws-rsa-aesgcm.c */

int
lws_jwe_auth_and_decrypt_gcm(struct lws_jwe *jwe, uint8_t *enc_cek,
			     uint8_t *aad, int aad_len);

int
lws_jwe_auth_and_decrypt_rsa_aes_gcm(struct lws_jwe *jwe);

int
lws_jwe_encrypt_gcm(struct lws_jwe *jwe,
		    uint8_t *enc_cek, uint8_t *aad, int aad_len);

int
lws_jwe_encrypt_rsa_aes_gcm(struct lws_jwe *jwe,
			    char *temp, int *temp_len);




/* jwe-rsa-aeskw.c */

int
lws_jwe_encrypt_aeskw_cbc_hs(struct lws_jwe *jwe,
			     char *temp, int *temp_len);

int
lws_jwe_auth_and_decrypt_aeskw_cbc_hs(struct lws_jwe *jwe);

/* aescbc.c */

int
lws_jwe_auth_and_decrypt_cbc_hs(struct lws_jwe *jwe, uint8_t *enc_cek,
				uint8_t *aad, int aad_len);

int
lws_jwe_encrypt_cbc_hs(struct lws_jwe *jwe,
		       uint8_t *cek, uint8_t *aad, int aad_len);

int
lws_jwe_auth_and_decrypt_ecdh_cbc_hs(struct lws_jwe *jwe,
		char *temp, int *temp_len);

int
lws_jwe_encrypt_ecdh_cbc_hs(struct lws_jwe *jwe,
		 	     char *temp, int *temp_len);
