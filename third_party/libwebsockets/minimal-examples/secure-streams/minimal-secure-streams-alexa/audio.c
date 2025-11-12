/*
 * alsa audio handling
 *
 * Written in 2010-2020 by Andy Green <andy@warmcat.com>
 *
 * This file is made available under the Creative Commons CC0 1.0
 * Universal Public Domain Dedication.
 */

#include <libwebsockets.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <alsa/asoundlib.h>
#include <pv_porcupine.h>

#include <mpg123.h>

#include "private.h"

extern struct lws_ss_handle *hss_avs_event, *hss_avs_sync;

int
avs_query_start(struct lws_context *context);

enum {
	MODE_IDLE,
	MODE_CAPTURING,
	MODE_PLAYING
};

struct raw_vhd {
	int16_t			p[8 * 1024]; /* 500ms at 16kHz 16-bit PCM */
	pv_porcupine_object_t	*porc;
	snd_pcm_t		*pcm_capture;
	snd_pcm_t		*pcm_playback;
	snd_pcm_hw_params_t	*params;
	snd_pcm_uframes_t	frames;
	int16_t			*porcbuf;

	mpg123_handle		*mh;

	mp3_done_cb		done_cb;
	void			*opaque;

	int			mode;
	int			rate;

	int			porc_spf;
	int			filefd;
	int			rpos;
	int			wpos;
	int			porcpos;
	int			npos;
	int			times;
	int			quietcount;
	int			anycount;

	int			wplay;
	int			rplay;

	char			last_wake_detect;
	char			destroy_mh_on_drain;
};

static struct raw_vhd *avhd;

/*
 * called from alexa.c to grab the next chunk of audio capture buffer
 * for upload
 */

int
spool_capture(uint8_t *buf, size_t len)
{
	int16_t *sam = (int16_t *)buf;
	size_t s, os;

	if (avhd->mode != MODE_CAPTURING)
		return -1;

	os = s = len / 2;

	while (s && avhd->wpos != avhd->npos) {
		*sam++ = avhd->p[avhd->npos];
		avhd->npos = (avhd->npos + 1)  % LWS_ARRAY_SIZE(avhd->p);
		s--;
	}

	lwsl_info("Copied %d samples (%d %d)\n", (int)(os - s),
			avhd->wpos, avhd->npos);

	return (os - s) * 2;
}

/*
 * Called from alexa.c to control when the mp3 playback should begin and end
 */

int
play_mp3(mpg123_handle *mh, mp3_done_cb cb, void *opaque)
{
	if (mh) {
		avhd->mh = mh;
		avhd->mode = MODE_PLAYING;
		snd_pcm_prepare(avhd->pcm_playback);

		return 0;
	}

	avhd->destroy_mh_on_drain = 1;
	avhd->done_cb = cb;
	avhd->opaque = opaque;

	return 0;
}

/*
 * Helper used to set alsa hwparams on both capture and playback channels
 */

static int
set_hw_params(struct lws_vhost *vh, snd_pcm_t **pcm, int type)
{
	unsigned int rate = pv_sample_rate(); /* it's 16kHz */
	snd_pcm_hw_params_t *params;
	lws_sock_file_fd_type u;
	struct pollfd pfd;
	struct lws *wsi1;
	int n;

	n = snd_pcm_open(pcm, "default", type, SND_PCM_NONBLOCK);
	if (n < 0) {
		lwsl_err("%s: Can't open default for playback: %s\n",
			 __func__, snd_strerror(n));

		return -1;
	}

	if (snd_pcm_poll_descriptors(*pcm, &pfd, 1) != 1) {
		lwsl_err("%s: failed to get playback desc\n", __func__);
		return -1;
	}

	u.filefd = (lws_filefd_type)(long long)pfd.fd;
	wsi1 = lws_adopt_descriptor_vhost(vh, LWS_ADOPT_RAW_FILE_DESC, u,
					  "lws-audio-test", NULL);
	if (!wsi1) {
		lwsl_err("%s: Failed to adopt playback desc\n", __func__);
		goto bail;
	}
	if (type == SND_PCM_STREAM_PLAYBACK)
		lws_rx_flow_control(wsi1, 0); /* no POLLIN */

	snd_pcm_hw_params_malloc(&params);
	snd_pcm_hw_params_any(*pcm, params);

	n = snd_pcm_hw_params_set_access(*pcm, params,
					 SND_PCM_ACCESS_RW_INTERLEAVED);
	if (n < 0)
		goto bail1;

	n = snd_pcm_hw_params_set_format(*pcm, params, SND_PCM_FORMAT_S16_LE);
	if (n < 0)
		goto bail1;

	n = snd_pcm_hw_params_set_channels(*pcm, params, 1);
	if (n < 0)
		goto bail1;

	n = snd_pcm_hw_params_set_rate_near(*pcm, params, &rate, 0);
	if (n < 0)
		goto bail1;

	lwsl_notice("%s: %s rate %d\n", __func__,
		type == SND_PCM_STREAM_PLAYBACK ? "Playback" : "Capture", rate);

	n = snd_pcm_hw_params(*pcm, params);
	snd_pcm_hw_params_free(params);
	if (n < 0)
		goto bail;

	return 0;

bail1:
	snd_pcm_hw_params_free(params);
bail:
	lwsl_err("%s: Set hw params failed: %s\n", __func__, snd_strerror(n));

	return -1;
}

/*
 * The lws RAW file protocol handler that wraps ALSA.
 *
 * The timing is coming from ALSA capture channel... since they are both set to
 * 16kHz, it's enough just to have the one.
 */

static int
callback_audio(struct lws *wsi, enum lws_callback_reasons reason, void *user,
	       void *in, size_t len)
{
	struct raw_vhd *vhd = (struct raw_vhd *)lws_protocol_vh_priv_get(
				   lws_get_vhost(wsi), lws_get_protocol(wsi));
	uint16_t rands[50];
	int16_t temp[256];
	bool det;
	long avg;
	int n, s;

	switch (reason) {
	case LWS_CALLBACK_PROTOCOL_INIT:

		if (avhd) /* just on one vhost */
			return 0;

		avhd = vhd = lws_protocol_vh_priv_zalloc(lws_get_vhost(wsi),
				lws_get_protocol(wsi), sizeof(struct raw_vhd));

		/*
		 * Set up the wakeword library
		 */

		n = pv_porcupine_init("porcupine_params.pv", "alexa_linux.ppn",
					1.0, &vhd->porc);
		if (n) {
			lwsl_err("%s: porcupine init fail %d\n", __func__, n);

			return -1;
		}
		vhd->porc_spf = pv_porcupine_frame_length();
		vhd->porcbuf = malloc(vhd->porc_spf * 2);
		lwsl_info("%s: %s porc frame length is %d samples\n", __func__,
				lws_get_vhost_name(lws_get_vhost(wsi)),
				vhd->porc_spf);

		vhd->rate = pv_sample_rate(); /* 16kHz */

		/* set up alsa */

		if (set_hw_params(lws_get_vhost(wsi), &vhd->pcm_playback,
				  SND_PCM_STREAM_PLAYBACK))  {
			lwsl_err("%s: Can't open default for playback\n",
				 __func__);

			return -1;
		}

		if (set_hw_params(lws_get_vhost(wsi), &vhd->pcm_capture,
				  SND_PCM_STREAM_CAPTURE))  {
			lwsl_err("%s: Can't open default for capture\n",
				 __func__);

			return -1;
		}

		snd_config_update_free_global();

		break;

	case LWS_CALLBACK_PROTOCOL_DESTROY:
		lwsl_info("%s: LWS_CALLBACK_PROTOCOL_DESTROY\n", __func__);
		if (!vhd)
			break;

		if (vhd->porcbuf) {
			free(vhd->porcbuf);
			vhd->porcbuf = NULL;
		}
		if (vhd->pcm_playback) {
			snd_pcm_drop(vhd->pcm_playback);
			snd_pcm_close(vhd->pcm_playback);
			vhd->pcm_playback = NULL;
		}
		if (vhd->pcm_capture) {
			snd_pcm_drop(vhd->pcm_capture);
			snd_pcm_close(vhd->pcm_capture);
			vhd->pcm_capture = NULL;
		}
		if (vhd->porc) {
			pv_porcupine_delete(vhd->porc);
			vhd->porc = NULL;
		}

		/* avoid most of the valgrind mess from alsa */
		snd_config_update_free_global();

		break;

	case LWS_CALLBACK_RAW_CLOSE_FILE:
		lwsl_info("%s: closed\n", __func__);
		break;

	case LWS_CALLBACK_RAW_RX_FILE:
		/* we come here about every 250ms */

		/*
		 * Playing back the mp3?
		 */
		if (vhd->mode == MODE_PLAYING && vhd->mh) {
			size_t amt, try;

			do {
				try = snd_pcm_avail(vhd->pcm_playback);
				if (try > LWS_ARRAY_SIZE(vhd->p))
					try = LWS_ARRAY_SIZE(vhd->p);

				n = mpg123_read(vhd->mh, (uint8_t *)vhd->p,
						try * 2, &amt);
				lwsl_info("%s: PLAYING: mpg123 read %d, n %d\n",
						__func__, (int)amt, n);
				if (n == MPG123_NEW_FORMAT) {
					snd_pcm_start(vhd->pcm_playback);
					memset(vhd->p, 0, try);
					snd_pcm_writei(vhd->pcm_playback,
						       vhd->p, try / 2);
					snd_pcm_prepare(vhd->pcm_playback);
				}
			} while (n == MPG123_NEW_FORMAT);

			if (amt) {
				n = snd_pcm_writei(vhd->pcm_playback,
						   vhd->p, amt / 2);
				if (n < 0)
					lwsl_notice("%s: snd_pcm_writei: %d %s\n",
						    __func__, n, snd_strerror(n));
				if (n == -EPIPE) {
					lwsl_err("%s: did EPIPE prep\n", __func__);
					snd_pcm_prepare(vhd->pcm_playback);
				}
			} else
				if (vhd->destroy_mh_on_drain &&
				    n != MPG123_NEW_FORMAT) {
					snd_pcm_drain(vhd->pcm_playback);
					vhd->destroy_mh_on_drain = 0;
					lwsl_notice("%s: mp3 destroyed\n",
							__func__);
					mpg123_close(vhd->mh);
					mpg123_delete(vhd->mh);
					vhd->mh = NULL;
					vhd->mode = MODE_IDLE;

					if (vhd->done_cb)
						vhd->done_cb(vhd->opaque);
				}
		}

		/*
		 * Get the capture data
		 */

		n = snd_pcm_readi(vhd->pcm_capture, temp, LWS_ARRAY_SIZE(temp));
		s = 0;
		while (s < n) {
			vhd->p[(vhd->wpos + s) % LWS_ARRAY_SIZE(vhd->p)] = temp[s];
			s++;
		}

		if (vhd->mode == MODE_CAPTURING) {

			/*
			 * We are recording an utterance.
			 *
			 * Estimate the sound density in the frame by picking 50
			 * samples at random and averaging the sampled
			 * [abs()^2] / 10000 to create a Figure of Merit.
			 *
			 * Speaking on my laptop gets us 1000 - 5000, silence
			 * is typ under 30.  The wakeword tells us there was
			 * speech at the start, end the capture when there's
			 * ~750ms (12000 samples) under 125 FOM.
			 */

#define SILENCE_THRESH 125

			avg = 0;
			lws_get_random(lws_get_context(wsi), rands, sizeof(rands));
			for (s = 0; s < (int)LWS_ARRAY_SIZE(rands); s++) {
				long q;

				q = temp[rands[s] % n];

				avg += (q * q);
			}
			avg = (avg / (int)LWS_ARRAY_SIZE(rands)) / 10000;

			lwsl_notice("est audio energy: %ld %d\n", avg, vhd->mode);

			/*
			 * Only start looking for "silence" after 1.5s, in case
			 * he does a long pause after the wakeword
			 */

			if (vhd->anycount < (3 *vhd->rate) / 2 &&
			    avg < SILENCE_THRESH) {
				vhd->quietcount += n;
				/* then 500ms of "silence" does it for us */
				if (vhd->quietcount >= ((vhd->rate * 3) / 4)) {
					lwsl_warn("%s: ended capture\n", __func__);
					vhd->mode = MODE_IDLE;
					vhd->quietcount = 0;
				}
			}

			/* if we're not "silent", reset the count */
			if (avg > SILENCE_THRESH * 2)
				vhd->quietcount = 0;

			/*
			 * Since we are in capturing mode, we have something
			 * new to send now.
			 *
			 * We must send an extra one at the end so we can finish
			 * the tx.
			 */
			lws_ss_request_tx(hss_avs_sync);
		}

		/*
		 * Just waiting for a wakeword
		 */

		while (vhd->mode == MODE_IDLE) {
			int m = 0, ppold = vhd->porcpos;

			s = (vhd->wpos - vhd->porcpos) % LWS_ARRAY_SIZE(vhd->p);
			if (s < vhd->porc_spf)
				goto eol;

			while (m < vhd->porc_spf) {
				vhd->porcbuf[m++] = avhd->p[vhd->porcpos];
				vhd->porcpos = (vhd->porcpos + 1) %
							LWS_ARRAY_SIZE(vhd->p);
			}

			if (pv_porcupine_process(vhd->porc, vhd->porcbuf, &det))
				lwsl_err("%s: porc_process failed\n", __func__);

			if (!det && vhd->last_wake_detect &&
			    vhd->mode == MODE_IDLE) {
				lwsl_warn("************* Wakeword\n");
				if (!avs_query_start(lws_get_context(wsi))) {
					vhd->mode = MODE_CAPTURING;
					vhd->quietcount = 0;
					vhd->last_wake_detect = det;
					vhd->npos = ppold;
					break;
				}
			}
			vhd->last_wake_detect = det;
		}

eol:
		vhd->wpos = (vhd->wpos + n) % LWS_ARRAY_SIZE(vhd->p);
		break;

	default:
		break;
	}

	return 0;
}

struct lws_protocols protocol_audio_test =
	{ "lws-audio-test", callback_audio, 0, 0 };
