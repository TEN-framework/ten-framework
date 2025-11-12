/*
 * lws-minimal-secure-streams-server
 *
 * Written in 2010-2020 by Andy Green <andy@warmcat.com>
 *
 * This file is made available under the Creative Commons CC0 1.0
 * Universal Public Domain Dedication.
 */

#include <libwebsockets.h>
#include <assert.h>

extern int interrupted, bad;

typedef struct myss {
	struct lws_ss_handle 		*ss;
	void				*opaque_data;
	/* ... application specific state ... */

	lws_sorted_usec_list_t		sul;
	int				count;
	char				upgraded;

} myss_srv_t;

/*
 * This is the Secure Streams Server RX and TX
 */

static lws_ss_state_return_t
myss_raw_rx(void *userobj, const uint8_t *buf, size_t len, int flags)
{
//	myss_srv_t *m = (myss_srv_t *)userobj;

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

/* this is the callback that mediates sending the incrementing number */

static void
spam_sul_cb(struct lws_sorted_usec_list *sul)
{
	myss_srv_t *m = lws_container_of(sul, myss_srv_t, sul);

	if (!lws_ss_request_tx(m->ss))
		lws_sul_schedule(lws_ss_get_context(m->ss), 0, &m->sul, spam_sul_cb,
			 100 * LWS_US_PER_MS);
}

static lws_ss_state_return_t
myss_raw_tx(void *userobj, lws_ss_tx_ordinal_t ord, uint8_t *buf, size_t *len,
	int *flags)
{
	myss_srv_t *m = (myss_srv_t *)userobj;

	*flags = LWSSS_FLAG_SOM | LWSSS_FLAG_EOM;

	*len = (unsigned int)lws_snprintf((char *)buf, *len, "hello from raw %d\n", m->count++);

	lws_sul_schedule(lws_ss_get_context(m->ss), 0, &m->sul, spam_sul_cb,
			 100 * LWS_US_PER_MS);

	return 0;
}

static lws_ss_state_return_t
myss_raw_state(void *userobj, void *sh, lws_ss_constate_t state,
	   lws_ss_tx_ordinal_t ack)
{
	myss_srv_t *m = (myss_srv_t *)userobj;

	lwsl_user("%s: %p %s, ord 0x%x\n", __func__, m->ss,
		  lws_ss_state_name((int)state), (unsigned int)ack);

	switch (state) {
	case LWSSSCS_DISCONNECTED:
		lws_sul_cancel(&m->sul);
		break;
	case LWSSSCS_CONNECTED:
		return lws_ss_request_tx(m->ss);

	default:
		break;
	}

	return 0;
}

const lws_ss_info_t ssi_server = {
	.handle_offset			= offsetof(myss_srv_t, ss),
	.opaque_user_data_offset	= offsetof(myss_srv_t, opaque_data),
	.streamtype			= "myrawserver",
	.rx				= myss_raw_rx,
	.tx				= myss_raw_tx,
	.state				= myss_raw_state,
	.user_alloc			= sizeof(myss_srv_t),
};
