/*
 * Abstract SMTP support for libwebsockets - SMTP sequencer
 *
 * Copyright (C) 2016-2019 Andy Green <andy@warmcat.com>
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
 *
 * This sequencer sits above the abstract protocol, and manages queueing,
 * retrying mail transmission, and retry limits.
 *
 * Having the sequencer means that, eg, we can manage retries after complete
 * connection failure.
 *
 * Connections to the smtp server are serialized
 */

#include "private-lib-core.h"
#include "private-lib-abstract-protocols-smtp.h"

typedef enum {
	LSMTPSS_DISCONNECTED,
	LSMTPSS_CONNECTING,
	LSMTPSS_CONNECTED,
	LSMTPSS_BUSY,
} smtpss_connstate_t;

typedef struct lws_smtp_sequencer {
	struct lws_dll2_owner		emails_owner; /* email queue */

	lws_abs_t			*abs, *instance;
	lws_smtp_sequencer_args_t	args;
	struct lws_sequencer		*seq;

	smtpss_connstate_t		connstate;

	time_t				email_connect_started;

	/* holds the HELO for the smtp protocol to consume */
	lws_token_map_t			apt[3];
} lws_smtp_sequencer_t;

/* sequencer messages specific to this sequencer */

enum {
	SEQ_MSG_CLIENT_FAILED = LWSSEQ_USER_BASE,
	SEQ_MSG_CLIENT_DONE,
};

/*
 * We're going to bind to the raw-skt transport, so tell that what we want it
 * to connect to
 */

static const lws_token_map_t smtp_rs_transport_tokens[] = {
 {
	.u = { .value = "127.0.0.1" },
	.name_index = LTMI_PEER_V_DNS_ADDRESS,
 }, {
	.u = { .lvalue = 25 },
	.name_index = LTMI_PEER_LV_PORT,
 }, {
 }
};

static void
lws_smtpc_kick_internal(lws_smtp_sequencer_t *s)
{
	lws_smtp_email_t *e;
	lws_dll2_t *d;
	char buf[64];
	int n;
	lws_dll2_t *pd2;

	pd2 = lws_dll2_get_head(&s->emails_owner);
	if (!pd2)
		return;

	e = lws_container_of(pd2, lws_smtp_email_t, list);
	if (!e)
		return;

	/* Is there something to do?  If so, we need a connection... */

	if (s->connstate == LSMTPSS_DISCONNECTED) {

		s->apt[0].u.value = s->args.helo;
		s->apt[0].name_index = LTMI_PSMTP_V_HELO;
		s->apt[1].u.value = (void *)e;
		s->apt[1].name_index = LTMI_PSMTP_V_LWS_SMTP_EMAIL_T;

		/*
		 * create and connect the smtp protocol + transport
		 */

		s->abs = lws_abstract_alloc(s->args.vhost, NULL, "smtp.raw_skt",
					    s->apt, smtp_rs_transport_tokens,
					    s->seq, NULL);
		if (!s->abs)
			return;

		s->instance = lws_abs_bind_and_create_instance(s->abs);
		if (!s->instance) {
			lws_abstract_free(&s->abs);
			lwsl_err("%s: failed to create SMTP client\n", __func__);

			goto bail1;
		}

		s->connstate = LSMTPSS_CONNECTING;
		lws_seq_timeout_us(s->seq, 10 * LWS_USEC_PER_SEC);
		return;
	}

	/* ask the transport if we have a connection to the server ongoing */

	if (s->abs->at->state(s->abs->ati)) {
		/*
		 * there's a connection, it could be still trying to connect
		 * or established
		 */
		s->abs->at->ask_for_writeable(s->abs->ati);

		return;
	}

	/* there's no existing connection */

	lws_smtpc_state_transition(c, LGSSMTP_CONNECTING);

	if (s->abs->at->client_conn(s->abs)) {
		lwsl_err("%s: failed to connect\n", __func__);

		return;
	}

	e->tries++;
	e->last_try = lws_now_secs();
}


/*
 * The callback we get from the smtp protocol... we use this to drive
 * decisions about destroy email, retry and fail.
 *
 * Sequencer will handle it via the event loop.
 */

static int
email_result(void *e, void *d, int disp, void *b, size_t l)
{
	lws_smtp_sequencer_t *s = (lws_smtp_sequencer_t *)d;

	lws_sequencer_event(s->seq, LWSSEQ_USER_BASE + disp, e);

	return 0;
}

static int
cleanup(struct lws_dll2 *d, void *user)
{
	lws_smtp_email_t *e;

	e = lws_container_of(d, lws_smtp_email_t, list);
	if (e->done)
		e->done(e, "destroying", 10);

	lws_dll2_remove(d);
	lws_free(e);

	return 0;
}

static lws_seq_cb_return_t
smtp_sequencer_cb(struct lws_sequencer *seq, void *user, int event, void *data)
{
	struct lws_smtp_sequencer_t *s = (struct lws_smtp_sequencer_t *)user;

	switch ((int)event) {
	case LWSSEQ_CREATED: /* our sequencer just got started */
		lwsl_notice("%s: %s: created\n", __func__,
			    lws_sequencer_name(seq));
		s->connstate = LSMTPSS_DISCONNECTED;
		s->state = 0;  /* first thing we'll do is the first url */
		goto step;

	case LWSSEQ_DESTROYED:
		lws_dll2_foreach_safe(&s->pending_owner, NULL, cleanup);
		break;

	case LWSSEQ_TIMED_OUT:
		lwsl_notice("%s: LWSSEQ_TIMED_OUT\n", __func__);
		break;

	case LWSSEQ_USER_BASE + LWS_SMTP_DISPOSITION_SENT:
		lws_smtpc_free_email(data);
		break;

	case LWSSEQ_WSI_CONNECTED:
		s->connstate = LSMTPSS_CONNECTED;
		lws_smtpc_kick_internal(s);
		break;

	case LWSSEQ_WSI_CONN_FAIL:
	case LWSSEQ_WSI_CONN_CLOSE:
		s->connstate = LSMTPSS_DISCONNECTED;
		lws_smtpc_kick_internal(s);
		break;

	case SEQ_MSG_SENT:
		break;

	default:
		break;
	}

	return LWSSEQ_RET_CONTINUE;
}

/*
 * Creates an lws_sequencer to manage the test sequence
 */

lws_smtp_sequencer_t *
lws_smtp_sequencer_create(const lws_smtp_sequencer_args_t *args)
{
	lws_smtp_sequencer_t *s;
	struct lws_sequencer *seq;

	/*
	 * Create a sequencer in the event loop to manage the SMTP queue
	 */

	seq = lws_sequencer_create(args->vhost->context, 0,
				   sizeof(lws_smtp_sequencer_t), (void **)&s,
				   smtp_sequencer_cb, "smtp-seq");
	if (!seq) {
		lwsl_err("%s: unable to create sequencer\n", __func__);
		return NULL;
	}

	s->abs = *args->abs;
	s->args = *args;
	s->seq = seq;

	/* set defaults in our copy of the args */

	if (!s->args.helo[0])
		strcpy(s->args.helo, "default-helo");
	if (!s->args.email_queue_max)
		s->args.email_queue_max = 8;
	if (!s->args.retry_interval)
		s->args.retry_interval = 15 * 60;
	if (!s->args.delivery_timeout)
		s->args.delivery_timeout = 12 * 60 * 60;

	return s;
}

void
lws_smtp_sequencer_destroy(lws_smtp_sequencer_t *s)
{
	/* sequencer destruction destroys all assets */
	lws_sequencer_destroy(&s->seq);
}

int
lws_smtpc_add_email(lws_smtp_sequencer_t *s, const char *payload,
		    size_t payload_len, const char *sender,
		    const char *recipient, void *data, lws_smtp_cb_t done)
{
	lws_smtp_email_t *e;

	if (s->emails_owner.count > s->args.email_queue_max) {
		lwsl_err("%s: email queue at limit of %d\n", __func__,
			 (int)s->args.email_queue_max);

		return 1;
	}

	if (!done)
		return 1;

	e = malloc(sizeof(*e) + payload_len + 1);
	if (!e)
		return 1;

	memset(e, 0, sizeof(*e));

	e->data = data;
	e->done = done;

	lws_strncpy(e->from, sender, sizeof(e->from));
	lws_strncpy(e->to, recipient, sizeof(e->to));

	memcpy((char *)&e[1], payload, payload_len + 1);

	e->added = lws_now_secs();
	e->last_try = 0;
	e->tries = 0;

	lws_dll2_clear(&e->list);
	lws_dll2_add_tail(&e->list, &s->emails_owner);

	lws_smtpc_kick_internal(s);

	return 0;
}
