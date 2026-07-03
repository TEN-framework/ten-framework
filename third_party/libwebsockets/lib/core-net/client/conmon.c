/*
 * libwebsockets - small server side websockets and web server implementation
 *
 * Copyright (C) 2019 - 2021 Andy Green <andy@warmcat.com>
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
 * Client Connection Latency and DNS reporting
 */

/*
 * We want to allocate copies for and append DNS results that we don't already
 * have.  We take this approach because a) we may be getting duplicated results
 * from multiple DNS servers, and b) we may be getting results stacatto over
 * time.
 *
 * We capture DNS results from either getaddrinfo or ASYNC_DNS the same here,
 * before they are sorted and filtered.
 *
 * Because this is relatively expensive, we only do it on client wsi that
 * explicitly indicated that they want it with the LCCSCF_CONMON flag.
 */

#include <private-lib-core.h>

int
lws_conmon_append_copy_new_dns_results(struct lws *wsi,
				       const struct addrinfo *cai)
{
	if (!(wsi->flags & LCCSCF_CONMON))
		return 0;

	/*
	 * Let's go through the incoming guys, seeing if we already have them,
	 * or if we want to take a copy
	 */

	while (cai) {
		struct addrinfo *ai = wsi->conmon.dns_results_copy;
		char skip = 0;

		/* do we already have this guy? */

		while (ai) {

			if (ai->ai_family != cai->ai_family &&
			    ai->ai_addrlen != cai->ai_addrlen &&
			    ai->ai_protocol != cai->ai_protocol &&
			    ai->ai_socktype != cai->ai_socktype &&
			    /* either ipv4 or v6 address must match */
			    ((ai->ai_family == AF_INET &&
			      ((struct sockaddr_in *)ai->ai_addr)->
							     sin_addr.s_addr ==
			      ((struct sockaddr_in *)cai->ai_addr)->
							     sin_addr.s_addr)
#if defined(LWS_WITH_IPV6)
					    ||
			    (ai->ai_family == AF_INET6 &&
			     !memcmp(((struct sockaddr_in6 *)ai->ai_addr)->
							     sin6_addr.s6_addr,
				     ((struct sockaddr_in6 *)cai->ai_addr)->
						     sin6_addr.s6_addr, 16))
#endif
			    )) {
				/* yes, we already got a copy then */
				skip = 1;
				break;
			}

			ai = ai->ai_next;
		}

		if (!skip) {
			/*
			 * No we don't already have a copy of this one, let's
			 * allocate and append it then
			 */
			size_t al = sizeof(struct addrinfo) +
				    (size_t)cai->ai_addrlen;
			size_t cl = cai->ai_canonname ?
					strlen(cai->ai_canonname) + 1 : 0;

			ai = lws_malloc(al + cl + 1, __func__);
			if (!ai) {
				lwsl_wsi_warn(wsi, "OOM");
				return 1;
			}
			*ai = *cai;
			ai->ai_addr = (struct sockaddr *)&ai[1];
			memcpy(ai->ai_addr, cai->ai_addr, (size_t)cai->ai_addrlen);

			if (cl) {
				ai->ai_canonname = ((char *)ai->ai_addr) +
							cai->ai_addrlen;
				memcpy(ai->ai_canonname, cai->ai_canonname,
				       cl + 1);
			}
			ai->ai_next = wsi->conmon.dns_results_copy;
			wsi->conmon.dns_results_copy = ai;
		}

		cai = cai->ai_next;
	}

	return 0;
}

void
lws_conmon_addrinfo_destroy(struct addrinfo *ai)
{
	while (ai) {
		struct addrinfo *ai1 = ai->ai_next;

		lws_free(ai);
		ai = ai1;
	}
}

void
lws_conmon_wsi_take(struct lws *wsi, struct lws_conmon *dest)
{
	memcpy(dest, &wsi->conmon, sizeof(*dest));
	dest->peer46 = wsi->sa46_peer;

	/* wsi no longer has to free it... */
	wsi->conmon.dns_results_copy = NULL;
	wsi->perf_done = 1;
}

void
lws_conmon_release(struct lws_conmon *conmon)
{
	if (!conmon)
		return;

	lws_conmon_addrinfo_destroy(conmon->dns_results_copy);
	conmon->dns_results_copy = NULL;
}
