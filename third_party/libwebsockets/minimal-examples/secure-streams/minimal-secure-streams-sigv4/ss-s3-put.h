/*
 * S3 Put Object via Secure Streams minimal sigv4 example
 *
 * Written in 2010-2020 by Andy Green <andy@warmcat.com>
 *			   Amit Pachore <apachor@amazon.com>
 *			   securestreams-dev@amazon.com
 *
 * This file is made available under the Creative Commons CC0 1.0
 * Universal Public Domain Dedication.
 */

typedef struct ss_s3_put {
	struct lws_ss_handle	*ss;
	void			*opaque_data;

	/* ... application specific state ... */

	size_t			total;
	size_t			pos;
	uint8_t			*buf;
} ss_s3_put_t;
