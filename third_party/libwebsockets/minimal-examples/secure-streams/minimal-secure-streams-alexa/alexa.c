/*
 * lws-minimal-secure-streams-alexa
 *
 * This file is made available under the Creative Commons CC0 1.0
 * Universal Public Domain Dedication.
 */

#include <libwebsockets.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

#include <mpg123.h>

#include "private.h"

struct lws_ss_handle *hss_avs_event, *hss_avs_sync;

/* this is the type for the long poll event channel */

typedef struct ss_avs_event {
	struct lws_ss_handle 	*ss;
	void			*opaque_data;
	/* ... application specific state ... */

	struct lejp_ctx		jctx;
} ss_avs_event_t;

enum {
	LAMP3STATE_IDLE,
	LAMP3STATE_SPOOLING,
	LAMP3STATE_DRAINING,
};

/* this is the type for the utterance metadata (and audio rideshares) */

typedef struct ss_avs_metadata {
	struct lws_ss_handle 	*ss;
	void			*opaque_data;
	/* ... application specific state ... */

	struct lws_buflist	*dribble; /* next mp3 data while draining last */

	struct lejp_ctx		jctx;
	size_t			pos;
	size_t			mp3_in;
	mpg123_handle		*mh;

	lws_sorted_usec_list_t	sul;

	uint8_t			stash_eom[16];

	uint8_t			se_head;
	uint8_t			se_tail;

	char			mp3_state;
	char			first_mp3;
	uint8_t			mp3_mime_match;
	uint8_t			seen;
	uint8_t			inside_mp3;

} ss_avs_metadata_t;

/*
 * The remote server only seems to give us a budget of 10s to consume the
 * results, after that it doesn't drop the stream, but doesn't send us anything
 * further on it.
 *
 * This makes it impossible to optimize buffering for incoming mp3 since we
 * have to go ahead and take it before the 10s is up.
 */

#define MAX_MP3_IN_BUFFERING_BYTES 32768

/*
 * Structure of JSON metadata for utterance handling
 */

static const char *metadata = "{"
	"\"event\": {"
		"\"header\": {"
			"\"namespace\": \"SpeechRecognizer\","
			"\"name\": \"Recognize\","
			"\"messageId\": \"message-123\","
			"\"dialogRequestId\": \"dialog-request-321\""
		"},"
		"\"payload\": {"
			"\"profile\":"	"\"CLOSE_TALK\","
			"\"format\":"	"\"AUDIO_L16_RATE_16000_CHANNELS_1\""
		"}"
	"}"
"}";

/*
 * avs metadata
 */

static void
use_buffer_250ms(lws_sorted_usec_list_t *sul)
{
	ss_avs_metadata_t *m = lws_container_of(sul, ss_avs_metadata_t, sul);
	struct lws_context *context = (struct lws_context *)m->opaque_data;
	int est = lws_ss_get_est_peer_tx_credit(m->ss);

	lwsl_notice("%s: est txcr %d\n", __func__, est);

	if (est < MAX_MP3_IN_BUFFERING_BYTES - (MAX_MP3_IN_BUFFERING_BYTES / 4)) {
		lwsl_notice("   adding %d\n", MAX_MP3_IN_BUFFERING_BYTES / 4);
		lws_ss_add_peer_tx_credit(m->ss, MAX_MP3_IN_BUFFERING_BYTES / 4);
	}

	lws_sul_schedule(context, 0, &m->sul, use_buffer_250ms,
			 250 * LWS_US_PER_MS);
}

static const char *mp3_mimetype = "application/octet-stream",
		  *match2 = "\x0d\x0a\x0d\x0a";

static int
ss_avs_mp3_open(ss_avs_metadata_t *m)
{
	int r;

	lwsl_notice("%s\n", __func__);

	m->first_mp3 = 1;
	m->mh = mpg123_new(NULL, NULL);
	if (!m->mh) {
		lwsl_err("%s: unable to make new mp3\n",
				__func__);
		goto bail;
	}
	mpg123_format_none(m->mh);
	r = mpg123_format(m->mh, 16000, MPG123_M_MONO,
			  MPG123_ENC_SIGNED_16);
	if (r) {
		lwsl_err("%s: mpg123 format failed %d\n",
				__func__, r);
		goto bail1;
	}
	r = mpg123_open_feed(m->mh);
	if (r) {
		lwsl_err("%s: mpg123 open feed failed %d\n",
				__func__, r);
		goto bail1;
	}

	return 0;

bail1:
	mpg123_delete(m->mh);
	m->mh = NULL;

bail:
	return 1;
}

static lws_ss_state_return_t
ss_avs_metadata_rx(void *userobj, const uint8_t *buf, size_t len, int flags);

/*
 * This is called when the mp3 has drained it's input buffer and destroyed
 * itself.
 */

static int
drain_end_cb(void *v)
{
	ss_avs_metadata_t *m = (ss_avs_metadata_t *)v;
	struct lws_context *context = (struct lws_context *)m->opaque_data;
	int tot = 0;

	lwsl_err("%s\n", __func__);

	/*
	 * We have drained and destroyed the existing mp3 session.  Is there
	 * a new one pending?
	 */

	m->first_mp3 = 1;
	m->mp3_state = LAMP3STATE_IDLE;

	if (lws_buflist_total_len(&m->dribble)) {
		/* we started another one */

		/* resume tx credit top up */
		lws_sul_schedule(context, 0, &m->sul, use_buffer_250ms, 1);

		if (ss_avs_mp3_open(m))
			return 1;

		m->mp3_state = LAMP3STATE_SPOOLING;

		/*
		 * Dump what we stashed from draining into the new mp3
		 */

		while (lws_buflist_total_len(&m->dribble)) {
			size_t s;
			uint8_t *u, t;

			s = lws_buflist_next_segment_len(&m->dribble, &u);
			t = m->stash_eom[m->se_tail];
			lwsl_notice("%s: preload %d: %d\n", __func__, (int)s, t);

			mpg123_feed(m->mh, u, s);
			lws_buflist_use_segment(&m->dribble, s);
			if (m->first_mp3) {
				play_mp3(m->mh, NULL, NULL);
				m->first_mp3 = 0;
			}

			tot += s;

			m->se_tail = (m->se_tail + 1) % sizeof(m->stash_eom);
			if (t) {
				lwsl_notice("%s: preloaded EOM\n", __func__);

				/*
				 * We stashed the whole of the message, we need
				 * to also do the EOM processing.  We will come
				 * back here if there's another message in the
				 * stash.
				 */

				m->mp3_state = LAMP3STATE_DRAINING;
				if (m->mh)
					play_mp3(NULL, drain_end_cb, m);

				lws_ss_add_peer_tx_credit(m->ss, tot);
#if 0
				/*
				 * Put a hold on bringing in any more data
				 */
				lws_sul_cancel(&m->sul);
#endif
				/* destroy our copy of the handle */
				m->mh = NULL;

				break;
			}
		}

		lws_ss_add_peer_tx_credit(m->ss, tot);
	}

	return 0;
}

static lws_ss_state_return_t
ss_avs_metadata_rx(void *userobj, const uint8_t *buf, size_t len, int flags)
{
	ss_avs_metadata_t *m = (ss_avs_metadata_t *)userobj;
	struct lws_context *context = (struct lws_context *)m->opaque_data;
	int n = 0, hit = 0;

	lwsl_notice("%s: len %d, flags %d (est peer txcr %d)\n", __func__,
		    (int)len, flags, lws_ss_get_est_peer_tx_credit(m->ss));

	// lwsl_hexdump_warn(buf, len);

	if ((flags & LWSSS_FLAG_SOM) && !m->mh && !m->seen) {
		m->mp3_mime_match = 0;
		m->seen = 0;
		m->inside_mp3 = 0;
	}

	if (!m->inside_mp3) {
		/*
		 * Identify the part with the mp3 in, if any
		 */

		while (n < (int)len - 24) {
			if (!m->seen) {
				if (buf[n] == mp3_mimetype[m->mp3_mime_match]) {
					m->mp3_mime_match++;
					if (m->mp3_mime_match == 24) {
						m->mp3_mime_match = 0;
						m->seen = 1;
						n++;
						continue;
					}
				} else
					m->mp3_mime_match = 0;
			} else {
				if (buf[n] == match2[m->mp3_mime_match]) {
					m->mp3_mime_match++;
					if (m->mp3_mime_match == 4) {
						m->seen = 0;
						m->mp3_mime_match = 0;
						hit = 1;
						n++;
						buf += n;
						len -= n;
						lwsl_notice("identified reply...\n");
						m->inside_mp3 = 1;
						break;
					}
				} else
					m->mp3_mime_match = 0;
			}

			n++;
		}

		if (!hit) {
			lws_ss_add_peer_tx_credit(m->ss, len);
			return 0;
		}
	}

	// lwsl_notice("%s: state %d\n", __func__, m->mp3_state);

	switch (m->mp3_state) {
	case LAMP3STATE_IDLE:

		if (hit) {

			lws_ss_add_peer_tx_credit(m->ss, n);

			if (ss_avs_mp3_open(m))
				goto bail;

			lws_sul_schedule(context, 0, &m->sul, use_buffer_250ms, 1);
			m->mp3_state = LAMP3STATE_SPOOLING;
			break;
		}

		lws_ss_add_peer_tx_credit(m->ss, len);

		if (!m->inside_mp3)
			break;

		/* fallthru */

	case LAMP3STATE_SPOOLING:

		if (m->dribble)
			goto draining;

		if (len) {
			/*
			 * We are shoving encoded mp3 into mpg123-allocated heap
			 * buffers... unfortunately mpg123 doesn't seem to
			 * expose where it is in its allocated input so we can
			 * track how much is stashed.  Instead while in playback
			 * mode, we assume 64kbps mp3 encoding, ie, 8KB/s, and
			 * run a sul that allows an additional 2KB tx credit
			 * every 250ms, with 4KB initial credit.
			 */
			lwsl_notice("%s: SPOOL %d\n", __func__, (int)len);
			mpg123_feed(m->mh, buf, len);

			if (m->first_mp3) {
				lws_sul_schedule(context, 0, &m->sul,
						 use_buffer_250ms, 1);
		//		lws_ss_add_peer_tx_credit(m->ss,
		//			len + (MAX_MP3_IN_BUFFERING_BYTES / 2));
				play_mp3(m->mh, NULL, NULL);
			} //else
		//		lws_ss_add_peer_tx_credit(m->ss, len);
			m->first_mp3 = 0;
		}

		if (flags & LWSSS_FLAG_EOM) {
			/*
			 * This means one "message" / mime part with mp3 data
			 * has finished coming in.  But there may be whole other
			 * parts with other mp3s following, with potentially
			 * different mp3 parameters.  So we want to tell this
			 * one to drain and finish and destroy the current mp3
			 * object before we go on.
			 *
			 * But not knowing the length of the current one, there
			 * will already be outstanding tx credit at the server,
			 * so it's going to spam us with the next part before we
			 * have the new mp3 sink for it.
			 */
			lwsl_notice("%s: EOM\n", __func__);
			m->mp3_mime_match = 0;
			m->seen = 0;
			m->mp3_state = LAMP3STATE_DRAINING;
			/* from input POV, we're no longer inside an mp3 */
			m->inside_mp3 = 0;
			if (m->mh)
				play_mp3(NULL, drain_end_cb, m);
#if 0
			/*
			 * Put a hold on bringing in any more data
			 */
			lws_sul_cancel(&m->sul);
#endif
			/* destroy our copy of the handle */
			m->mh = NULL;
		}
		break;

	case LAMP3STATE_DRAINING:

draining:
		if (buf && len && m->inside_mp3) {
			lwsl_notice("%s: DRAINING: stashing %d: %d %d %d\n",
				    __func__, (int)len, !!(flags & LWSSS_FLAG_EOM),
				    m->se_head, m->se_tail);
			lwsl_hexdump_notice(buf, len);
			if (lws_buflist_append_segment(&m->dribble, buf, len) < 0)
				goto bail;

			m->stash_eom[m->se_head] = !!(flags & LWSSS_FLAG_EOM);
			m->se_head = (m->se_head + 1) % sizeof(m->stash_eom);
			lwsl_notice("%s: next head %d\n", __func__, m->se_head);

			lws_ss_add_peer_tx_credit(m->ss, len);
		}

		if (flags & LWSSS_FLAG_EOM) {
			if (!len && m->se_head != m->se_tail) {
				/* 0-len EOM... retrospectively mark last stash */
				lwsl_notice("%s: retro EOM\n", __func__);
				m->stash_eom[(m->se_head - 1) % sizeof(m->stash_eom)] = 1;
			}

			lwsl_notice("%s: Draining EOM\n", __func__);
			m->inside_mp3 = 0;
		}
		/*
		 * Don't provide any additional tx credit... we're just
		 * mopping up the overspill from the previous mp3 credit
		 */
		break;
	}

	return 0;

bail:
	return -1;
}

/*
 * Because this is multipart mime in h2 currently, use a "rideshare" to handle
 * first the native metadata on this secure stream, then the "rideshare" audio
 * stream mentioned in the policy.
 *
 * Lws takes care of interleaving the multipart mime pieces since the policy
 * calls for it.
 */

static lws_ss_state_return_t
ss_avs_metadata_tx(void *userobj, lws_ss_tx_ordinal_t ord, uint8_t *buf,
		   size_t *len, int *flags)
{
	ss_avs_metadata_t *m = (ss_avs_metadata_t *)userobj;
	size_t tot;
	int n;

	// lwsl_notice("%s %d\n", __func__, (int)m->pos);

	if ((long)m->pos < 0) {
		*len = 0;
		lwsl_info("%s: skip\n", __func__);
		return 1;
	}

	if (!strcmp(lws_ss_rideshare(m->ss), "avs_audio")) {

		/* audio rideshare part */

		if (!m->pos)
			*flags |= LWSSS_FLAG_SOM;

		n = spool_capture(buf, *len);
		if (n > 0)
			*len = n;
		else
			*len = 0;
		if (!n) {
			lwsl_info("%s: trying to skip tx\n", __func__);
			return 1;
		}

		m->pos += *len;

		if (n < 0) {
			*flags |= LWSSS_FLAG_EOM;
			m->pos = (long)-1l; /* ban subsequent until new stream */
		}

		lwsl_notice("%s: tx audio %d\n", __func__, (int)*len);

#if 0
		{
			int ff = open("/tmp/z1", O_RDWR | O_CREAT | O_APPEND, 0666);
			if (ff == -1)
				lwsl_err("%s: errno %d\n", __func__, errno);
			write(ff, buf, *len);
			close(ff);
		}
#endif

		return 0;
	}

	/* metadata part */

	tot = strlen(metadata);

	if (!m->pos)
		*flags |= LWSSS_FLAG_SOM;

	if (*len > tot - m->pos)
		*len = tot - m->pos;

	memcpy(buf, metadata + m->pos, *len);

	m->pos += *len;

	if (m->pos == tot) {
		lwsl_notice("metadata done\n");
		*flags |= LWSSS_FLAG_EOM;
		m->pos = 0; /* for next time */
	}

	return 0;
}

static lws_ss_state_return_t
ss_avs_metadata_state(void *userobj, void *sh,
		      lws_ss_constate_t state, lws_ss_tx_ordinal_t ack)
{
	ss_avs_metadata_t *m = (ss_avs_metadata_t *)userobj;
	struct lws_context *context = (struct lws_context *)m->opaque_data;

	lwsl_notice("%s: %p: %s, ord 0x%x\n", __func__, m->ss,
		    lws_ss_state_name(state), (unsigned int)ack);

	switch (state) {
	case LWSSSCS_CREATING:
		return lws_ss_client_connect(m->ss);

	case LWSSSCS_CONNECTING:
		m->pos = 0;
		break;
	case LWSSSCS_CONNECTED:
		lwsl_info("%s: CONNECTED\n", __func__);
		return lws_ss_request_tx(m->ss);

	case LWSSSCS_DISCONNECTED:
		lws_sul_cancel(&m->sul);
		//if (m->mh) {
			play_mp3(NULL, NULL, NULL);
			m->mh = NULL;
		//}
		/*
		 * For this stream encapsulating an alexa exchange, dropping
		 * is the end of its life
		 */
		return 1;

	case LWSSSCS_DESTROYING:
		lws_buflist_destroy_all_segments(&m->dribble);
		break;
	default:
		break;
	}

	return 0;
}

/*
 * avs event
 */

static lws_ss_state_return_t
ss_avs_event_rx(void *userobj, const uint8_t *buf, size_t len, int flags)
{
	return 0;
}

static lws_ss_state_return_t
ss_avs_event_tx(void *userobj, lws_ss_tx_ordinal_t ord, uint8_t *buf,
		      size_t *len, int *flags)
{
	return 1; /* don't transmit anything */
}

static lws_ss_state_return_t
ss_avs_event_state(void *userobj, void *sh,
		   lws_ss_constate_t state, lws_ss_tx_ordinal_t ack)
{
	lwsl_info("%s: %s, ord 0x%x\n", __func__, lws_ss_state_name(state),
		  (unsigned int)ack);

	switch (state) {
	case LWSSSCS_CREATING:
		mpg123_init();
		break;
	case LWSSSCS_CONNECTING:
		break;
	case LWSSSCS_CONNECTED:
		lwsl_user("Connected to Alexa... speak \"Alexa, ...\"\n");
		break;
	case LWSSSCS_DISCONNECTED:
		lwsl_user("Disconnected from Alexa\n");
		break;
	case LWSSSCS_DESTROYING:
		mpg123_exit();
		break;
	default:
		break;
	}

	return 0;
}

int
avs_query_start(struct lws_context *context)
{
	lws_ss_info_t ssi;

	lwsl_notice("%s:\n", __func__);

	memset(&ssi, 0, sizeof(ssi));
	ssi.handle_offset	    = offsetof(ss_avs_metadata_t, ss);
	ssi.opaque_user_data_offset = offsetof(ss_avs_metadata_t, opaque_data);
	ssi.rx			    = ss_avs_metadata_rx;
	ssi.tx			    = ss_avs_metadata_tx;
	ssi.state		    = ss_avs_metadata_state;
	ssi.user_alloc		    = sizeof(ss_avs_metadata_t);
	ssi.streamtype		    = "avs_metadata";

	ssi.manual_initial_tx_credit = 8192;

	if (lws_ss_create(context, 0, &ssi, context, &hss_avs_sync, NULL, NULL)) {
		lwsl_err("%s: failed to create avs metadata secstream\n",
			 __func__);

		return 1;
	}

	lwsl_user("%s: created query stream %p\n", __func__, hss_avs_sync);

	return 0;
}

int
avs_example_start(struct lws_context *context)
{
	lws_ss_info_t ssi;

	if (hss_avs_event)
		return 0;

	lwsl_info("%s: Starting AVS stream\n", __func__);

	/* AVS wants us to establish the long poll event stream first */

	memset(&ssi, 0, sizeof(ssi));
	ssi.handle_offset	    = offsetof(ss_avs_event_t, ss);
	ssi.opaque_user_data_offset = offsetof(ss_avs_event_t, opaque_data);
	ssi.rx			    = ss_avs_event_rx;
	ssi.tx			    = ss_avs_event_tx;
	ssi.state		    = ss_avs_event_state;
	ssi.user_alloc		    = sizeof(ss_avs_event_t);
	ssi.streamtype		    = "avs_event";

	if (lws_ss_create(context, 0, &ssi, context, &hss_avs_event, NULL, NULL)) {
		lwsl_err("%s: failed to create avs event secure stream\n",
			 __func__);
		return 1;
	}

	return 0;
}
