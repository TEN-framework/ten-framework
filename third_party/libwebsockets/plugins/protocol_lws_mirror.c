/*
 * libwebsockets-test-server - libwebsockets test implementation
 *
 * Written in 2010-2019 by Andy Green <andy@warmcat.com>
 *
 * This file is made available under the Creative Commons CC0 1.0
 * Universal Public Domain Dedication.
 *
 * The person who associated a work with this deed has dedicated
 * the work to the public domain by waiving all of his or her rights
 * to the work worldwide under copyright law, including all related
 * and neighboring rights, to the extent allowed by law. You can copy,
 * modify, distribute and perform the work, even for commercial purposes,
 * all without asking permission.
 *
 * The test apps are intended to be adapted for use in your code, which
 * may be proprietary.  So unlike the library itself, they are licensed
 * Public Domain.
 *
 * Notice that the lws_pthread... locking apis are all zero-footprint
 * NOPs in the case LWS_MAX_SMP == 1, which is the default.  When lws
 * is built for multiple service threads though, they resolve to their
 * pthreads equivalents.
 */

#if !defined (LWS_PLUGIN_STATIC)
#if !defined(LWS_DLL)
#define LWS_DLL
#endif
#if !defined(LWS_INTERNAL)
#define LWS_INTERNAL
#endif
#include <libwebsockets.h>
#endif

#include <string.h>
#include <stdlib.h>

#define QUEUELEN 32
/* queue free space below this, rx flow is disabled */
#define RXFLOW_MIN (4)
/* queue free space above this, rx flow is enabled */
#define RXFLOW_MAX ((2 * QUEUELEN) / 3)

#define MAX_MIRROR_INSTANCES 3

struct mirror_instance;

struct per_session_data__lws_mirror {
	struct lws *wsi;
	struct mirror_instance *mi;
	struct per_session_data__lws_mirror *same_mi_pss_list;
	uint32_t tail;
};

/* this is the element in the ring */
struct a_message {
	void *payload;
	size_t len;
};

struct mirror_instance {
	struct mirror_instance *next;
	lws_pthread_mutex(lock) /* protects all mirror instance data */
	struct per_session_data__lws_mirror *same_mi_pss_list;
	/**< must hold the the per_vhost_data__lws_mirror.lock as well
	 * to change mi list membership */
	struct lws_ring *ring;
	int messages_allocated;
	char name[30];
	char rx_enabled;
};

struct per_vhost_data__lws_mirror {
	lws_pthread_mutex(lock) /* protects mi_list membership changes */
	struct mirror_instance *mi_list;
};


/* enable or disable rx from all connections to this mirror instance */
static void
__mirror_rxflow_instance(struct mirror_instance *mi, int enable)
{
	lws_start_foreach_ll(struct per_session_data__lws_mirror *,
			     pss, mi->same_mi_pss_list) {
		lws_rx_flow_control(pss->wsi, enable);
	} lws_end_foreach_ll(pss, same_mi_pss_list);

	mi->rx_enabled = (char)enable;
}

/*
 * Find out which connection to this mirror instance has the longest number
 * of still unread elements in the ringbuffer and update the lws_ring "oldest
 * tail" with it.  Elements behind the "oldest tail" are freed and recycled for
 * new head content.  Elements after the "oldest tail" are still waiting to be
 * read by somebody.
 *
 * If the oldest tail moved on from before, check if it created enough space
 * in the queue to re-enable RX flow control for the mirror instance.
 *
 * Mark connections that are at the oldest tail as being on a 3s timeout to
 * transmit something, otherwise the connection will be closed.  Without this,
 * a choked or nonresponsive connection can block the FIFO from freeing up any
 * new space for new data.
 *
 * You can skip calling this if on your connection, before processing, the tail
 * was not equal to the current worst, ie,  if the tail you will work on is !=
 * lws_ring_get_oldest_tail(ring) then no need to call this when the tail
 * has changed; it wasn't the oldest so it won't change the oldest.
 *
 * Returns 0 if oldest unchanged or 1 if oldest changed from this call.
 */
static int
__mirror_update_worst_tail(struct mirror_instance *mi)
{
	uint32_t wai, worst = 0, worst_tail = 0, oldest;
	struct per_session_data__lws_mirror *worst_pss = NULL;

	oldest = lws_ring_get_oldest_tail(mi->ring);

	lws_start_foreach_ll(struct per_session_data__lws_mirror *,
			     pss, mi->same_mi_pss_list) {
		wai = (uint32_t)lws_ring_get_count_waiting_elements(mi->ring,
								&pss->tail);
		if (wai >= worst) {
			worst = wai;
			worst_tail = pss->tail;
			worst_pss = pss;
		}
	} lws_end_foreach_ll(pss, same_mi_pss_list);

	if (!worst_pss)
		return 0;

	lws_ring_update_oldest_tail(mi->ring, worst_tail);
	if (oldest == lws_ring_get_oldest_tail(mi->ring))
		return 0;
	/*
	 * The oldest tail did move on.  Check if we should re-enable rx flow
	 * for the mirror instance since we made some space now.
	 */
	if (!mi->rx_enabled && /* rx is disabled */
	    lws_ring_get_count_free_elements(mi->ring) >= RXFLOW_MAX)
		/* there is enough space, let's re-enable rx for our instance */
		__mirror_rxflow_instance(mi, 1);

	/* if nothing in queue, no timeout needed */
	if (!worst)
		return 1;

	/*
	 * The guy(s) with the oldest tail block the ringbuffer from recycling
	 * the FIFO entries he has not read yet.  Don't allow those guys to
	 * block the FIFO operation for very long.
	 */
	lws_start_foreach_ll(struct per_session_data__lws_mirror *,
			     pss, mi->same_mi_pss_list) {
		if (pss->tail == worst_tail)
			/*
			 * Our policy is if you are the slowest connection,
			 * you had better transmit something to help with that
			 * within 3s, or we will hang up on you to stop you
			 * blocking the FIFO for everyone else.
			 */
			lws_set_timeout(pss->wsi,
					PENDING_TIMEOUT_USER_REASON_BASE, 3);
	} lws_end_foreach_ll(pss, same_mi_pss_list);

	return 1;
}

static void
__mirror_callback_all_in_mi_on_writable(struct mirror_instance *mi)
{
	/* ask for WRITABLE callback for every wsi on this mi */
	lws_start_foreach_ll(struct per_session_data__lws_mirror *,
			     pss, mi->same_mi_pss_list) {
		lws_callback_on_writable(pss->wsi);
	} lws_end_foreach_ll(pss, same_mi_pss_list);
}

static void
__mirror_destroy_message(void *_msg)
{
	struct a_message *msg = _msg;

	free(msg->payload);
	msg->payload = NULL;
	msg->len = 0;
}

static int
callback_lws_mirror(struct lws *wsi, enum lws_callback_reasons reason,
		    void *user, void *in, size_t len)
{
	struct per_session_data__lws_mirror *pss =
			(struct per_session_data__lws_mirror *)user;
	struct per_vhost_data__lws_mirror *v =
			(struct per_vhost_data__lws_mirror *)
			lws_protocol_vh_priv_get(lws_get_vhost(wsi),
						 lws_get_protocol(wsi));
	char name[300], update_worst, sent_something, *pn = name;
	struct mirror_instance *mi = NULL;
	const struct a_message *msg;
	struct a_message amsg;
	uint32_t oldest_tail;
	int n, count_mi = 0;

	switch (reason) {
	case LWS_CALLBACK_ESTABLISHED:
		lwsl_info("%s: LWS_CALLBACK_ESTABLISHED\n", __func__);
		if (!v) {
			lws_protocol_vh_priv_zalloc(lws_get_vhost(wsi),
					lws_get_protocol(wsi),
					sizeof(struct per_vhost_data__lws_mirror));
			v = (struct per_vhost_data__lws_mirror *)
					lws_protocol_vh_priv_get(lws_get_vhost(wsi),
								 lws_get_protocol(wsi));
			lws_pthread_mutex_init(&v->lock);
		}

		/*
		 * mirror instance name... defaults to "", but if URL includes
		 * "?mirror=xxx", will be "xxx"
		 */

		if (lws_get_urlarg_by_name_safe(wsi, "mirror", name,
					        sizeof(name) - 1) < 0) {
			lwsl_debug("get urlarg failed\n");
			name[0] = '\0';
		}

		//lwsl_notice("%s: mirror name '%s'\n", __func__, pn);

		/* is there already a mirror instance of this name? */

		lws_pthread_mutex_lock(&v->lock); /* vhost lock { */

		lws_start_foreach_ll(struct mirror_instance *, mi1,
				     v->mi_list) {
			count_mi++;
			if (!strcmp(pn, mi1->name)) {
				/* yes... we will join it */
				mi = mi1;
				break;
			}
		} lws_end_foreach_ll(mi1, next);

		if (!mi) {

			/* no existing mirror instance for name */
			if (count_mi == MAX_MIRROR_INSTANCES) {
				lws_pthread_mutex_unlock(&v->lock); /* } vh lock */
				return -1;
			}

			/* create one with this name, and join it */
			mi = malloc(sizeof(*mi));
			if (!mi)
				goto bail1;
			memset(mi, 0, sizeof(*mi));
			mi->ring = lws_ring_create(sizeof(struct a_message),
						   QUEUELEN,
						   __mirror_destroy_message);
			if (!mi->ring) {
				free(mi);
				goto bail1;
			}

			mi->next = v->mi_list;
			v->mi_list = mi;
			lws_snprintf(mi->name, sizeof(mi->name) - 1, "%s", pn);
			mi->rx_enabled = 1;

			lws_pthread_mutex_init(&mi->lock);

			lwsl_notice("Created new mi %p '%s'\n", mi, pn);
		}

		/* add our pss to list of guys bound to this mi */

		lws_ll_fwd_insert(pss, same_mi_pss_list, mi->same_mi_pss_list);

		/* init the pss */

		pss->mi = mi;
		pss->tail = lws_ring_get_oldest_tail(mi->ring);
		pss->wsi = wsi;

		lws_pthread_mutex_unlock(&v->lock); /* } vhost lock */
		break;

bail1:
		lws_pthread_mutex_unlock(&v->lock); /* } vhost lock */
		return 1;

	case LWS_CALLBACK_CLOSED:
		/* detach our pss from the mirror instance */
		mi = pss->mi;
		if (!mi)
			break;

		lws_pthread_mutex_lock(&v->lock); /* vhost lock { */

		/* remove our closing pss from its mirror instance list */
		lws_ll_fwd_remove(struct per_session_data__lws_mirror,
				  same_mi_pss_list, pss, mi->same_mi_pss_list);
		pss->mi = NULL;

		if (mi->same_mi_pss_list) {
			/*
			 * Still other pss using the mirror instance.  The pss
			 * going away may have had the oldest tail, reconfirm
			 * using the remaining pss what is the current oldest
			 * tail.  If the oldest tail moves on, this call also
			 * will re-enable rx flow control when appropriate.
			 */
			lws_pthread_mutex_lock(&mi->lock); /* mi lock { */
			__mirror_update_worst_tail(mi);
			lws_pthread_mutex_unlock(&mi->lock); /* } mi lock */
			lws_pthread_mutex_unlock(&v->lock); /* } vhost lock */
			break;
		}

		/* No more pss using the mirror instance... delete mi */

		lws_start_foreach_llp(struct mirror_instance **,
				pmi, v->mi_list) {
			if (*pmi == mi) {
				*pmi = (*pmi)->next;

				lws_ring_destroy(mi->ring);
				lws_pthread_mutex_destroy(&mi->lock);

				free(mi);
				break;
			}
		} lws_end_foreach_llp(pmi, next);

		lws_pthread_mutex_unlock(&v->lock); /* } vhost lock */
		break;

	case LWS_CALLBACK_CONFIRM_EXTENSION_OKAY:
		return 1; /* disallow compression */

	case LWS_CALLBACK_PROTOCOL_INIT: /* per vhost */
		if (!v) {
			lws_protocol_vh_priv_zalloc(lws_get_vhost(wsi),
				lws_get_protocol(wsi),
				sizeof(struct per_vhost_data__lws_mirror));
			v = (struct per_vhost_data__lws_mirror *)
				lws_protocol_vh_priv_get(lws_get_vhost(wsi),
							 lws_get_protocol(wsi));
			if (!v)
				return 0;
			lws_pthread_mutex_init(&v->lock);
		}
		break;

	case LWS_CALLBACK_PROTOCOL_DESTROY:
		lws_pthread_mutex_destroy(&v->lock);
		break;

	case LWS_CALLBACK_SERVER_WRITEABLE:
		lws_pthread_mutex_lock(&pss->mi->lock); /* instance lock { */
		oldest_tail = lws_ring_get_oldest_tail(pss->mi->ring);
		update_worst = oldest_tail == pss->tail;
		sent_something = 0;

		do {
			msg = lws_ring_get_element(pss->mi->ring, &pss->tail);
			if (!msg)
				break;

			if (!msg->payload) {
				lwsl_err("%s: NULL payload: worst = %d,"
					 " pss->tail = %d\n", __func__,
					 oldest_tail, pss->tail);
				if (lws_ring_consume(pss->mi->ring, &pss->tail,
						     NULL, 1))
					continue;
				break;
			}

			n = lws_write(wsi, (unsigned char *)msg->payload +
				      LWS_PRE, msg->len, LWS_WRITE_TEXT);
			if (n < 0) {
				lwsl_info("%s: WRITEABLE: %d\n", __func__, n);

				goto bail2;
			}
			sent_something = 1;
			lws_ring_consume(pss->mi->ring, &pss->tail, NULL, 1);

		} while (!lws_send_pipe_choked(wsi));

		/* if any left for us to send, ask for writeable again */
		if (lws_ring_get_count_waiting_elements(pss->mi->ring,
							&pss->tail))
			lws_callback_on_writable(wsi);

		if (!sent_something || !update_worst)
			goto done1;

		/*
		 * We are no longer holding the oldest tail (since we sent
		 * something.  So free us of the timeout related to hogging the
		 * oldest tail.
		 */
		lws_set_timeout(pss->wsi, NO_PENDING_TIMEOUT, 0);
		/*
		 * If we were originally at the oldest fifo position of
		 * all the tails, now we used some up we may have
		 * changed the oldest fifo position and made some space.
		 */
		__mirror_update_worst_tail(pss->mi);

done1:
		lws_pthread_mutex_unlock(&pss->mi->lock); /* } instance lock */
		break;

bail2:
		lws_pthread_mutex_unlock(&pss->mi->lock); /* } instance lock */

		return -1;

	case LWS_CALLBACK_RECEIVE:
		lws_pthread_mutex_lock(&pss->mi->lock); /* mi lock { */
		n = (int)lws_ring_get_count_free_elements(pss->mi->ring);
		if (!n) {
			lwsl_notice("dropping!\n");
			if (pss->mi->rx_enabled)
				__mirror_rxflow_instance(pss->mi, 0);
			goto req_writable;
		}

		amsg.payload = malloc(LWS_PRE + len);
		amsg.len = len;
		if (!amsg.payload) {
			lwsl_notice("OOM: dropping\n");
			goto done2;
		}

		memcpy((char *)amsg.payload + LWS_PRE, in, len);
		if (!lws_ring_insert(pss->mi->ring, &amsg, 1)) {
			__mirror_destroy_message(&amsg);
			lwsl_notice("dropping!\n");
			if (pss->mi->rx_enabled)
				__mirror_rxflow_instance(pss->mi, 0);
			goto req_writable;
		}

		if (pss->mi->rx_enabled &&
		    lws_ring_get_count_free_elements(pss->mi->ring) <
								    RXFLOW_MIN)
			__mirror_rxflow_instance(pss->mi, 0);

req_writable:
		__mirror_callback_all_in_mi_on_writable(pss->mi);

done2:
		lws_pthread_mutex_unlock(&pss->mi->lock); /* } mi lock */
		break;

	case LWS_CALLBACK_EVENT_WAIT_CANCELLED:
		lwsl_info("LWS_CALLBACK_EVENT_WAIT_CANCELLED\n");
		break;

	default:
		break;
	}

	return 0;
}

#define LWS_PLUGIN_PROTOCOL_MIRROR { \
		"lws-mirror-protocol", \
		callback_lws_mirror, \
		sizeof(struct per_session_data__lws_mirror), \
		4096, /* rx buf size must be >= permessage-deflate rx size */ \
		0, NULL, 0 \
	}

#if !defined (LWS_PLUGIN_STATIC)

LWS_VISIBLE const struct lws_protocols lws_mirror_protocols[] = {
	LWS_PLUGIN_PROTOCOL_MIRROR
};

LWS_VISIBLE const lws_plugin_protocol_t lws_mirror = {
	.hdr = {
		"lws mirror",
		"lws_protocol_plugin",
		LWS_BUILD_HASH,
		LWS_PLUGIN_API_MAGIC
	},

	.protocols = lws_mirror_protocols,
	.count_protocols = LWS_ARRAY_SIZE(lws_mirror_protocols),
	.extensions = NULL,
	.count_extensions = 0,
};

#endif
