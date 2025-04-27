#define LWS_SMTP_MAX_EMAIL_LEN 32


/*
 * These are allocated on to the heap with an over-allocation to hold the
 * payload at the end
 */

typedef struct lws_smtp_email {
	struct lws_dll2	list;
	void		*data;

	char		from[LWS_SMTP_MAX_EMAIL_LEN];
	char		to[LWS_SMTP_MAX_EMAIL_LEN];

	time_t		added;
	time_t		last_try;

	lws_smtp_cb_t	done;

	int		tries;

	/* email payload follows */
} lws_smtp_email_t;
