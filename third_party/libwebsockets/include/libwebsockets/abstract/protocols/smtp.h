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

/** \defgroup smtp SMTP related functions
 * ##SMTP related functions
 * \ingroup lwsapi
 *
 * These apis let you communicate with a local SMTP server to send email from
 * lws.  It handles all the SMTP sequencing and protocol actions.
 *
 * Your system should have postfix, sendmail or another MTA listening on port
 * 25 and able to send email using the "mail" commandline app.  Usually distro
 * MTAs are configured for this by default.
 *
 * You can either use the abstract protocol layer directly, or instead use the
 * provided smtp sequencer... this takes care of creating the protocol
 * connections, and provides and email queue and retry management.
 */
//@{

#if defined(LWS_WITH_SMTP)

enum {
	LTMI_PSMTP_V_HELO = LTMI_PROTOCOL_BASE,		/* u.value */

	LTMI_PSMTP_V_LWS_SMTP_EMAIL_T,			/* u.value */
};

enum {
	LWS_SMTP_DISPOSITION_SENT,
	LWS_SMTP_DISPOSITION_FAILED,
	LWS_SMTP_DISPOSITION_FAILED_DESTROY
};

typedef struct lws_smtp_sequencer_args {
	const char		helo[32];
	struct lws_vhost	*vhost;
	time_t			retry_interval;
	time_t			delivery_timeout;
	size_t			email_queue_max;
	size_t			max_content_size;
} lws_smtp_sequencer_args_t;

typedef struct lws_smtp_sequencer lws_smtp_sequencer_t;
typedef struct lws_smtp_email lws_smtp_email_t;

LWS_VISIBLE LWS_EXTERN lws_smtp_sequencer_t *
lws_smtp_sequencer_create(const lws_smtp_sequencer_args_t *args);

LWS_VISIBLE LWS_EXTERN void
lws_smtp_sequencer_destroy(lws_smtp_sequencer_t *s);

typedef int (*lws_smtp_cb_t)(void *e, void *d, int disp, const void *b, size_t l);
typedef struct lws_smtp_email lws_smtp_email_t;

/**
 * lws_smtpc_add_email() - Allocates and queues an email object
 *
 * \param s: smtp sequencer to queue on
 * \param payload: the email payload string, with headers and terminating .
 * \param payload_len: size in bytes of the payload string
 * \param sender: the sender name and email
 * \param recipient: the recipient name and email
 * \param data: opaque user data returned in the done callback
 * \param done: callback called when the email send succeeded or failed
 *
 * Allocates an email object and copies the payload, sender and recipient into
 * it and initializes it.  Returns NULL if OOM, otherwise the allocated email
 * object.
 *
 * Because it copies the arguments into an allocated buffer, the original
 * arguments can be safely destroyed after calling this.
 *
 * The done() callback must free the email object.  It doesn't have to free any
 * individual members.
 */
LWS_VISIBLE LWS_EXTERN int
lws_smtpc_add_email(lws_smtp_sequencer_t *s, const char *payload,
		    size_t payload_len, const char *sender,
		    const char *recipient, void *data, lws_smtp_cb_t done);

/**
 * lws_smtpc_free_email() - Add email to the list of ones being sent
 *
 * \param e: email to queue for sending on \p c
 *
 * Adds an email to the linked-list of emails to send
 */
LWS_VISIBLE LWS_EXTERN int
lws_smtpc_free_email(lws_smtp_email_t *e);


#endif
//@}
