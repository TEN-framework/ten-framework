/*
 * libwebsockets - small server side websockets and web server implementation
 *
 * Copyright (C) 2010 - 2019 Andy Green <andy@warmcat.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include "private-lib-core.h"
#include "private-lib-abstract.h"

/** enum lwsgs_smtp_states - where we are in SMTP protocol sequence */
typedef enum lwsgs_smtp_states {
	LGSSMTP_IDLE,		/**< awaiting new email */
	LGSSMTP_CONNECTING,	/**< opening tcp connection to MTA */
	LGSSMTP_CONNECTED,	/**< tcp connection to MTA is connected */
		/* (server sends greeting) */
	LGSSMTP_SENT_HELO,	/**< sent the HELO */

	LGSSMTP_SENT_FROM,	/**< sent FROM */
	LGSSMTP_SENT_TO,	/**< sent TO */
	LGSSMTP_SENT_DATA,	/**< sent DATA request */
	LGSSMTP_SENT_BODY,	/**< sent the email body */

		/*
		 * (server sends, eg, "250 Ok: queued as 12345")
		 * at this point we can return to LGSSMTP_SENT_HELO and send a
		 * new email, or continue below to QUIT, or just wait
		 */

	LGSSMTP_SENT_QUIT,	/**< sent the session quit */

	/* (server sends, eg, "221 Bye" and closes the connection) */
} lwsgs_smtp_states_t;

/** abstract protocol instance data */

typedef struct lws_smtp_client_protocol {
	const struct lws_abs	*abs;
	lwsgs_smtp_states_t	estate;

	lws_smtp_email_t	*e;	/* the email we are trying to send */
	const char		*helo;

	unsigned char		send_pending:1;
} lws_smtpcp_t;

static const short retcodes[] = {
	0,	/* idle */
	0,	/* connecting */
	220,	/* connected */
	250,	/* helo */
	250,	/* from */
	250,	/* to */
	354,	/* data */
	250,	/* body */
	221,	/* quit */
};

static void
lws_smtpc_state_transition(lws_smtpcp_t *c, lwsgs_smtp_states_t s)
{
	lwsl_debug("%s: cli %p: state %d -> %d\n", __func__, c, c->estate, s);
	c->estate = s;
}

static lws_smtp_email_t *
lws_smtpc_get_email(lws_smtpcp_t *c)
{
	const lws_token_map_t *tm;

	/* ... the email we want to send */
	tm = lws_abs_get_token(c->abs->ap_tokens, LTMI_PSMTP_V_LWS_SMTP_EMAIL_T);
	if (!tm) {
		assert(0);

		return NULL;
	}

	return (lws_smtp_email_t *)tm->u.value;
}

/*
 * Called when something happened so that we know now the final disposition of
 * the email send attempt, for good or ill.
 *
 * Inform the owner via the done callback and set up the next queued one if any.
 *
 * Returns nonzero if we queued a new one
 */

static int
lws_smtpc_email_disposition(lws_smtpcp_t *c, int disp, const void *buf,
			    size_t len)
{
	lws_smtpcp_t *ch;
	lws_abs_t *ach;
	lws_dll2_t *d;

	lws_smtpc_state_transition(c, LGSSMTP_SENT_HELO);

	/* lifetime of the email object is handled by done callback */
	c->e->done(c->e, c->e->data, disp, buf, len);
	c->e = NULL;

	/* this may not be the time to try to send anything else... */

	if (disp == LWS_SMTP_DISPOSITION_FAILED_DESTROY)
		return 0;

	/* ... otherwise... do we have another queued? */

	d = lws_dll2_get_tail(&c->abs->children_owner);
	if (!d)
		return 0;

	ach = lws_container_of(d, lws_abs_t, bound);
	ch = (lws_smtpcp_t *)ach->api;

	c->e = lws_smtpc_get_email(ch);

	/* since we took it on, remove it from the queue */
	lws_dll2_remove(d);

	return 1;
}

/*
 * we became connected
 */

static int
lws_smtpc_abs_accept(lws_abs_protocol_inst_t *api)
{
	lws_smtpcp_t *c = (lws_smtpcp_t *)api;

	/* we have become connected in the tcp sense */

	lws_smtpc_state_transition(c, LGSSMTP_CONNECTED);

	/*
	 * From the accept(), the next thing that should happen is the SMTP
	 * server sends its greeting like "220 smtp2.example.com ESMTP Postfix",
	 * we'll hear about it in the rx callback, or time out
	 */

	c->abs->at->set_timeout(c->abs->ati,
				PENDING_TIMEOUT_AWAITING_SERVER_RESPONSE, 3);

	return 0;
}

static int
lws_smtpc_abs_rx(lws_abs_protocol_inst_t *api, const uint8_t *buf, size_t len)
{
	lws_smtpcp_t *c = (lws_smtpcp_t *)api;
	char dotstar[96], at[5];
	int n;

	c->abs->at->set_timeout(c->abs->ati, NO_PENDING_TIMEOUT, 0);

	lws_strncpy(at, (const char *)buf, sizeof(at));
	n = atoi(at);

	switch (c->estate) {
	case LGSSMTP_CONNECTED:
		if (n != 220) {
			/*
			 * The server did not properly greet us... we can't
			 * even get started, so fail the transport connection
			 * (and anything queued on it)
			 */

			lws_strnncpy(dotstar, (const char *)buf, len, sizeof(dotstar));
			lwsl_err("%s: server: %s\n", __func__, dotstar);

			return 1;
		}
		break;

	case LGSSMTP_SENT_BODY:
		/*
		 * We finished one way or another... let's prepare to send a
		 * new one... or wait until server hangs up on us
		 */
		if (!lws_smtpc_email_disposition(c,
					n == 250 ? LWS_SMTP_DISPOSITION_SENT :
						   LWS_SMTP_DISPOSITION_FAILED,
					"destroyed", 0))
			return 0; /* become idle */

		break; /* ask to send */

	case LGSSMTP_SENT_QUIT:
		lwsl_debug("%s: done\n", __func__);
		lws_smtpc_state_transition(c, LGSSMTP_IDLE);

		return 1;

	default:
		if (n != retcodes[c->estate]) {
			lws_strnncpy(dotstar, buf, len, sizeof(dotstar));
			lwsl_notice("%s: bad response: %d (state %d) %s\n",
				    __func__, n, c->estate, dotstar);

			lws_smtpc_email_disposition(c,
					LWS_SMTP_DISPOSITION_FAILED, buf, len);

			return 0;
		}
		break;
	}

	c->send_pending = 1;
	c->abs->at->ask_for_writeable(c->abs->ati);

	return 0;
}

static int
lws_smtpc_abs_writeable(lws_abs_protocol_inst_t *api, size_t budget)
{
	char b[256 + LWS_PRE], *p = b + LWS_PRE;
	lws_smtpcp_t *c = (lws_smtpcp_t *)api;
	int n;

	if (!c->send_pending || !c->e)
		return 0;

	c->send_pending = 0;

	lwsl_debug("%s: writing response for state %d\n", __func__, c->estate);

	switch (c->estate) {
	case LGSSMTP_CONNECTED:
		n = lws_snprintf(p, sizeof(b) - LWS_PRE, "HELO %s\n", c->helo);
		lws_smtpc_state_transition(c, LGSSMTP_SENT_HELO);
		break;

	case LGSSMTP_SENT_HELO:
		n = lws_snprintf(p, sizeof(b) - LWS_PRE, "MAIL FROM: <%s>\n",
				 c->e->from);
		lws_smtpc_state_transition(c, LGSSMTP_SENT_FROM);
		break;

	case LGSSMTP_SENT_FROM:
		n = lws_snprintf(p, sizeof(b) - LWS_PRE,
				 "RCPT TO: <%s>\n", c->e->to);
		lws_smtpc_state_transition(c, LGSSMTP_SENT_TO);
		break;

	case LGSSMTP_SENT_TO:
		n = lws_snprintf(p, sizeof(b) - LWS_PRE, "DATA\n");
		lws_smtpc_state_transition(c, LGSSMTP_SENT_DATA);
		break;

	case LGSSMTP_SENT_DATA:
		p = (char *)&c->e[1];
		n = strlen(p);
		lws_smtpc_state_transition(c, LGSSMTP_SENT_BODY);
		break;

	case LGSSMTP_SENT_BODY:
		n = lws_snprintf(p, sizeof(b) - LWS_PRE, "quit\n");
		lws_smtpc_state_transition(c, LGSSMTP_SENT_QUIT);
		break;

	case LGSSMTP_SENT_QUIT:
		return 0;

	default:
		return 0;
	}

	//puts(p);
	c->abs->at->tx(c->abs->ati, (uint8_t *)p, n);

	return 0;
}

static int
lws_smtpc_abs_closed(lws_abs_protocol_inst_t *api)
{
	lws_smtpcp_t *c = (lws_smtpcp_t *)api;

	if (c)
		lws_smtpc_state_transition(c, LGSSMTP_IDLE);

	return 0;
}

/*
 * Creating for initial transport and for piggybacking on another transport
 * both get created here the same.  But piggybackers have ai->bound attached.
 */

static int
lws_smtpc_create(const lws_abs_t *ai)
{
	lws_smtpcp_t *c = (lws_smtpcp_t *)ai->api;

	memset(c, 0, sizeof(*c));

	c->abs = ai;
	c->e = lws_smtpc_get_email(c);

	lws_smtpc_state_transition(c, lws_dll2_is_detached(&ai->bound) ?
					LGSSMTP_CONNECTING : LGSSMTP_IDLE);

	/* If we are initiating the transport, we will get an accept() next...
	 *
	 * If we are piggybacking, the parent will get a .child_bind() after
	 * this to give it a chance to act on us joining (eg, it was completely
	 * idle and we joined).
	 */

	return 0;
}

static void
lws_smtpc_destroy(lws_abs_protocol_inst_t **_c)
{
	lws_smtpcp_t *c = (lws_smtpcp_t *)*_c;

	if (!c)
		return;

	/* so if we are still holding on to c->e, we have failed to send it */
	if (c->e)
		lws_smtpc_email_disposition(c,
			LWS_SMTP_DISPOSITION_FAILED_DESTROY, "destroyed", 0);

	*_c = NULL;
}

static int
lws_smtpc_compare(lws_abs_t *abs1, lws_abs_t *abs2)
{
	return 0;
}

static int
lws_smtpc_child_bind(lws_abs_t *abs)
{
	return 0;
}

/* events the transport invokes (handled by abstract protocol) */

const lws_abs_protocol_t lws_abs_protocol_smtp = {
	.name		= "smtp",
	.alloc		= sizeof(lws_smtpcp_t),
	.flags		= LWSABSPR_FLAG_PIPELINE,

	.create		= lws_smtpc_create,
	.destroy	= lws_smtpc_destroy,
	.compare	= lws_smtpc_compare,

	.accept		= lws_smtpc_abs_accept,
	.rx		= lws_smtpc_abs_rx,
	.writeable	= lws_smtpc_abs_writeable,
	.closed		= lws_smtpc_abs_closed,
	.heartbeat	= NULL,

	.child_bind	= lws_smtpc_child_bind,
};
