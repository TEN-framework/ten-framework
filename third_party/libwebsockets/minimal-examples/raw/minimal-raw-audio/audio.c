/*
 * lws-minimal-raw-audio
 *
 * Written in 2010-2019 by Andy Green <andy@warmcat.com>
 *
 * This file is made available under the Creative Commons CC0 1.0
 * Universal Public Domain Dedication.
 *
 * This demonstrates adopting and managing audio device file descriptors in the
 * event loop.
 */

#include <libwebsockets.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <alsa/asoundlib.h>

static unsigned int sample_rate = 16000;

struct raw_vhd {
	uint8_t simplebuf[32768 * 2];
	snd_pcm_t *pcm_capture;
	snd_pcm_t *pcm_playback;
	snd_pcm_hw_params_t *params;
	snd_pcm_uframes_t frames;
	int filefd;
	int rpos;
	int wpos;
	int times;
};

static int
set_hw_params(struct lws_vhost *vh, snd_pcm_t **pcm, int type)
{
	unsigned int rate = sample_rate;
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

static int
callback_raw_test(struct lws *wsi, enum lws_callback_reasons reason,
			void *user, void *in, size_t len)
{
	struct raw_vhd *vhd = (struct raw_vhd *)lws_protocol_vh_priv_get(
				     lws_get_vhost(wsi), lws_get_protocol(wsi));
	int n;

	switch (reason) {
	case LWS_CALLBACK_PROTOCOL_INIT:
		vhd = lws_protocol_vh_priv_zalloc(lws_get_vhost(wsi),
				lws_get_protocol(wsi), sizeof(struct raw_vhd));

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
		break;

	case LWS_CALLBACK_PROTOCOL_DESTROY:
		lwsl_notice("LWS_CALLBACK_PROTOCOL_DESTROY\n");
		if (vhd && vhd->pcm_playback) {
			snd_pcm_drain(vhd->pcm_playback);
			snd_pcm_close(vhd->pcm_playback);
			vhd->pcm_playback = NULL;
		}
		if (vhd && vhd->pcm_capture) {
			snd_pcm_close(vhd->pcm_capture);
			vhd->pcm_capture = NULL;
		}
		break;

	case LWS_CALLBACK_RAW_RX_FILE:
		if (vhd->times >= 6) {  /* delay amount decided by this */
			n = snd_pcm_writei(vhd->pcm_playback,
					   &vhd->simplebuf[vhd->rpos],
					   ((vhd->wpos - vhd->rpos) &
					    (sizeof(vhd->simplebuf) - 1)) / 2);
			vhd->rpos =  (vhd->rpos + (n * 2)) &
					(sizeof(vhd->simplebuf) - 1);
		}

		n = snd_pcm_readi(vhd->pcm_capture, &vhd->simplebuf[vhd->wpos],
				  (sizeof(vhd->simplebuf) - vhd->wpos) / 2);
		lwsl_notice("LWS_CALLBACK_RAW_RX_FILE: %d samples\n", n);
		vhd->times++;

		vhd->wpos = (vhd->wpos + (n * 2)) & (sizeof(vhd->simplebuf) - 1);
		break;

	default:
		break;
	}

	return 0;
}

static struct lws_protocols protocols[] = {
	{ "lws-audio-test", callback_raw_test, 0, 0 },
	LWS_PROTOCOL_LIST_TERM
};

static int interrupted;

void sigint_handler(int sig)
{
	interrupted = 1;
}

int main(int argc, const char **argv)
{
	struct lws_context_creation_info info;
	struct lws_context *context;
	int n = 0;

	signal(SIGINT, sigint_handler);
	memset(&info, 0, sizeof info);
	lws_cmdline_option_handle_builtin(argc, argv, &info);

	lwsl_user("LWS minimal raw audio\n");

	info.port = CONTEXT_PORT_NO_LISTEN_SERVER;
	info.protocols = protocols;

	context = lws_create_context(&info);
	if (!context) {
		lwsl_err("lws init failed\n");
		return 1;
	}

	while (n >= 0 && !interrupted)
		n = lws_service(context, 0);

	lws_context_destroy(context);

	return 0;
}
