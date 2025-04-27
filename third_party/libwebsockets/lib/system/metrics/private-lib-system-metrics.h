/*
 * lws System Metrics
 *
 * Copyright (C) 2021 Andy Green <andy@warmcat.com>
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

/*
 * Const struct that describes a policy for processing raw metrics to turn them
 * into events.
 *
 * Typically although we want to monitor every event, the data produced can be
 * too large, and many events that are "normal" just need to be counted as such;
 * outliers or change-to-continuous outliers may deserve closer recording as
 * events in their own right.
 *
 * Mean computation must "decay" as it ages, we do this by halving the sum and
 * count after .us_decay_unit us.
 *
 * We don't acknowledge outliers until there are at least .min_contributors
 * in the current mean (which is subject to decaying)
 *
 * We decide something is an outlier event if it deviates from the mean by
 * .pc_outlier_deviation %.
 */

/*
 * The dynamic counterpart for each static metric policy, this is on heap
 * one per const lws_metric_policy_t.  It's listed in context->owner_mtr_dynpol
 */

typedef struct lws_metric_policy_dyn {
	const lws_metric_policy_t	*policy;
	/**< the static part of the policy we belong to... can be NULL if no
	 * policy matches or the policy was invalidated */

	lws_dll2_owner_t		owner;
	/**< list of metrics that are using this policy */

	lws_dll2_t			list;
	/**< context owns us */

	lws_sorted_usec_list_t		sul;
	/**< schedule periodic reports for metrics using this policy */
} lws_metric_policy_dyn_t;

/*
 * A metrics private part, encapsulating the public part
 */

typedef struct lws_metric {

	lws_dll2_t			list;
	/**< owned by either 1) ctx.lws_metric_policy_dyn_t.owner, or
	 * 2) ctx.owner_mtr_no_pol */

	struct lws_context		*ctx;

	/* public part overallocated */
} lws_metric_t;


#if defined(LWS_WITH_SYS_METRICS)
#define lws_metrics_hist_bump_priv(_mt, _name) \
		lws_metrics_hist_bump_(lws_metrics_priv_to_pub(_mt), _name)
#define lws_metrics_hist_bump_priv_wsi(_wsi, _hist, _name) \
		lws_metrics_hist_bump_(lws_metrics_priv_to_pub(_wsi->a.context->_hist), _name)
#define lws_metrics_hist_bump_priv_ss(_ss, _hist, _name) \
		lws_metrics_hist_bump_(lws_metrics_priv_to_pub(_ss->context->_hist), _name)
#define lws_metrics_priv_to_pub(_x) ((lws_metric_pub_t *)&(_x)[1])
#else
#define lws_metrics_hist_bump_priv(_mt, _name)
#define lws_metrics_hist_bump_priv_wsi(_wsi, _hist, _name)
#define lws_metrics_hist_bump_priv_ss(_ss, _hist, _name)
#define lws_metrics_priv_to_pub(_x) ((lws_metric_pub_t *)NULL)
#endif

#if defined(LWS_WITH_SECURE_STREAMS_PROXY_API)
/*
 * sspc-specific version that also appends the tag value to the lifecycle tag
 * used for logging the sspc identity
 */
int
lws_metrics_tag_sspc_add(struct lws_sspc_handle *ss, const char *name, const char *val);
#endif

int
lws_metrics_register_policy(struct lws_context *ctx,
			    const lws_metric_policy_t *head);

void
lws_metrics_destroy(struct lws_context *ctx);

void
lws_metric_event(lws_metric_t *mt, char go_nogo, u_mt_t val);

lws_metric_t *
lws_metric_create(struct lws_context *ctx, uint8_t flags, const char *name);

int
lws_metric_destroy(lws_metric_t **mt, int keep);

void
lws_metric_policy_dyn_destroy(lws_metric_policy_dyn_t *dm, int keep);

void
lws_metric_rebind_policies(struct lws_context *ctx);
