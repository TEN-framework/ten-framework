/*
 * ws protocol handler plugin for sshd demo
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
 * These test plugins are intended to be adapted for use in your code, which
 * may be proprietary.  So unlike the library itself, they are licensed
 * Public Domain.
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

#include <lws-ssh.h>

#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>

#define TEST_SERVER_KEY_PATH "/etc/lws-test-sshd-server-key"

struct per_vhost_data__lws_sshd_demo {
	const struct lws_protocols *ssh_base_protocol;
	int privileged_fd;
};

/*
 *  This is a copy of the lws ssh test public key, you can find it in
 *  /usr[/local]/share/libwebsockets-test-server/lws-ssh-test-keys.pub
 *  and the matching private key there too in .../lws-ssh-test-keys
 *
 *  If the vhost with this protocol is using localhost:2222, you can test with
 *  the matching private key like this:
 *
 *  ssh -p 2222 -i /usr/local/share/libwebsockets-test-server/lws-ssh-test-keys anyuser@127.0.0.1
 *
 *  These keys are distributed for testing!  Don't use them on a real system
 *  unless you want anyone with a copy of lws to access it.
 */
static const char *authorized_key =
	"ssh-rsa AAAAB3NzaC1yc2EAAAADAQABAAACAQCnWiP+c+kSD6Lk+C6NA9KruApa45sbt"
	"94/dxT0bCITlAA/+PBk6mR1lwWgXYozOMdrHrqx34piqDyXnc4HabqCaOm/FrYhkCPL8z"
	"a26PMYqteSosuwKv//5iT6ZWhNnsMwExBwtV6MIq0MxAeWqxRnYNWpNM8iN6sFzkdG/YF"
	"dyHrIBTgwzM77NLCMl6GEkJErRCFppC2SwYxGa3BRrgUwX3LkV8HpMIaYHFo1Qgj7Scqm"
	"HwS2R75SOqi2aOWDpKjznARg9JgzDWSQi4seBMV2oL0BTwJANSDf+p0sQLsaKGJhpVpBQ"
	"yS2wUeyuGyytupWzEluQrajMZq52iotcogv5BfeulfTTFbJP4kuHOsSP0lsQ2lpMDQANS"
	"HEvXxzHJLDLXM9gXJzwJ+ZiRt6R+bfmP1nfN3MiWtxcIbBanWwQK6xTCKBe4wPaKta5EU"
	"6wsLPeakOIVzoeaOu/HsbtPZlwX0Mu/oUFcfKyKAhlkU15MOAIEfUPo8Yh52bWMlIlpZa"
	"4xWbLMGw3GrsrPPdcsAauyqvY4/NjjWQbWhP1SuUfvv5709PIiOUjVKK2HUwmR1ouch6X"
	"MQGXfMR1h1Wjvc+bkNs17gCIrQnFilAZLC3Sm3Opiz/4LO99Hw448G0RM2vQn0mJE46w"
	"Eu/B10U6Jf4Efojhh1dk85BD1LTIb+N3Q== ssh-test-key@lws";

enum states {
	SSH_TEST_GREET,
	SSH_TEST_PRESSED,
	SSH_TEST_DONE,
};

static const char * const strings[] =
	{
		/* SSH_TEST_GREET */
		"Thanks for logging to lws sshd server demo.\n\r"
		"\n\r"
		"This demo is very simple, it waits for you to press\n\r"
		"a key, and acknowledges it.  Then press another key\n\r"
		"and it will exit.  But actually that demos the basic\n\r"
		"sshd functions underneath.  You can use the ops struct\n\r"
		"members to add a pty / shell or whatever you want.\n\r"
		"\n\r"
		"Press a key...\n\r",

		/* SSH_TEST_PRESSED */
		"Thanks for pressing a key.  Press another to exit.\n\r",

		/* SSH_TEST_DONE */
		"Bye!\n\r"
	};

struct sshd_instance_priv {
	struct lws *wsi;
	enum states state;
	const char *ptr;
	int pos;
	int len;
};

static void
enter_state(struct sshd_instance_priv *priv, enum states state)
{
	priv->state = state;
	priv->ptr = strings[state];
	priv->pos = 0;
	priv->len = (int)strlen(priv->ptr);

	lws_callback_on_writable(priv->wsi);
}

/* ops: channel lifecycle */

static int
ssh_ops_channel_create(struct lws *wsi, void **_priv)
{
	struct sshd_instance_priv *priv;

	priv = malloc(sizeof(struct sshd_instance_priv));
	*_priv = priv;
	if (!priv)
		return 1;

	memset(priv, 0, sizeof(*priv));
	priv->wsi = wsi;

	return 0;
}

static int
ssh_ops_channel_destroy(void *_priv)
{
	struct sshd_instance_priv *priv = _priv;

	free(priv);

	return 0;
}

/* ops: IO */

static int
ssh_ops_tx_waiting(void *_priv)
{
	struct sshd_instance_priv *priv = _priv;

	if (priv->state == SSH_TEST_DONE &&
	    priv->pos == priv->len)
		return -1; /* exit */

	if (priv->pos != priv->len)
		return LWS_STDOUT;

	return 0;
}

static size_t
ssh_ops_tx(void *_priv, int stdch, uint8_t *buf, size_t len)
{
	struct sshd_instance_priv *priv = _priv;
	size_t chunk = len;

	if (stdch != LWS_STDOUT)
		return 0;

	if ((size_t)(priv->len - priv->pos) < chunk)
		chunk = (size_t)(priv->len - priv->pos);

	if (!chunk)
		return 0;

	memcpy(buf, priv->ptr + priv->pos, chunk);
	priv->pos += (int)chunk;

	if (priv->state == SSH_TEST_DONE && priv->pos == priv->len) {
		/*
		 * we are sending the last thing we want to send
		 * before exiting.  Make it ask again at ssh_ops_tx_waiting()
		 * and we will exit then, after this has been sent
		 */
		lws_callback_on_writable(priv->wsi);
	}

	return chunk;
}


static int
ssh_ops_rx(void *_priv, struct lws *wsi, const uint8_t *buf, uint32_t len)
{
	struct sshd_instance_priv *priv = _priv;

	if (priv->state < SSH_TEST_DONE)
		enter_state(priv, priv->state + 1);
	else
		return -1;

	return 0;
}

/* ops: storage for the (autogenerated) persistent server key */

static size_t
ssh_ops_get_server_key(struct lws *wsi, uint8_t *buf, size_t len)
{
	struct per_vhost_data__lws_sshd_demo *vhd =
			(struct per_vhost_data__lws_sshd_demo *)
			lws_protocol_vh_priv_get(lws_get_vhost(wsi),
						 lws_get_protocol(wsi));
	int n;

	if (lseek(vhd->privileged_fd, 0, SEEK_SET) < 0)
		return 0;
	n = (int)read(vhd->privileged_fd, buf, (unsigned int)len);
	if (n < 0) {
		lwsl_err("%s: read failed: %d\n", __func__, n);
		n = 0;
	}

	return (size_t)n;
}

static size_t
ssh_ops_set_server_key(struct lws *wsi, uint8_t *buf, size_t len)
{
	struct per_vhost_data__lws_sshd_demo *vhd =
			(struct per_vhost_data__lws_sshd_demo *)
			lws_protocol_vh_priv_get(lws_get_vhost(wsi),
						 lws_get_protocol(wsi));
	int n;

	n = (int)write(vhd->privileged_fd, buf, (unsigned int)len);
	if (n < 0) {
		lwsl_err("%s: read failed: %d\n", __func__, errno);
		n = 0;
	}

	return (size_t)n;
}

/* ops: auth */

static int
ssh_ops_is_pubkey_authorized(const char *username, const char *type,
				 const uint8_t *peer, int peer_len)
{
	char *aps, *p, *ps;
	int n = (int)strlen(type), alen = 2048, ret = 2, len;
	size_t s = 0;

	lwsl_info("%s: checking pubkey for %s\n", __func__, username);

	s = strlen(authorized_key) + 1;

	aps = malloc(s);
	if (!aps) {
		lwsl_notice("OOM 1\n");
		goto bail_p1;
	}
	memcpy(aps, authorized_key, s);

	/* we only understand RSA */
	if (strcmp(type, "ssh-rsa")) {
		lwsl_notice("type is not ssh-rsa\n");
		goto bail_p1;
	}
	p = aps;

	if (strncmp(p, type, (unsigned int)n)) {
		lwsl_notice("lead-in string  does not match %s\n", type);
		goto bail_p1;
	}

	p += n;
	if (*p != ' ') {
		lwsl_notice("missing space at end of lead-in\n");
		goto bail_p1;
	}

	p++;
	ps = malloc((unsigned int)alen);
	if (!ps) {
		lwsl_notice("OOM 2\n");
		free(aps);
		goto bail;
	}
	len = lws_b64_decode_string(p, ps, alen);
	free(aps);
	if (len < 0) {
		lwsl_notice("key too big\n");
		goto bail;
	}

	if (peer_len > len) {
		lwsl_notice("peer_len %d bigger than decoded len %d\n",
				peer_len, len);
		goto bail;
	}

	/*
	 * once we are past that, it's the same <len32>name
	 * <len32>E<len32>N that the peer sends us
	 */
	if (memcmp(peer, ps, (unsigned int)peer_len)) {
		lwsl_info("%s: factors mismatch, rejecting key\n", __func__);
		goto bail;
	}

	lwsl_info("pubkey authorized\n");

	ret = 0;
bail:
	free(ps);

	return ret;

bail_p1:
	if (aps)
		free(aps);

	return 1;
}

static int
ssh_ops_shell(void *_priv, struct lws *wsi, lws_ssh_finish_exec finish, void *finish_handle)
{
	struct sshd_instance_priv *priv = _priv;

	/* for this demo, we don't open a real shell */

	enter_state(priv, SSH_TEST_GREET);

	return 0;
}

/* ops: banner */

static size_t
ssh_ops_banner(char *buf, size_t max_len, char *lang, size_t max_lang_len)
{
	int n = lws_snprintf(buf, max_len, "\n"
		      " |\\---/|  lws-ssh Test Server\n"
		      " | o_o |  SSH Terminal Server\n"
		      "  \\_^_/   Copyright (C) 2017 Crash Barrier Ltd\n\n");

	lws_snprintf(lang, max_lang_len, "en/US");

	return (size_t)n;
}

static void
ssh_ops_disconnect_reason(uint32_t reason, const char *desc,
			  const char *desc_lang)
{
	lwsl_notice("DISCONNECT reason 0x%X, %s (lang %s)\n", reason, desc,
		    desc_lang);
}


static const struct lws_ssh_ops ssh_ops = {
	.channel_create			= ssh_ops_channel_create,
	.channel_destroy		= ssh_ops_channel_destroy,
	.tx_waiting			= ssh_ops_tx_waiting,
	.tx				= ssh_ops_tx,
	.rx				= ssh_ops_rx,
	.get_server_key			= ssh_ops_get_server_key,
	.set_server_key			= ssh_ops_set_server_key,
	.set_env			= NULL,
	.pty_req			= NULL,
	.child_process_io		= NULL,
	.child_process_terminated	= NULL,
	.exec				= NULL,
	.shell				= ssh_ops_shell,
	.is_pubkey_authorized		= ssh_ops_is_pubkey_authorized,
	.banner				= ssh_ops_banner,
	.disconnect_reason		= ssh_ops_disconnect_reason,
	.server_string			= "SSH-2.0-Libwebsockets",
	.api_version			= 2,
};

static int
callback_lws_sshd_demo(struct lws *wsi, enum lws_callback_reasons reason,
		       void *user, void *in, size_t len)
{
	struct per_vhost_data__lws_sshd_demo *vhd =
			(struct per_vhost_data__lws_sshd_demo *)
			lws_protocol_vh_priv_get(lws_get_vhost(wsi),
						 lws_get_protocol(wsi));

	switch (reason) {
	case LWS_CALLBACK_PROTOCOL_INIT:
		vhd = lws_protocol_vh_priv_zalloc(lws_get_vhost(wsi),
						  lws_get_protocol(wsi),
				sizeof(struct per_vhost_data__lws_sshd_demo));
		if (!vhd)
			return 0;
		/*
		 * During this we still have the privs / caps we were started
		 * with.  So open an fd on the server key, either just for read
		 * or for creat / trunc if doesn't exist.  This allows us to
		 * deal with it down /etc/.. when just after this we will lose
		 * the privileges needed to read / write /etc/...
		 */
		vhd->privileged_fd = lws_open(TEST_SERVER_KEY_PATH, O_RDONLY);
		if (vhd->privileged_fd == -1)
			vhd->privileged_fd = lws_open(TEST_SERVER_KEY_PATH,
					O_CREAT | O_TRUNC | O_RDWR, 0600);
		if (vhd->privileged_fd == -1) {
			lwsl_warn("%s: Can't open %s\n", __func__,
				 TEST_SERVER_KEY_PATH);
			return 0;
		}
		break;

	case LWS_CALLBACK_PROTOCOL_DESTROY:
		if (vhd)
			close(vhd->privileged_fd);
		break;

	case LWS_CALLBACK_VHOST_CERT_AGING:
		break;

	case LWS_CALLBACK_EVENT_WAIT_CANCELLED:
		break;

	default:
		if (!vhd->ssh_base_protocol) {
			vhd->ssh_base_protocol = lws_vhost_name_to_protocol(
							lws_get_vhost(wsi),
							"lws-ssh-base");
			if (vhd->ssh_base_protocol)
				user = lws_adjust_protocol_psds(wsi,
				vhd->ssh_base_protocol->per_session_data_size);
		}

		if (vhd->ssh_base_protocol)
			return vhd->ssh_base_protocol->callback(wsi, reason,
								user, in, len);
		else
			lwsl_notice("can't find lws-ssh-base\n");
		break;
	}

	return 0;
}

#define LWS_PLUGIN_PROTOCOL_LWS_SSHD_DEMO \
	{ \
		"lws-sshd-demo", \
		callback_lws_sshd_demo, \
		0, \
		1024, /* rx buf size must be >= permessage-deflate rx size */ \
		0, (void *)&ssh_ops, 0 \
	}

#if !defined (LWS_PLUGIN_STATIC)
		
LWS_VISIBLE const struct lws_protocols lws_sshd_demo_protocols[] = {
		LWS_PLUGIN_PROTOCOL_LWS_SSHD_DEMO
};

LWS_VISIBLE const lws_plugin_protocol_t lws_sshd_demo = {
	.hdr = {
		"lws sshd demo",
		"lws_protocol_plugin",
		LWS_BUILD_HASH,
		LWS_PLUGIN_API_MAGIC
	},

	.protocols = lws_sshd_demo_protocols,
	.count_protocols = LWS_ARRAY_SIZE(lws_sshd_demo_protocols),
	.extensions = NULL,
	.count_extensions = 0,
};

#endif
