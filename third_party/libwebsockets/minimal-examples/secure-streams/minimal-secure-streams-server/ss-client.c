/*
 * lws-minimal-secure-streams-server
 *
 * Written in 2010-2020 by Andy Green <andy@warmcat.com>
 *
 * This file is made available under the Creative Commons CC0 1.0
 * Universal Public Domain Dedication.
 */

#include <libwebsockets.h>

extern int interrupted, bad;

typedef struct myss {
	struct lws_ss_handle 	*ss;
	void			*opaque_data;
	/* ... application specific state ... */
	lws_sorted_usec_list_t	sul;

	int			count;
} myss_t;

/* secure streams payload interface */

static lws_ss_state_return_t
myss_rx(void *userobj, const uint8_t *buf, size_t len, int flags)
{
//	myss_t *m = (myss_t *)userobj;

	lwsl_user("%s: len %d, flags: %d\n", __func__, (int)len, flags);
	lwsl_hexdump_info(buf, len);

	/*
	 * If we received the whole message, for our example it means
	 * we are done.
	 */
	if (flags & LWSSS_FLAG_EOM) {
		bad = 0;
		interrupted = 1;
	}

	return 0;
}

static lws_ss_state_return_t
myss_tx(void *userobj, lws_ss_tx_ordinal_t ord, uint8_t *buf, size_t *len,
	int *flags)
{
	//myss_t *m = (myss_t *)userobj;

	return LWSSSSRET_TX_DONT_SEND; /* don't want to write */
}

static lws_ss_state_return_t
myss_state(void *userobj, void *sh, lws_ss_constate_t state,
	   lws_ss_tx_ordinal_t ack)
{
	myss_t *m = (myss_t *)userobj;

	lwsl_user("%s: %p %s, ord 0x%x\n", __func__, m->ss,
		  lws_ss_state_name((int)state), (unsigned int)ack);

	switch (state) {
	case LWSSSCS_CREATING:
		return lws_ss_request_tx(m->ss);
		break;
	case LWSSSCS_ALL_RETRIES_FAILED:
		/* if we're out of retries, we want to close the app and FAIL */
		interrupted = 1;
		break;
	default:
		break;
	}

	return 0;
}

const lws_ss_info_t ssi_client = {
	.handle_offset			= offsetof(myss_t, ss),
	.opaque_user_data_offset	= offsetof(myss_t, opaque_data),
	.streamtype			= "mintest",
	.rx				= myss_rx,
	.tx				= myss_tx,
	.state				= myss_state,
	.user_alloc			= sizeof(myss_t),
};
