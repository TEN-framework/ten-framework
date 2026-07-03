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

typedef struct lws_retry_bo {
	const uint32_t	*retry_ms_table;	   /* base delay in ms */
	uint16_t	retry_ms_table_count;      /* entries in table */
	uint16_t	conceal_count;		   /* max retries to conceal */
	uint16_t	secs_since_valid_ping;     /* idle before PING issued */
	uint16_t	secs_since_valid_hangup;   /* idle before hangup conn */
	uint8_t		jitter_percent;		/* % additional random jitter */
} lws_retry_bo_t;

#define LWS_RETRY_CONCEAL_ALWAYS (0xffff)

/**
 * lws_retry_get_delay_ms() - get next delay from backoff table
 *
 * \param lws_context: the lws context (used for getting random)
 * \param retry: the retry backoff table we are using, or NULL for default
 * \param ctry: pointer to the try counter
 * \param conceal: pointer to flag set to nonzero if the try should be concealed
 *			in terms of creating an error
 *
 * Increments *\p try and retruns the number of ms that should elapse before the
 * next connection retry, according to the backoff table \p retry. *\p conceal is
 * set if the number of tries is less than the backoff table conceal_count, or
 * is zero if it exceeded it.  This lets you conceal a certain number of retries
 * before alerting the caller there is a problem.
 *
 * If \p retry is NULL, a default of 3s + (0..300ms jitter) is used.  If it's
 * non-NULL but jitter_percent is 0, the default of 30% jitter is retained.
 */

LWS_VISIBLE LWS_EXTERN unsigned int
lws_retry_get_delay_ms(struct lws_context *context, const lws_retry_bo_t *retry,
		       uint16_t *ctry, char *conceal);

/**
 * lws_retry_sul_schedule() - schedule a sul according to the backoff table
 *
 * \param lws_context: the lws context (used for getting random)
 * \param sul: pointer to the sul to schedule
 * \param retry: the retry backoff table we are using, or NULL for default
 * \param cb: the callback for when the sul schedule time arrives
 * \param ctry: pointer to the try counter
 *
 * Helper that combines interpreting the retry table with scheduling a sul to
 * the computed delay.  If conceal is not set, it will not schedule the sul
 * and just return 1.  Otherwise the sul is scheduled and it returns 0.
 */
LWS_VISIBLE LWS_EXTERN int
lws_retry_sul_schedule(struct lws_context *context, int tid,
		       lws_sorted_usec_list_t *sul, const lws_retry_bo_t *retry,
		       sul_cb_t cb, uint16_t *ctry);

/**
 * lws_retry_sul_schedule_retry_wsi() - retry sul schedule helper using wsi
 *
 * \param wsi: the wsi to set the hrtimer sul on to the next retry interval
 * \param sul: pointer to the sul to schedule
 * \param cb: the callback for when the sul schedule time arrives
 * \param ctry: pointer to the try counter
 *
 * Helper that uses context, tid and retry policy from a wsi to call
 * lws_retry_sul_schedule.
 *
 * Since a udp connection can have many writes in flight, the retry count and
 * the sul used to track each thing that wants to be written have to be handled
 * individually, not the wsi.  But the retry policy and the other things can
 * be filled in from the wsi conveniently.
 */
LWS_VISIBLE LWS_EXTERN int
lws_retry_sul_schedule_retry_wsi(struct lws *wsi, lws_sorted_usec_list_t *sul,
				 sul_cb_t cb, uint16_t *ctry);
