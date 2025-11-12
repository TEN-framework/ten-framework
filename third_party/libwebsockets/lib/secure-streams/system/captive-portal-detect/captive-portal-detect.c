/*
 * Captive portal detect for Secure Streams
 *
 * libwebsockets - small server side websockets and web server implementation
 *
 * Copyright (C) 2019 - 2020 Andy Green <andy@warmcat.com>
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

#include <private-lib-core.h>

typedef struct ss_cpd {
	struct lws_ss_handle 	*ss;
	void			*opaque_data;
	/* ... application specific state ... */

	lws_sorted_usec_list_t	sul;
} ss_cpd_t;

static lws_ss_state_return_t
ss_cpd_state(void *userobj, void *sh, lws_ss_constate_t state,
	     lws_ss_tx_ordinal_t ack)
{
	ss_cpd_t *m = (ss_cpd_t *)userobj;
	struct lws_context *cx = (struct lws_context *)m->opaque_data;

	lwsl_ss_info(m->ss, "%s, ord 0x%x\n", lws_ss_state_name((int)state),
		  (unsigned int)ack);

	switch (state) {
	case LWSSSCS_CREATING:
		lws_ss_start_timeout(m->ss, 3 * LWS_US_PER_SEC);
		return lws_ss_request_tx(m->ss);

	case LWSSSCS_QOS_ACK_REMOTE:
		lws_system_cpd_set(cx, LWS_CPD_INTERNET_OK);
		cx->ss_cpd = NULL;
		return LWSSSSRET_DESTROY_ME;

	case LWSSSCS_TIMEOUT:
	case LWSSSCS_ALL_RETRIES_FAILED:
	case LWSSSCS_DISCONNECTED:
		/*
		 * First result reported sticks... if nothing else, this will
		 * cover the situation we didn't connect to anything
		 */
		lws_system_cpd_set(cx, LWS_CPD_NO_INTERNET);
		cx->ss_cpd = NULL;
		return LWSSSSRET_DESTROY_ME;

	default:
		break;
	}

	return LWSSSSRET_OK;
}

static const lws_ss_info_t ssi_cpd = {
	.handle_offset			= offsetof(ss_cpd_t, ss),
	.opaque_user_data_offset	= offsetof(ss_cpd_t, opaque_data),
	.state				= ss_cpd_state,
	.user_alloc			= sizeof(ss_cpd_t),
	.streamtype			= "captive_portal_detect",
};

int
lws_ss_sys_cpd(struct lws_context *cx)
{
	if (cx->ss_cpd) {
		lwsl_cx_notice(cx, "CPD already ongoing");
		return 0;
	}

	if (lws_ss_create(cx, 0, &ssi_cpd, cx, &cx->ss_cpd, NULL, NULL)) {
		lwsl_cx_info(cx, "Create stream failed (policy?)");

		return 1;
	}

	return 0;
}
