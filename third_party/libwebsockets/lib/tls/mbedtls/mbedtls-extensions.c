/*
 * libwebsockets - small server side websockets and web server implementation
 *
 * Copyright (C) 2010 - 2022 Andy Green <andy@warmcat.com>
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
 *
 * These are additional apis that belong in mbedtls but do not yet exist there.
 * Alternaives are provided for lws to use that understand additional standard
 * v3 tls extensions.  Error results are simplified to lws style.
 *
 * This file includes code taken from mbedtls and modified, and from an as of
 * 2021-06-11 unaccepted-upstream patch for mbedtls contributed by Gábor Tóth
 * <toth92g@gmail.com>.  Gabor has graciously allowed use of his patch with more
 * liberal terms but to not complicate matters I provide it here under the same
 * Apache 2.0 terms as the mbedtls pieces.
 *
 * Those original pieces are licensed Apache-2.0 as follows
 *
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0
 *
 *  Licensed under the Apache License, Version 2.0 (the "License"); you may
 *  not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 *  WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

#include "private-lib-core.h"
#include "private-lib-tls-mbedtls.h"
#include <mbedtls/oid.h>
#include <mbedtls/x509.h>

/*
 * This section from mbedtls oid.c
 */

typedef struct {
    mbedtls_oid_descriptor_t    descriptor;
    int                 	ext_type;
} oid_x509_ext_t;

#define ADD_LEN(s)      s, MBEDTLS_OID_SIZE(s)

#define LWS_MBEDTLS_OID_AUTHORITY_KEY_IDENTIFIER    MBEDTLS_OID_ID_CE "\x23" /**< id-ce-authorityKeyIdentifier OBJECT IDENTIFIER ::=  { id-ce 35 } */
#define LWS_MBEDTLS_OID_SUBJECT_KEY_IDENTIFIER      MBEDTLS_OID_ID_CE "\x0E" /**< id-ce-subjectKeyIdentifier OBJECT IDENTIFIER ::=  { id-ce 14 } */

#define LWS_MBEDTLS_OID_X509_EXT_AUTHORITY_KEY_IDENTIFIER    (1 << 0)
#define LWS_MBEDTLS_OID_X509_EXT_SUBJECT_KEY_IDENTIFIER      (1 << 1)

#define LWS_MBEDTLS_X509_EXT_AUTHORITY_KEY_IDENTIFIER LWS_MBEDTLS_OID_X509_EXT_AUTHORITY_KEY_IDENTIFIER
#define LWS_MBEDTLS_X509_EXT_SUBJECT_KEY_IDENTIFIER LWS_MBEDTLS_OID_X509_EXT_SUBJECT_KEY_IDENTIFIER

#define LWS_MBEDTLS_X509_SAN_OTHER_NAME                      0
#define LWS_MBEDTLS_X509_SAN_RFC822_NAME                     1
#define LWS_MBEDTLS_X509_SAN_DNS_NAME                        2

#define LWS_MBEDTLS_ASN1_TAG_CLASS_MASK          0xC0
#define LWS_MBEDTLS_ASN1_TAG_VALUE_MASK		 0x1F

static const oid_x509_ext_t oid_x509_ext[] = {
    { {ADD_LEN( LWS_MBEDTLS_OID_SUBJECT_KEY_IDENTIFIER ),
		"id-ce-subjectKeyIdentifier",
		"Subject Key Identifier" },
        LWS_MBEDTLS_OID_X509_EXT_SUBJECT_KEY_IDENTIFIER,
    },
    { {ADD_LEN( LWS_MBEDTLS_OID_AUTHORITY_KEY_IDENTIFIER ),
        	"id-ce-authorityKeyIdentifier",
        	"Authority Key Identifier" },
        LWS_MBEDTLS_OID_X509_EXT_AUTHORITY_KEY_IDENTIFIER,
    },
    { { NULL, 0, NULL, NULL }, 0 },
};

#define FN_OID_TYPED_FROM_ASN1( TYPE_T, NAME, LIST )                    \
    static const TYPE_T * oid_ ## NAME ## _from_asn1(                   \
                                      const mbedtls_asn1_buf *oid )     \
    {                                                                   \
        const TYPE_T *p = (LIST);                                       \
        const mbedtls_oid_descriptor_t *cur =                           \
            (const mbedtls_oid_descriptor_t *) p;                       \
        if( p == NULL || oid == NULL ) return( NULL );                  \
        while( cur->MBEDTLS_PRIVATE(asn1) != NULL ) {                          \
            if( cur->MBEDTLS_PRIVATE(asn1_len) == oid->MBEDTLS_PRIVATE_V30_ONLY(len) && \
                memcmp( cur->MBEDTLS_PRIVATE(asn1), oid->MBEDTLS_PRIVATE_V30_ONLY(p), oid->MBEDTLS_PRIVATE_V30_ONLY(len) ) == 0 ) {          \
                return( p );                                            \
            }                                                           \
            p++;                                                        \
            cur = (const mbedtls_oid_descriptor_t *) p;                 \
        }                                                               \
        return( NULL );                                                 \
    }


#define FN_OID_GET_ATTR1(FN_NAME, TYPE_T, TYPE_NAME, ATTR1_TYPE, ATTR1) \
int FN_NAME( const mbedtls_asn1_buf *oid, ATTR1_TYPE * ATTR1 )                  \
{                                                                       \
    const TYPE_T *data = oid_ ## TYPE_NAME ## _from_asn1( oid );        \
    if (!data) return 1;            \
    *ATTR1 = data->ATTR1;                                               \
    return 0;                                                        \
}

FN_OID_TYPED_FROM_ASN1(oid_x509_ext_t, x509_ext, oid_x509_ext)
FN_OID_GET_ATTR1(lws_mbedtls_oid_get_x509_ext_type,
		 oid_x509_ext_t, x509_ext, int, ext_type)

typedef struct lws_mbedtls_x509_san_other_name
{
    /**
     * The type_id is an OID as deifned in RFC 5280.
     * To check the value of the type id, you should use
     * \p MBEDTLS_OID_CMP with a known OID mbedtls_x509_buf.
     */
    mbedtls_x509_buf type_id;                   /**< The type id. */
    union
    {
        /**
         * From RFC 4108 section 5:
         * HardwareModuleName ::= SEQUENCE {
         *                         hwType OBJECT IDENTIFIER,
         *                         hwSerialNum OCTET STRING }
         */
        struct
        {
            mbedtls_x509_buf oid;               /**< The object identifier. */
            mbedtls_x509_buf val;               /**< The named value. */
        }
        hardware_module_name;
    }
    value;
}
lws_mbedtls_x509_san_other_name;


typedef struct lws_mbedtls_x509_subject_alternative_name
{
	int type;                              /**< The SAN type, value of LWS_MBEDTLS_X509_SAN_XXX. */
	union {
	 lws_mbedtls_x509_san_other_name other_name; /**< The otherName supported type. */
	 mbedtls_x509_buf   unstructured_name; /**< The buffer for the un constructed types. Only dnsName currently supported */
	}
	san; /**< A union of the supported SAN types */
}
lws_mbedtls_x509_subject_alternative_name;

static int
x509_get_skid(uint8_t **p, const uint8_t *end, mbedtls_x509_buf *skid)
{
	int ret = 1;
	size_t len = 0u;

	ret = mbedtls_asn1_get_tag(p, end, &len, MBEDTLS_ASN1_OCTET_STRING);
	if (ret)
		return ret;

	skid->MBEDTLS_PRIVATE_V30_ONLY(len)	= len;
	skid->MBEDTLS_PRIVATE_V30_ONLY(tag)	= MBEDTLS_ASN1_OCTET_STRING;
	skid->MBEDTLS_PRIVATE_V30_ONLY(p)	= *p;
	*p					+= len;

	return *p != end;
}

/*
 * Names may have multiple allocated segments in a linked-list, when the mbedtls
 * api mbedtls_x509_get_name() fails, it doesn't clean up any already-allocated
 * segments, wrongly leaving it to the caller to handle.  This helper takes care
 * of the missing cleaning for allocation error path.
 *
 * name.next must be set to NULL by user code before calling ...get_name(...,
 * &name), since not every error exit sets it and it will contain garbage if
 * defined on stack as is usual.
 */

static void
lws_x509_clean_name(mbedtls_x509_name *name)
{
	mbedtls_x509_name *n1;

	if (!name)
		return;

	n1 = name->MBEDTLS_PRIVATE_V30_ONLY(next);

	while (n1) {
		name = n1->MBEDTLS_PRIVATE_V30_ONLY(next);
		free(n1);
		n1 = name;
	}
}

static int
lws_mbedtls_x509_parse_general_name(const mbedtls_x509_buf *name_buf,
				    lws_mbedtls_x509_subject_alternative_name *name)
{
	// mbedtls_x509_name_other_name other_name;
	uint8_t *bufferPointer, **p, *end;
	mbedtls_x509_name rfc822Name;
	int ret;

	switch (name_buf->MBEDTLS_PRIVATE_V30_ONLY(tag) &
				(LWS_MBEDTLS_ASN1_TAG_CLASS_MASK |
				 LWS_MBEDTLS_ASN1_TAG_VALUE_MASK)) {

#if 0
	case MBEDTLS_ASN1_CONTEXT_SPECIFIC | LWS_MBEDTLS_X509_SAN_OTHER_NAME:
		ret = x509_get_other_name( name_buf, &other_name );
		if (ret)
			return ret;

		memset(name, 0, sizeof(*name));
		name->type = LWS_MBEDTLS_X509_SAN_OTHER_NAME;
		memcpy(&name->name.other_name, &other_name, sizeof(other_name));
		return 0;
#endif
	case MBEDTLS_ASN1_SEQUENCE | LWS_MBEDTLS_X509_SAN_RFC822_NAME:

		bufferPointer = name_buf->MBEDTLS_PRIVATE_V30_ONLY(p);
		p = &bufferPointer;
		end = name_buf->MBEDTLS_PRIVATE_V30_ONLY(p) +
		      name_buf->MBEDTLS_PRIVATE_V30_ONLY(len);

		/* The leading ASN1 tag and length has been processed.
		 * Stepping back with 2 bytes, because mbedtls_x509_get_name
		 * expects the beginning of the SET tag */
		*p = *p - 2;

		rfc822Name.MBEDTLS_PRIVATE_V30_ONLY(next) = NULL;
		ret = mbedtls_x509_get_name( p, end, &rfc822Name );
		if (ret) {
			lws_x509_clean_name(&rfc822Name);
			return ret;
		}

		memset(name, 0, sizeof(*name));
		name->type = LWS_MBEDTLS_X509_SAN_OTHER_NAME;
		memcpy(&name->san.other_name,
		       &rfc822Name, sizeof(rfc822Name));
		return 0;

	case MBEDTLS_ASN1_CONTEXT_SPECIFIC | LWS_MBEDTLS_X509_SAN_DNS_NAME:
		memset(name, 0, sizeof(*name));
		name->type = LWS_MBEDTLS_X509_SAN_DNS_NAME;

		memcpy(&name->san.unstructured_name,
		       name_buf, sizeof(*name_buf) );
		return 0;

	default:
		return 1;
	}

	return 1;
}

static int
lws_x509_get_general_names(uint8_t **p, const uint8_t *end,
			   mbedtls_x509_sequence *name )
{
	mbedtls_asn1_sequence *cur = name;
	mbedtls_asn1_buf *buf;
	size_t len, tag_len;
	unsigned char tag;
	int r;

	/* Get main sequence tag */
	r = mbedtls_asn1_get_tag(p, end, &len, MBEDTLS_ASN1_CONSTRUCTED |
					       MBEDTLS_ASN1_SEQUENCE);
	if (r)
		return r;

	if (*p + len != end)
		return 1;

	while (*p < end) {
		lws_mbedtls_x509_subject_alternative_name dnb;
		memset(&dnb, 0, sizeof(dnb));

		tag = **p;
		(*p)++;

		r = mbedtls_asn1_get_len(p, end, &tag_len);
		if (r)
		    return r;

		/* Tag shall be CONTEXT_SPECIFIC or SET */
		if ((tag & LWS_MBEDTLS_ASN1_TAG_CLASS_MASK) !=
					        MBEDTLS_ASN1_CONTEXT_SPECIFIC &&
		    (tag & (MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE)) !=
			   (MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE))
			return 1;

		/*
		 * Check that the name is structured correctly.
		 */
		r = lws_mbedtls_x509_parse_general_name(
					&cur->MBEDTLS_PRIVATE_V30_ONLY(buf), &dnb);
		/*
		 * In case the extension is malformed, return an error,
		 * and clear the allocated sequences.
		 */
		if (r && r != MBEDTLS_ERR_X509_FEATURE_UNAVAILABLE) {
		    mbedtls_x509_sequence *seq_cur = name->MBEDTLS_PRIVATE_V30_ONLY(next);
		    mbedtls_x509_sequence *seq_prv;

			while( seq_cur != NULL ) {
				seq_prv = seq_cur;
				seq_cur = seq_cur->MBEDTLS_PRIVATE_V30_ONLY(next);
				lws_explicit_bzero(seq_prv, sizeof(*seq_cur));
				lws_free(seq_prv);
			}

			name->MBEDTLS_PRIVATE_V30_ONLY(next) = NULL;

			return r;
		}

		/* Allocate and assign next pointer */
		if (cur->MBEDTLS_PRIVATE_V30_ONLY(buf).MBEDTLS_PRIVATE_V30_ONLY(p)) {
			if (cur->MBEDTLS_PRIVATE_V30_ONLY(next))
				return 1;

			cur->MBEDTLS_PRIVATE_V30_ONLY(next) =
					lws_zalloc(sizeof(*cur), __func__);

			if (!cur->MBEDTLS_PRIVATE_V30_ONLY(next))
				return 1;

			cur = cur->MBEDTLS_PRIVATE_V30_ONLY(next);
		}

		buf = &(cur->MBEDTLS_PRIVATE_V30_ONLY(buf));
		buf->MBEDTLS_PRIVATE_V30_ONLY(tag) = tag;
		buf->MBEDTLS_PRIVATE_V30_ONLY(p) = *p;
		buf->MBEDTLS_PRIVATE_V30_ONLY(len) = tag_len;

		*p += buf->MBEDTLS_PRIVATE_V30_ONLY(len);
	}

	/* Set final sequence entry's next pointer to NULL */
	cur->MBEDTLS_PRIVATE_V30_ONLY(next) = NULL;

	return *p != end;
}

static int
x509_get_akid(uint8_t **p, uint8_t *end, lws_mbedtls_x509_authority *akid)
{
	size_t len = 0u;
	int r;

	r = mbedtls_asn1_get_tag(p, end, &len, MBEDTLS_ASN1_CONSTRUCTED |
						 MBEDTLS_ASN1_SEQUENCE);
	if (r)
		return r;

	r = mbedtls_asn1_get_tag(p, end, &len, MBEDTLS_ASN1_CONTEXT_SPECIFIC);
	if (!r) {
		akid->keyIdentifier.MBEDTLS_PRIVATE_V30_ONLY(len) = len;
		akid->keyIdentifier.MBEDTLS_PRIVATE_V30_ONLY(p) = *p;
		akid->keyIdentifier.MBEDTLS_PRIVATE_V30_ONLY(tag) = MBEDTLS_ASN1_OCTET_STRING;

		*p += len;
	}

	if (*p < end) {
		/* Getting authorityCertIssuer using the required specific
		 * class tag [1] */
		r = mbedtls_asn1_get_tag(p, end, &len,
					   MBEDTLS_ASN1_CONTEXT_SPECIFIC |
					   MBEDTLS_ASN1_CONSTRUCTED | 1 );
		if (!r) {
			/* Getting directoryName using the required specific
			 * class tag [4] */
			r = mbedtls_asn1_get_tag(p, end, &len,
						 MBEDTLS_ASN1_CONTEXT_SPECIFIC |
						 MBEDTLS_ASN1_CONSTRUCTED | 4);
			if (r)
				return(r);

			/* "end" also includes the CertSerialNumber field
			 * so "len" shall be used */
			r = lws_x509_get_general_names(p, (*p + len),
						    &akid->authorityCertIssuer);
		}
	}

	if (*p < end) {
		r = mbedtls_asn1_get_tag(p, end, &len,
					   MBEDTLS_ASN1_CONTEXT_SPECIFIC |
					   MBEDTLS_ASN1_INTEGER );
		if (r)
			return r;

		akid->authorityCertSerialNumber.MBEDTLS_PRIVATE_V30_ONLY(len) = len;
		akid->authorityCertSerialNumber.MBEDTLS_PRIVATE_V30_ONLY(p) = *p;
		akid->authorityCertSerialNumber.MBEDTLS_PRIVATE_V30_ONLY(tag) = MBEDTLS_ASN1_OCTET_STRING;
		*p += len;
	}

	return *p != end;
}

/*
 * Work around lack of this in mbedtls... we don't need to do sanity checks
 * sanity checks because they will be done at x509 validation time
 */

int
lws_x509_get_crt_ext(mbedtls_x509_crt *crt, mbedtls_x509_buf *skid,
		     lws_mbedtls_x509_authority *akid)
{
	uint8_t *p = crt->MBEDTLS_PRIVATE_V30_ONLY(v3_ext).MBEDTLS_PRIVATE_V30_ONLY(p),
					*end_ext_data, *end_ext_octet;
	const uint8_t *end = p + crt->MBEDTLS_PRIVATE_V30_ONLY(v3_ext).MBEDTLS_PRIVATE_V30_ONLY(len);
	size_t len;
	int r = mbedtls_asn1_get_tag(&p, end, &len, MBEDTLS_ASN1_CONSTRUCTED |
						      MBEDTLS_ASN1_SEQUENCE);
	if (r)
		return r;

	while (p < end) {
		mbedtls_x509_buf extn_oid = { 0, 0, NULL };
		int is_critical = 0; /* DEFAULT FALSE */
		int ext_type = 0;

		r = mbedtls_asn1_get_tag(&p, end, &len,
					   MBEDTLS_ASN1_CONSTRUCTED |
					   MBEDTLS_ASN1_SEQUENCE);
		if (r)
			return r;

		end_ext_data = p + len;

		/* Get extension ID */
		r = mbedtls_asn1_get_tag(&p, end_ext_data, &extn_oid.MBEDTLS_PRIVATE_V30_ONLY(len),
					   MBEDTLS_ASN1_OID);
		if (r)
			return r;

		extn_oid.MBEDTLS_PRIVATE_V30_ONLY(tag) = MBEDTLS_ASN1_OID;
		extn_oid.MBEDTLS_PRIVATE_V30_ONLY(p) = p;
		p += extn_oid.MBEDTLS_PRIVATE_V30_ONLY(len);

		/* Get optional critical */
		r = mbedtls_asn1_get_bool(&p, end_ext_data, &is_critical);
		if (r && r != MBEDTLS_ERR_ASN1_UNEXPECTED_TAG)
			return r;

		/* Data should be octet string type */
		r = mbedtls_asn1_get_tag(&p, end_ext_data, &len,
					   MBEDTLS_ASN1_OCTET_STRING);
		if (r)
			return r;

		end_ext_octet = p + len;

		if (end_ext_octet != end_ext_data)
			return 1;

		r = lws_mbedtls_oid_get_x509_ext_type(&extn_oid, &ext_type);
		if (r) {
			p = end_ext_octet;
			continue;
		}

		switch (ext_type) {
		case LWS_MBEDTLS_X509_EXT_SUBJECT_KEY_IDENTIFIER:
			/* Parse subject key identifier */
			r = x509_get_skid(&p, end_ext_data, skid);
			if (r)
				return r;
			break;

		case LWS_MBEDTLS_X509_EXT_AUTHORITY_KEY_IDENTIFIER:
			/* Parse authority key identifier */
			r = x509_get_akid(&p, end_ext_octet, akid);
			if (r)
				return r;
			break;

		default:
			p = end_ext_octet;
		}
	}

	return 0;
}
