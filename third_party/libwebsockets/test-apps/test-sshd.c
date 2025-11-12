/*
 * Example embedded sshd server using libwebsockets sshd plugin
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
 * may be proprietary.	So unlike the library itself, they are licensed
 * Public Domain.
 *
 *
 * This test app listens on port 2200 for authorized ssh connections.  Run it
 * using
 *
 * $ sudo libwebsockets-test-sshd
 *
 * Connect to it using the test private key with:
 *
 * $ ssh -p 2200 -i /usr/local/share/libwebsockets-test-server/lws-ssh-test-keys anyuser@127.0.0.1
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
/* import the whole of lws-plugin-sshd-base statically */
#include <lws-plugin-sshd-static-build-includes.h>

/*
 * We store the test server's own key here (will be created with a new
 * random key if it doesn't exist
 *
 * The /etc path is the only reason we have to run as root.
 */
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

static struct lws_context *context = NULL;
static volatile char force_exit = 0;

/*
 * These are our "ops" that form our customization of, and interface to, the
 * generic sshd plugin.
 *
 * The priv struct contains our data we want to associate to each channel
 * individually.
 */

struct sshd_instance_priv {
	struct lws_protocol_vhost_options *env;
	struct lws_ring	*ring_stdout;
	struct lws_ring	*ring_stderr;

	struct lws 	*wsi_stdout;
	struct lws 	*wsi_stderr;

	uint32_t	pty_in_bloat_nl_to_crnl:1;
	uint32_t	pty_in_echo:1;
	uint32_t	pty_in_cr_to_nl:1;

	uint32_t	insert_lf:1;
};


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

	priv->ring_stdout = lws_ring_create(1, 1024, NULL);
	if (!priv->ring_stdout) {
		free(priv);

		return 1;
	}

	priv->ring_stderr = lws_ring_create(1, 1024, NULL);
	if (!priv->ring_stderr) {
		lws_ring_destroy(priv->ring_stdout);
		free(priv);

		return 1;
	}

	return 0;
}

static int
ssh_ops_channel_destroy(void *_priv)
{
	struct sshd_instance_priv *priv = _priv;
	const struct lws_protocol_vhost_options *pvo = priv->env, *pvo1;

	while (pvo) {
		pvo1 = pvo;
		free((char *)pvo->name);
		free((char *)pvo->value);
		pvo = pvo->next;
		free((void *)pvo1);
	}
	priv->env = NULL;

	lws_ring_destroy(priv->ring_stdout);
	lws_ring_destroy(priv->ring_stderr);
	free(priv);

	return 0;
}

/* ops: IO */

static int
ssh_ops_tx_waiting(void *_priv)
{
	struct sshd_instance_priv *priv = _priv;
	int s = 0;

	if (lws_ring_get_count_waiting_elements(priv->ring_stdout, NULL))
		s |= LWS_STDOUT;
	if (lws_ring_get_count_waiting_elements(priv->ring_stderr, NULL))
		s |= LWS_STDERR;

	return s;
}

static size_t
ssh_ops_tx(void *_priv, int stdch, uint8_t *buf, size_t len)
{
	struct sshd_instance_priv *priv = _priv;
	struct lws_ring *r;
	struct lws *wsi;
	size_t n;

	if (stdch == LWS_STDOUT) {
		r = priv->ring_stdout;
		wsi = priv->wsi_stdout;
	} else {
		r = priv->ring_stderr;
		wsi = priv->wsi_stderr;
	}

	n = lws_ring_consume(r, NULL, buf, len);

	if (n)
		lws_rx_flow_control(wsi, 1);

	return n;
}


static int
ssh_ops_rx(void *_priv, struct lws *wsi, const uint8_t *buf, uint32_t len)
{
	struct sshd_instance_priv *priv = _priv;
	struct lws *wsi_stdin = lws_cgi_get_stdwsi(wsi, LWS_STDIN);
	int fd;
	uint8_t bbuf[256];

	if (!wsi_stdin)
		return -1;

	fd = lws_get_socket_fd(wsi_stdin);

	if (*buf != 0x0d) {
		if (write(fd, buf, len) != (int)len)
			return -1;
		if (priv->pty_in_echo) {
			if (!lws_ring_insert(priv->ring_stdout, buf, 1))
				lwsl_notice("dropping...\n");
			lws_callback_on_writable(wsi);
		}
	} else {
		bbuf[0] = 0x0a;
		bbuf[1] = 0x0a;
		if (write(fd, bbuf, 1) != 1)
			return -1;

		if (priv->pty_in_echo) {
			bbuf[0] = 0x0d;
			bbuf[1] = 0x0a;
			if (!lws_ring_insert(priv->ring_stdout, bbuf, 2))
				lwsl_notice("dropping...\n");
			lws_callback_on_writable(wsi);
		}
	}

	return 0;
}

/* ops: storage for the (autogenerated) persistent server key */

static size_t
ssh_ops_get_server_key(struct lws *wsi, uint8_t *buf, size_t len)
{
	int fd = lws_open(TEST_SERVER_KEY_PATH, O_RDONLY), n;

	if (fd == -1) {
		lwsl_err("%s: unable to open %s for read: %s\n", __func__,
				TEST_SERVER_KEY_PATH, strerror(errno));

		return 0;
	}

	n = (int)read(fd, buf, len);
	if (n < 0) {
		lwsl_err("%s: read failed: %d\n", __func__, n);
		n = 0;
	}

	close(fd);

	return (size_t)n;
}

static size_t
ssh_ops_set_server_key(struct lws *wsi, uint8_t *buf, size_t len)
{
	int fd = lws_open(TEST_SERVER_KEY_PATH, O_CREAT | O_TRUNC | O_RDWR, 0600);
	int n;

	lwsl_notice("%s: %d\n", __func__, fd);
	if (fd == -1) {
		lwsl_err("%s: unable to open %s for write: %s\n", __func__,
				TEST_SERVER_KEY_PATH, strerror(errno));

		return 0;
	}

	n = (int)write(fd, buf, len);
	if (n < 0) {
		lwsl_err("%s: read failed: %d\n", __func__, errno);
		n = 0;
	}

	close(fd);

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

	/* this is all we understand at the moment */
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

	if (lws_timingsafe_bcmp(peer, ps, (uint32_t)peer_len)) {
		lwsl_info("factors mismatch\n");
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

/* ops: spawn subprocess */

static int
ssh_cgi_env_add(struct sshd_instance_priv *priv, const char *name,
		const char *value)
{
	struct lws_protocol_vhost_options *pvo = malloc(sizeof(*pvo));

	if (!pvo)
		return 1;

	pvo->name = malloc(strlen(name) + 1);
	if (!pvo->name) {
		free(pvo);
		return 1;
	}

	pvo->value = malloc(strlen(value) + 1);
	if (!pvo->value) {
		free((char *)pvo->name);
		free(pvo);
		return 1;
	}

	strcpy((char *)pvo->name, name);
	strcpy((char *)pvo->value, value);

	pvo->next = priv->env;
	priv->env = pvo;

	lwsl_notice("%s: ENV %s <- %s\n", __func__, name, value);

	return 0;
}

static int
ssh_ops_set_env(void *_priv, const char *name, const char *value)
{
	struct sshd_instance_priv *priv = _priv;

	return ssh_cgi_env_add(priv, name, value);
}


static int
ssh_ops_pty_req(void *_priv, struct lws_ssh_pty *pty)
{
	struct sshd_instance_priv *priv = _priv;
	uint8_t *p = (uint8_t *)pty->modes, opc;
	uint32_t arg;

	lwsl_notice("%s: pty term %s, modes_len %d\n", __func__, pty->term,
		    pty->modes_len);

	ssh_cgi_env_add(priv, "TERM", pty->term);

	while (p < (uint8_t *)pty->modes + pty->modes_len) {
		if (*p >= 160)
			break;
		if (!*p)
			break;
		opc = *p++;

		arg = (uint32_t)(*p++ << 24);
		arg |= (uint32_t)(*p++ << 16);
		arg |= (uint32_t)(*p++ << 8);
		arg |= (uint32_t)(*p++);

		lwsl_debug("pty opc %d: 0x%x\n", opc, arg);

		switch (opc) {
		case SSHMO_ICRNL:
			priv->pty_in_cr_to_nl = !!arg;
			lwsl_notice(" SSHMO_ICRNL: %d\n", !!arg);
			break;
		case SSHMO_ONLCR:
			priv->pty_in_bloat_nl_to_crnl = !!arg;
			lwsl_notice(" SSHMO_ONLCR: %d\n", !!arg);
			break;
		case SSHMO_ECHO:
//			priv->pty_in_echo = !!arg;
			lwsl_notice(" SSHMO_ECHO: %d\n", !!arg);
			break;
		}
	}

	return 0;
}

static int
ssh_ops_child_process_io(void *_priv, struct lws *wsi,
			 struct lws_cgi_args *args)
{
	struct sshd_instance_priv *priv = _priv;
	struct lws_ring *r = priv->ring_stdout;
	void *rp;
	size_t bytes;
	int n, m;

	priv->wsi_stdout = args->stdwsi[LWS_STDOUT];
	priv->wsi_stderr = args->stdwsi[LWS_STDERR];

	switch (args->ch) {
	case LWS_STDIN:
		lwsl_notice("STDIN\n");
		break;

	case LWS_STDERR:
		r = priv->ring_stderr;
		/* fallthru */
	case LWS_STDOUT:
		if (lws_ring_next_linear_insert_range(r, &rp, &bytes) ||
		    bytes < 1) {
			lwsl_notice("bytes %d\n", (int)bytes);
			/* no room in the fifo */
			break;
		}
		if (priv->pty_in_bloat_nl_to_crnl) {
			uint8_t buf[256], *p, *d;

			if (bytes != 1)
				n = (int)(bytes / 2);
			else
				n = 1;
			if (n > (int)sizeof(buf))
				n = sizeof(buf);

			if (!n)
				break;

			m = lws_get_socket_fd(args->stdwsi[args->ch]);
			if (m < 0)
				return -1;
			n = (int)read(m, buf, (unsigned int)n);
			if (n < 0)
				return -1;
			if (n == 0) {
				lwsl_notice("zero length stdin %d\n", n);
				break;
			}
			m = 0;
			p = rp;
			d = buf;
			while (m++ < n) {
				if (priv->insert_lf) {
					priv->insert_lf = 0;
					*p++ = 0x0d;
				}
				if (*d == 0x0a)
					priv->insert_lf = 1;

				*p++ = *d++;
			}
			n = lws_ptr_diff((void *)p, rp);
			if (n < (int)bytes && priv->insert_lf) {
				priv->insert_lf = 0;
				*p++ = 0x0d;
				n++;
			}
		} else {
			n = lws_get_socket_fd(args->stdwsi[args->ch]);
			if (n < 0)
				return -1;
			n = (int)read(n, rp, bytes);
			if (n < 0)
				return -1;
		}

		lws_rx_flow_control(args->stdwsi[args->ch], 0);

		lws_ring_bump_head(r, (unsigned int)n);
		lws_callback_on_writable(wsi);
		break;
	}

	return 0;
}

static int
ssh_ops_child_process_terminated(void *priv, struct lws *wsi)
{
	lwsl_notice("%s\n", __func__);
	return -1;
}

static int
ssh_ops_exec(void *_priv, struct lws *wsi, const char *command, lws_ssh_finish_exec finish, void *finish_handle)
{
	lwsl_notice("%s: EXEC %s\n", __func__, command);

	/* we don't want to exec anything */
	return 1;
}

static int
ssh_ops_shell(void *_priv, struct lws *wsi, lws_ssh_finish_exec finish, void *finish_handle)
{
	struct sshd_instance_priv *priv = _priv;
	const char *cmd[] = {
		"/bin/bash",
		"-i",
		"-l",
		NULL
	};
	lwsl_notice("%s: SHELL\n", __func__);

	if (lws_cgi(wsi, cmd, -1, 0, priv->env)) {
		lwsl_notice("shell spawn failed\n");
		return -1;
	}

	return 0;
}

/* ops: banner */

static size_t
ssh_ops_banner(char *buf, size_t max_len, char *lang, size_t max_lang_len)
{
	int n = lws_snprintf(buf, max_len, "\n"
		      " |\\---/|  lws-ssh Test Server\n"
		      " | o_o |  SSH Terminal Server\n"
		      "  \\_^_/   Copyright (C) 2017-2020 Crash Barrier Ltd\n\n");

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
	.set_env			= ssh_ops_set_env,
	.pty_req			= ssh_ops_pty_req,
	.child_process_io		= ssh_ops_child_process_io,
	.child_process_terminated	= ssh_ops_child_process_terminated,
	.exec				= ssh_ops_exec,
	.shell				= ssh_ops_shell,
	.is_pubkey_authorized		= ssh_ops_is_pubkey_authorized,
	.banner				= ssh_ops_banner,
	.disconnect_reason		= ssh_ops_disconnect_reason,
	.server_string			= "SSH-2.0-Libwebsockets",
	.api_version			= 2,
};

/*
 * use per-vhost options to bind the ops struct to the instance of the
 * "lws_raw_sshd" protocol instantiated on our vhost
 */

static const struct lws_protocol_vhost_options pvo_ssh_ops = {
	NULL,
	NULL,
	"ops",
	(void *)&ssh_ops
};

static const struct lws_protocol_vhost_options pvo_ssh = {
	NULL,
	&pvo_ssh_ops,
	"lws-ssh-base",
	"" /* ignored, just matches the protocol name above */
};

void sighandler(int sig)
{
	force_exit = 1;
	lws_cancel_service(context);
}

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


const struct lws_protocols lws_sshd_demo_protocols[] = {
	{
		"lws-sshd-demo",
		callback_lws_sshd_demo,
		0,
		1024, /* rx buf size must be >= permessage-deflate rx size */
		0, (void *)&ssh_ops, 0
	}

};


int main()
{
	static struct lws_context_creation_info info;
	struct lws_vhost *vh_sshd;
	int ret = 1, n;

	/* info is on the stack, it must be cleared down before use */
	memset(&info, 0, sizeof(info));

	signal(SIGINT, sighandler);
	lws_set_log_level(LLL_ERR | LLL_WARN | LLL_NOTICE
			/*| LLL_INFO */
			/* | LLL_DEBUG */, NULL);

	lwsl_notice("lws test-sshd -- Copyright (C) 2017 <andy@warmcat.com>\n");

	/* create the lws context */

	info.options = LWS_SERVER_OPTION_EXPLICIT_VHOSTS |
		       LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;

	context = lws_create_context(&info);
	if (!context) {
		lwsl_err("Failed to create context\n");
		return 1;
	}

	/* create our listening vhost */

	info.port = 2200;
	info.options = LWS_SERVER_OPTION_ONLY_RAW;
	info.vhost_name = "sshd";
	info.protocols = lws_sshd_demo_protocols;
	info.pvo = &pvo_ssh;

	vh_sshd = lws_create_vhost(context, &info);
	if (!vh_sshd) {
		lwsl_err("Failed to create sshd vhost\n");
		goto bail;
	}

	/* spin doing service */

	n = 0;
	while (!n  && !force_exit)
		n = lws_service(context, 0);

	ret = 0;

	/* cleanup */

bail:
	lws_context_destroy(context);
	lwsl_notice("exiting...\n");

	return ret;
}
