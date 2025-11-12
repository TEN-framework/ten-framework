/*
 * ws protocol handler plugin for "lws-minimal"
 *
 * Written in 2010-2019 by Andy Green <andy@warmcat.com>
 *
 * This file is made available under the Creative Commons CC0 1.0
 * Universal Public Domain Dedication.
 *
 * This version uses an lws_ring ringbuffer to cache up to 8 messages at a time,
 * so it's not so easy to lose messages.
 */

#if !defined (LWS_PLUGIN_STATIC)
#define LWS_DLL
#define LWS_INTERNAL
#include <libwebsockets.h>
#endif

#include <string.h>
#include <stdlib.h>

/* one of these created for each message */

struct msg {
	void *payload; /* is malloc'd */
	size_t len;
};

/* one of these is created for each client connecting to us */

struct per_session_data__minimal {
	struct per_session_data__minimal *pss_list;
	struct lws *wsi;
	uint32_t tail;
};

/* one of these is created for each vhost our protocol is used with */

struct per_vhost_data__minimal {
	struct lws_context *context;
	struct lws_vhost *vhost;
	const struct lws_protocols *protocol;

	lws_sorted_usec_list_t sul;

	struct per_session_data__minimal *pss_list; /* linked-list of live pss*/

	struct lws_ring *ring; /* ringbuffer holding unsent messages */
	struct lws_client_connect_info i;
	struct lws *client_wsi;
};

/* destroys the message when everyone has had a copy of it */

static void
__minimal_destroy_message(void *_msg)
{
	struct msg *msg = _msg;

	free(msg->payload);
	msg->payload = NULL;
	msg->len = 0;
}

static void
sul_connect_attempt(struct lws_sorted_usec_list *sul)
{
	struct per_vhost_data__minimal *vhd =
		lws_container_of(sul, struct per_vhost_data__minimal, sul);

	vhd->i.context = vhd->context;
	vhd->i.port = 443;
	vhd->i.address = "libwebsockets.org";
	vhd->i.path = "/";
	vhd->i.host = vhd->i.address;
	vhd->i.origin = vhd->i.address;
	vhd->i.ssl_connection = 1;

	vhd->i.protocol = "dumb-increment-protocol";
	vhd->i.local_protocol_name = "lws-minimal-proxy";
	vhd->i.pwsi = &vhd->client_wsi;

	if (!lws_client_connect_via_info(&vhd->i))
		lws_sul_schedule(vhd->context, 0, &vhd->sul,
				 sul_connect_attempt, 10 * LWS_US_PER_SEC);
}

static int
callback_minimal(struct lws *wsi, enum lws_callback_reasons reason,
			void *user, void *in, size_t len)
{
	struct per_session_data__minimal *pss =
			(struct per_session_data__minimal *)user;
	struct per_vhost_data__minimal *vhd =
			(struct per_vhost_data__minimal *)
			lws_protocol_vh_priv_get(lws_get_vhost(wsi),
					lws_get_protocol(wsi));
	const struct msg *pmsg;
	struct msg amsg;
	int m;

	switch (reason) {

	/* --- protocol lifecycle callbacks --- */

	case LWS_CALLBACK_PROTOCOL_INIT:
		vhd = lws_protocol_vh_priv_zalloc(lws_get_vhost(wsi),
				lws_get_protocol(wsi),
				sizeof(struct per_vhost_data__minimal));
		vhd->context = lws_get_context(wsi);
		vhd->protocol = lws_get_protocol(wsi);
		vhd->vhost = lws_get_vhost(wsi);

		vhd->ring = lws_ring_create(sizeof(struct msg), 8,
					    __minimal_destroy_message);
		if (!vhd->ring)
			return 1;

		sul_connect_attempt(&vhd->sul);
		break;

	case LWS_CALLBACK_PROTOCOL_DESTROY:
		lws_ring_destroy(vhd->ring);
		lws_sul_cancel(&vhd->sul);
		break;

	/* --- serving callbacks --- */

	case LWS_CALLBACK_ESTABLISHED:
		/* add ourselves to the list of live pss held in the vhd */
		lws_ll_fwd_insert(pss, pss_list, vhd->pss_list);
		pss->tail = lws_ring_get_oldest_tail(vhd->ring);
		pss->wsi = wsi;
		break;

	case LWS_CALLBACK_CLOSED:
		/* remove our closing pss from the list of live pss */
		lws_ll_fwd_remove(struct per_session_data__minimal, pss_list,
				  pss, vhd->pss_list);
		break;

	case LWS_CALLBACK_SERVER_WRITEABLE:
		pmsg = lws_ring_get_element(vhd->ring, &pss->tail);
		if (!pmsg)
			break;

		/* notice we allowed for LWS_PRE in the payload already */
		m = lws_write(wsi, ((unsigned char *)pmsg->payload) + LWS_PRE,
			      pmsg->len, LWS_WRITE_TEXT);
		if (m < (int)pmsg->len) {
			lwsl_err("ERROR %d writing to ws socket\n", m);
			return -1;
		}

		lws_ring_consume_and_update_oldest_tail(
			vhd->ring,	/* lws_ring object */
			struct per_session_data__minimal, /* type of objects with tails */
			&pss->tail,	/* tail of guy doing the consuming */
			1,		/* number of payload objects being consumed */
			vhd->pss_list,	/* head of list of objects with tails */
			tail,		/* member name of tail in objects with tails */
			pss_list	/* member name of next object in objects with tails */
		);

		/* more to do? */
		if (lws_ring_get_element(vhd->ring, &pss->tail))
			/* come back as soon as we can write more */
			lws_callback_on_writable(pss->wsi);
		break;

	/* --- client callbacks --- */

	case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
		lwsl_err("CLIENT_CONNECTION_ERROR: %s\n",
			 in ? (char *)in : "(null)");
		vhd->client_wsi = NULL;
		lws_sul_schedule(vhd->context, 0, &vhd->sul,
				 sul_connect_attempt, LWS_US_PER_SEC);
		break;

	case LWS_CALLBACK_CLIENT_ESTABLISHED:
		lwsl_user("%s: established\n", __func__);
		break;

	case LWS_CALLBACK_CLIENT_RECEIVE:
		/* if no clients, just drop incoming */
		if (!vhd->pss_list)
			break;

		if (!lws_ring_get_count_free_elements(vhd->ring)) {
			lwsl_user("dropping!\n");
			break;
		}

		amsg.len = len;
		/* notice we over-allocate by LWS_PRE */
		amsg.payload = malloc(LWS_PRE + len);
		if (!amsg.payload) {
			lwsl_user("OOM: dropping\n");
			break;
		}

		memcpy((char *)amsg.payload + LWS_PRE, in, len);
		if (!lws_ring_insert(vhd->ring, &amsg, 1)) {
			__minimal_destroy_message(&amsg);
			lwsl_user("dropping!\n");
			break;
		}

		/*
		 * let everybody know we want to write something on them
		 * as soon as they are ready
		 */
		lws_start_foreach_llp(struct per_session_data__minimal **,
				      ppss, vhd->pss_list) {
			lws_callback_on_writable((*ppss)->wsi);
		} lws_end_foreach_llp(ppss, pss_list);
		break;

	case LWS_CALLBACK_CLIENT_CLOSED:
		vhd->client_wsi = NULL;
		lws_sul_schedule(vhd->context, 0, &vhd->sul,
				 sul_connect_attempt, LWS_US_PER_SEC);
		break;

	default:
		break;
	}

	return 0;
}

#define LWS_PLUGIN_PROTOCOL_MINIMAL \
	{  \
		"lws-minimal-proxy", \
		callback_minimal, \
		sizeof(struct per_session_data__minimal), \
		128, \
		0, NULL, 0 \
	}
