/*
 * ws protocol handler plugin for "lws-minimal-pmd-bulk"
 *
 * Written in 2010-2019 by Andy Green <andy@warmcat.com>
 *
 * This file is made available under the Creative Commons CC0 1.0
 * Universal Public Domain Dedication.
 *
 * The protocol shows how to send and receive bulk messages over a ws connection
 * that optionally may have the permessage-deflate extension negotiated on it.
 */

#if !defined (LWS_PLUGIN_STATIC)
#define LWS_DLL
#define LWS_INTERNAL
#include <libwebsockets.h>
#endif

#include <string.h>

/*
 * We will produce a large ws message either from this text repeated many times,
 * or from 0x40 + a 6-bit pseudorandom number
 */

static const char * const redundant_string =
	"No one would have believed in the last years of the nineteenth "
	"century that this world was being watched keenly and closely by "
	"intelligences greater than man's and yet as mortal as his own; that as "
	"men busied themselves about their various concerns they were "
	"scrutinised and studied, perhaps almost as narrowly as a man with a "
	"microscope might scrutinise the transient creatures that swarm and "
	"multiply in a drop of water.  With infinite complacency men went to "
	"and fro over this globe about their little affairs, serene in their "
	"assurance of their empire over matter. It is possible that the "
	"infusoria under the microscope do the same.  No one gave a thought to "
	"the older worlds of space as sources of human danger, or thought of "
	"them only to dismiss the idea of life upon them as impossible or "
	"improbable.  It is curious to recall some of the mental habits of "
	"those departed days.  At most terrestrial men fancied there might be "
	"other men upon Mars, perhaps inferior to themselves and ready to "
	"welcome a missionary enterprise. Yet across the gulf of space, minds "
	"that are to our minds as ours are to those of the beasts that perish, "
	"intellects vast and cool and unsympathetic, regarded this earth with "
	"envious eyes, and slowly and surely drew their plans against us.  And "
	"early in the twentieth century came the great disillusionment. "
;

/* this reflects the length of the string above */
#define REPEAT_STRING_LEN 1337
/* this is the total size of the ws message we will send */
#define MESSAGE_SIZE (100 * REPEAT_STRING_LEN)
/* this is how much we will send each time the connection is writable */
#define MESSAGE_CHUNK_SIZE (1 * 1024)

/* one of these is created for each client connecting to us */

struct per_session_data__minimal_pmd_bulk {
	int position_tx, position_rx;
	uint64_t rng_rx, rng_tx;
};

struct vhd_minimal_pmd_bulk {
	struct lws_context *context;
	struct lws_vhost *vhost;
	struct lws *client_wsi;

	lws_sorted_usec_list_t sul;

	int *interrupted;
	int *options;
};

static uint64_t rng(uint64_t *r)
{
        *r ^= *r << 21;
        *r ^= *r >> 35;
        *r ^= *r << 4;

        return *r;
}

static void
sul_connect_attempt(struct lws_sorted_usec_list *sul)
{
	struct vhd_minimal_pmd_bulk *vhd =
		lws_container_of(sul, struct vhd_minimal_pmd_bulk, sul);
	struct lws_client_connect_info i;

	memset(&i, 0, sizeof(i));

	i.context = vhd->context;
	i.port = 7681;
	i.address = "localhost";
	i.path = "/";
	i.host = i.address;
	i.origin = i.address;
	i.ssl_connection = 0;
	i.vhost = vhd->vhost;
	i.protocol = "lws-minimal-pmd-bulk";
	i.pwsi = &vhd->client_wsi;

	if (!lws_client_connect_via_info(&i))
		lws_sul_schedule(vhd->context, 0, &vhd->sul,
				 sul_connect_attempt, 10 * LWS_US_PER_SEC);
}

static int
callback_minimal_pmd_bulk(struct lws *wsi, enum lws_callback_reasons reason,
			  void *user, void *in, size_t len)
{
	struct per_session_data__minimal_pmd_bulk *pss =
			(struct per_session_data__minimal_pmd_bulk *)user;
	struct vhd_minimal_pmd_bulk *vhd = (struct vhd_minimal_pmd_bulk *)
			lws_protocol_vh_priv_get(lws_get_vhost(wsi),
				lws_get_protocol(wsi));
	uint8_t buf[LWS_PRE + MESSAGE_CHUNK_SIZE], *start = &buf[LWS_PRE], *p;
	int n, m, flags;

	switch (reason) {

	case LWS_CALLBACK_PROTOCOL_INIT:
		vhd = lws_protocol_vh_priv_zalloc(lws_get_vhost(wsi),
				lws_get_protocol(wsi),
				sizeof(struct vhd_minimal_pmd_bulk));
		if (!vhd)
			return -1;

		vhd->context = lws_get_context(wsi);
		vhd->vhost = lws_get_vhost(wsi);

		/* get the pointer to "interrupted" we were passed in pvo */
		vhd->interrupted = (int *)lws_pvo_search(
			(const struct lws_protocol_vhost_options *)in,
			"interrupted")->value;
		vhd->options = (int *)lws_pvo_search(
			(const struct lws_protocol_vhost_options *)in,
			"options")->value;

		sul_connect_attempt(&vhd->sul);
		break;

	case LWS_CALLBACK_PROTOCOL_DESTROY:
		lws_sul_cancel(&vhd->sul);
		break;

	case LWS_CALLBACK_CLIENT_ESTABLISHED:
		pss->rng_tx = 4;
		pss->rng_rx = 4;
		lws_callback_on_writable(wsi);
		break;

	case LWS_CALLBACK_CLIENT_WRITEABLE:

		/*
		 * when we connect, we will send the server a message
		 */

		if (pss->position_tx == MESSAGE_SIZE)
			break;

		/* fill up one chunk's worth of message content */

		p = start;
		n = MESSAGE_CHUNK_SIZE;
		if (n > MESSAGE_SIZE - pss->position_tx)
			n = MESSAGE_SIZE - pss->position_tx;

		flags = lws_write_ws_flags(LWS_WRITE_BINARY, !pss->position_tx,
					   pss->position_tx + n == MESSAGE_SIZE);

		/*
		 * select between producing compressible repeated text,
		 * or uncompressible PRNG output
		 */

		if (*vhd->options & 1) {
			while (n) {
				size_t s;

				m = pss->position_tx % REPEAT_STRING_LEN;
				s = (unsigned int)(REPEAT_STRING_LEN - m);
				if (s > (size_t)n)
					s = (unsigned int)n;
				memcpy(p, &redundant_string[m], s);
				pss->position_tx += (int)s;
				p += s;
				n -= (int)s;
			}
		} else {
			pss->position_tx += n;
			while (n--)
				*p++ = (uint8_t)rng(&pss->rng_tx);
		}

		n = lws_ptr_diff(p, start);
		m = lws_write(wsi, start, (unsigned int)n, (enum lws_write_protocol)flags);
		if (m < n) {
			lwsl_err("ERROR %d writing ws\n", m);
			return -1;
		}
		if (pss->position_tx != MESSAGE_SIZE) /* if more to do... */
			lws_callback_on_writable(wsi);
		else
			/* if we sent and received everything */
			if (pss->position_rx == MESSAGE_SIZE)
				*vhd->interrupted = 2;
		break;

	case LWS_CALLBACK_CLIENT_RECEIVE:

		/*
		 * When we connect, the server will send us a message too
		 */

		lwsl_user("LWS_CALLBACK_CLIENT_RECEIVE: %4d (rpp %5d, last %d)\n",
			(int)len, (int)lws_remaining_packet_payload(wsi),
			lws_is_final_fragment(wsi));

		if (*vhd->options & 1) {
			while (len) {
				size_t s;

				m = pss->position_rx % REPEAT_STRING_LEN;
				s = (unsigned int)(REPEAT_STRING_LEN - m);
				if (s > len)
					s = len;
				if (memcmp(in, &redundant_string[m], s)) {
					lwsl_user("echo'd data doesn't match\n");
					return -1;
				}
				pss->position_rx += (int)s;
				in = ((unsigned char *)in) + s;
				len -= s;
			}
		} else {
			p = (uint8_t *)in;
			pss->position_rx += (int)len;
			while (len--)
				if (*p++ != (uint8_t)rng(&pss->rng_rx)) {
					lwsl_user("echo'd data doesn't match\n");
					return -1;
				}
		}

		/* if we sent and received everything */

		if (pss->position_rx == MESSAGE_SIZE &&
		    pss->position_tx == MESSAGE_SIZE)
			*vhd->interrupted = 2;

		break;

	case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
		lwsl_err("CLIENT_CONNECTION_ERROR: %s\n",
			 in ? (char *)in : "(null)");
		vhd->client_wsi = NULL;
		lws_sul_schedule(vhd->context, 0, &vhd->sul,
				 sul_connect_attempt, LWS_US_PER_SEC);
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

#define LWS_PLUGIN_PROTOCOL_MINIMAL_PMD_BULK \
	{ \
		"lws-minimal-pmd-bulk", \
		callback_minimal_pmd_bulk, \
		sizeof(struct per_session_data__minimal_pmd_bulk), \
		4096, \
		0, NULL, 0 \
	}
