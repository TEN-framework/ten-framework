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

#ifndef __LWS_OPTEE_H
#define __LWS_OPTEE_H

/* 128-bit IP6 address */
struct in6_addr {
	union {
		uint8_t	  u6_addr8[16];
		uint16_t  u6_addr16[8];
		uint32_t  u6_addr32[4];
	};
};

#define		_SS_MAXSIZE	128U
#define		_SS_ALIGNSIZE	(sizeof(int64_t))
#define		_SS_PAD1SIZE	(_SS_ALIGNSIZE - \
			sizeof(sa_family_t))
#define		_SS_PAD2SIZE	(_SS_MAXSIZE - \
			sizeof(sa_family_t) - _SS_PAD1SIZE - _SS_ALIGNSIZE)

struct sockaddr_storage {
	sa_family_t	ss_family;	/* address family */
	char		__ss_pad1[_SS_PAD1SIZE];
	int64_t		__ss_align;	/* force desired struct alignment */
	char		__ss_pad2[_SS_PAD2SIZE];
};

#define __SOCK_SIZE__	16		/* sizeof(struct sockaddr)      */
struct sockaddr {
	sa_family_t	sa_family;	/* address family */
	uint8_t		sa_data[__SOCK_SIZE__	/* address value */
				- sizeof(sa_family_t)];
};

/* 16 bytes */
struct sockaddr_in {
	sa_family_t	sin_family;
	in_port_t	sin_port;
	struct in_addr	sin_addr;
	uint8_t		sin_zero[__SOCK_SIZE__	/* padding until 16 bytes */
				- sizeof(sa_family_t)
				- sizeof(in_port_t)
				- sizeof(struct  in_addr)];
};

struct sockaddr_in6 {
	sa_family_t	sin6_family;	/* AF_INET6 */
	in_port_t	sin6_port;	/* Transport layer port # */
	uint32_t	sin6_flowinfo;	/* IP6 flow information */
	struct in6_addr	sin6_addr;	/* IP6 address */
	uint32_t	sin6_scope_id;	/* scope zone index */
};

#endif /* __LWS_OPTEE_H */
