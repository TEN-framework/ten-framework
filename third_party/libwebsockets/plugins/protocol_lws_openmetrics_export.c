/*
 * libwebsockets-test-server - libwebsockets test implementation
 *
 * Written in 2010-2021 by Andy Green <andy@warmcat.com>
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
 * may be proprietary.  So unlike the library itself, they are licensed
 * Public Domain.
 *
 * Scrapeable, proxiable OpenMetrics metrics (compatible with Prometheus)
 *
 * https://tools.ietf.org/html/draft-richih-opsawg-openmetrics-00
 *
 * This plugin provides four protocols related to openmetrics handling:
 *
 * 1) "lws-openmetrics" direct http listener so scraper can directly get metrics
 *
 * 2) "lws-openmetrics-prox-agg" metrics proxy server that scraper can connect
 *    to locally to proxy through to connected remote clients at 3)
 *
 * 3) "lws-openmetrics-prox-server" metrics proxy server that remote clients can
 *    connect to, providing a path where scrapers at 2) can get metrics from
 *    clients connected us
 *
 * 4) "lws-openmetrics-prox-client" nailed-up metrics proxy client that tries to
 *    keep up a connection to the server at 3), allowing to scraper to reach
 *    clients that have no reachable way to serve.
 *
 * These are provided like this to maximize flexibility in being able to add
 * openmetrics serving, proxying, or client->proxy to existing lws code.
 *
 * Openmetrics supports a "metric" at the top of its report that describes the
 * source aka "target metadata".
 *
 * Since we want to enable collection from devices that are not externally
 * reachable, we must provide a reachable server that the clients can attach to
 * and have their stats aggregated and then read by Prometheus or whatever.
 * Openmetrics says that it wants to present the aggregated stats in a flat
 * summary with only the aggregator's "target metadata" and contributor targets
 * getting their data tagged with the source
 *
 * "The above discussion is in the context of individual exposers.  An
 *  exposition from a general purpose monitoring system may contain
 *  metrics from many individual targets, and thus may expose multiple
 *  target info Metrics.  The metrics may already have had target
 *  metadata added to them as labels as part of ingestion.  The metric
 *  names MUST NOT be varied based on target metadata.  For example it
 *  would be incorrect for all metrics to end up being prefixed with
 *  staging_ even if they all originated from targets in a staging
 *  environment)."
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
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>
#if !defined(WIN32)
#include <unistd.h>
#endif
#include <assert.h>

struct vhd {
	struct lws_context	*cx;
	struct lws_vhost	*vhost;

	char			ws_server_uri[128];
	char			metrics_proxy_path[128];
	char			ba_secret[128];

	const char		*proxy_side_bind_name;
	/**< name used to bind the two halves of the proxy together, must be
	 * the same name given in a pvo for both "lws-openmetrics-prox-agg"
	 * (the side local to the scraper) and "lws-openmetrics-prox-server"
	 * (the side the clients connect to)
	 */

	char			sanity[8];

	lws_dll2_owner_t	clients;

	lws_sorted_usec_list_t	sul;	     /* schedule connection retry */

	struct vhd		*bind_partner_vhd;

	struct lws		*wsi;	     /* related wsi if any */
	uint16_t		retry_count; /* count of consequetive retries */
};

struct pss {
	lws_dll2_t		list;
	char			proxy_path[64];
	struct lwsac		*ac;	/* the translated metrics, one ac per line */
	struct lwsac		*walk;	/* iterator for ac when writing */
	size_t			tot;	/* content-length computation */
	struct lws		*wsi;

	uint8_t			greet:1; /* set if client needs to send proxy path */
	uint8_t			trigger:1; /* we want to ask the client to dump */
};

#if defined(LWS_WITH_CLIENT)
static const uint32_t backoff_ms[] = { 1000, 2000, 3000, 4000, 5000 };

static const lws_retry_bo_t retry = {
	.retry_ms_table			= backoff_ms,
	.retry_ms_table_count		= LWS_ARRAY_SIZE(backoff_ms),
	.conceal_count			= LWS_ARRAY_SIZE(backoff_ms),

	.secs_since_valid_ping		= 400,  /* force PINGs after secs idle */
	.secs_since_valid_hangup	= 400, /* hangup after secs idle */

	.jitter_percent			= 0,
};

static void
omc_connect_client(lws_sorted_usec_list_t *sul)
{
	struct vhd *vhd = lws_container_of(sul, struct vhd, sul);
	struct lws_client_connect_info i;
	const char *prot;
	char url[128];

	memset(&i, 0, sizeof(i));

	lwsl_notice("%s: %s %s %s\n", __func__, vhd->ws_server_uri, vhd->metrics_proxy_path, vhd->ba_secret);

	lws_strncpy(url, vhd->ws_server_uri, sizeof(url));

	if (lws_parse_uri(url, &prot, &i.address, &i.port, &i.path)) {
		lwsl_err("%s: unable to parse uri %s\n", __func__,
			 vhd->ws_server_uri);
		return;
	}

	i.context		= vhd->cx;
	i.origin		= i.address;
	i.host			= i.address;
	i.ssl_connection	= LCCSCF_USE_SSL;
	i.protocol		= "lws-openmetrics-prox-server"; /* public subprot */
	i.local_protocol_name	= "lws-openmetrics-prox-client";
	i.pwsi			= &vhd->wsi;
	i.retry_and_idle_policy = &retry;
	i.userdata		= vhd;
	i.vhost			= vhd->vhost;

	lwsl_notice("%s: %s %u %s\n", __func__, i.address, i.port, i.path);

	if (lws_client_connect_via_info(&i))
		return;

	/*
	 * Failed... schedule a retry... we can't use the _retry_wsi()
	 * convenience wrapper api here because no valid wsi at this
	 * point.
	 */
	if (!lws_retry_sul_schedule(vhd->cx, 0, sul, &retry,
				    omc_connect_client, &vhd->retry_count))
		return;

	vhd->retry_count = 0;
	lws_retry_sul_schedule(vhd->cx, 0, sul, &retry,
			       omc_connect_client, &vhd->retry_count);
}
#endif

static void
openmetrics_san(char *nm, size_t nl)
{
	size_t m;

	/* Openmetrics has a very restricted token charset */

	for (m = 0; m < nl; m++)
		if ((nm[m] < 'A' || nm[m] > 'Z') &&
		    (nm[m] < 'a' || nm[m] > 'z') &&
		    (nm[m] < '0' || nm[m] > '9') &&
		    nm[m] != '_')
			nm[m] = '_';
}

static int
lws_metrics_om_format_agg(lws_metric_pub_t *pub, const char *nm, lws_usec_t now,
			  int gng, char *buf, size_t len)
{
	const char *_gng = gng ? "_nogo" : "_go";
	char *end = buf + len - 1, *obuf = buf;

	if (pub->flags & LWSMTFL_REPORT_ONLY_GO)
		_gng = "";

	if (!(pub->flags & LWSMTFL_REPORT_MEAN)) {
		/* only the sum is meaningful */
		if (pub->flags & LWSMTFL_REPORT_DUTY_WALLCLOCK_US) {
			buf += lws_snprintf(buf, lws_ptr_diff_size_t(end, buf),
				"%s_count %u\n"
				"%s_us_sum %llu\n"
				"%s_created %lu.%06u\n",
				nm, (unsigned int)pub->u.agg.count[gng],
				nm, (unsigned long long)pub->u.agg.sum[gng],
				nm, (unsigned long)(pub->us_first / 1000000),
				    (unsigned int)(pub->us_first % 1000000));

			return lws_ptr_diff(buf, obuf);
		}

		/* it's a monotonic ordinal, like total tx */
		buf += lws_snprintf(buf, lws_ptr_diff_size_t(end, buf),
				    "%s%s_count %u\n"
				    "%s%s_sum %llu\n",
				    nm, _gng,
				    (unsigned int)pub->u.agg.count[gng],
				    nm, _gng,
				    (unsigned long long)pub->u.agg.sum[gng]);

	} else
		buf += lws_snprintf(buf, lws_ptr_diff_size_t(end, buf),
				    "%s%s_count %u\n"
				    "%s%s_mean %llu\n",
				    nm, _gng,
				    (unsigned int)pub->u.agg.count[gng],
				    nm, _gng, (unsigned long long)
				    (pub->u.agg.count[gng] ?
						pub->u.agg.sum[gng] /
						pub->u.agg.count[gng] : 0));

	return lws_ptr_diff(buf, obuf);
}

static int
lws_metrics_om_ac_stash(struct pss *pss, const char *buf, size_t len)
{
	char *q;

	q = lwsac_use(&pss->ac, LWS_PRE + len + 2, LWS_PRE + len + 2);
	if (!q) {
		lwsac_free(&pss->ac);

		return -1;
	}
	q[LWS_PRE] = (char)((len >> 8) & 0xff);
	q[LWS_PRE + 1] = (char)(len & 0xff);
	memcpy(q + LWS_PRE + 2, buf, len);
	pss->tot += len;

	return 0;
}

/*
 * We have to do the ac listing at this level, because there can be too large
 * a number to metrics tags to iterate that can fit in a reasonable buffer.
 */

static int
lws_metrics_om_format(struct pss *pss, lws_metric_pub_t *pub, const char *nm)
{
	char buf[1200], *p = buf, *end = buf + sizeof(buf) - 1, tmp[512];
	lws_usec_t t = lws_now_usecs();

	if (pub->flags & LWSMTFL_REPORT_HIST) {
		lws_metric_bucket_t *buck = pub->u.hist.head;

		p += lws_snprintf(p, lws_ptr_diff_size_t(end, p),
				  "%s_count %llu\n",
				  nm, (unsigned long long)
				  pub->u.hist.total_count);

		while (buck) {
			lws_strncpy(tmp, lws_metric_bucket_name(buck),
				    sizeof(tmp));

			p += lws_snprintf(p, lws_ptr_diff_size_t(end, p),
					  "%s{%s} %llu\n", nm, tmp,
					  (unsigned long long)buck->count);

			lws_metrics_om_ac_stash(pss, buf,
						lws_ptr_diff_size_t(p, buf));
			p = buf;

			buck = buck->next;
		}

		goto happy;
	}

	if (!pub->u.agg.count[METRES_GO] && !pub->u.agg.count[METRES_NOGO])
		return 0;

	if (pub->u.agg.count[METRES_GO])
		p += lws_metrics_om_format_agg(pub, nm, t, METRES_GO, p,
					       lws_ptr_diff_size_t(end, p));

	if (!(pub->flags & LWSMTFL_REPORT_ONLY_GO) &&
	    pub->u.agg.count[METRES_NOGO])
		p += lws_metrics_om_format_agg(pub, nm, t, METRES_NOGO, p,
					       lws_ptr_diff_size_t(end, p));

	if (pub->flags & LWSMTFL_REPORT_MEAN)
		p += lws_snprintf(p, lws_ptr_diff_size_t(end, p),
				  "%s_min %llu\n"
				  "%s_max %llu\n",
				  nm, (unsigned long long)pub->u.agg.min,
				  nm, (unsigned long long)pub->u.agg.max);

happy:
	return lws_metrics_om_ac_stash(pss, buf, lws_ptr_diff_size_t(p, buf));
}

static int
append_om_metric(lws_metric_pub_t *pub, void *user)
{
	struct pss *pss = (struct pss *)user;
	char nm[64];
	size_t nl;

	/*
	 * Convert lws_metrics to openmetrics metrics data, stashing into an
	 * lwsac without backfill.  Since it's not backfilling, use areas are in
	 * linear sequence simplifying walking them.  Limiting the lwsac alloc
	 * to less than a typical mtu means we can write one per write
	 * efficiently
	 */

	lws_strncpy(nm, pub->name, sizeof(nm));
	nl = strlen(nm);

	openmetrics_san(nm, nl);

	return lws_metrics_om_format(pss, pub, nm);
}

#if defined(__linux__)
static int
grabfile(const char *fi, char *buf, size_t len)
{
	int n, fd = lws_open(fi, LWS_O_RDONLY);

	buf[0] = '\0';
	if (fd < 0)
		return -1;

	n = (int)read(fd, buf, len - 1);
	close(fd);
	if (n < 0) {
		buf[0] = '\0';
		return -1;
	}

	buf[n] = '\0';
	if (n > 0 && buf[n - 1] == '\n')
		buf[--n] = '\0';

	return n;
}
#endif

/*
 * Let's pregenerate the output into an lwsac all at once and
 * then spool it back to the peer afterwards
 *
 * - there's not going to be that much of it (a few kB)
 * - we then know the content-length for the headers
 * - it's stretchy to arbitrary numbers of metrics
 * - lwsac block list provides the per-metric structure to
 *   hold the data in a way we can walk to write it simply
 */

int
ome_prepare(struct lws_context *ctx, struct pss *pss)
{
	char buf[1224], *start = buf + LWS_PRE, *p = start,
	     *end = buf + sizeof(buf) - 1;
	char hn[64];

	pss->tot = 0;

	/*
	 * Target metadata
	 */

	hn[0] = '\0';
	gethostname(hn, sizeof(hn) - 1);
	p += lws_snprintf(p, lws_ptr_diff_size_t(end, p),
			  "# TYPE target info\n"
			  "# HELP target Target metadata\n"
			  "target_info{hostname=\"%s\"", hn);

#if defined(__linux__)
	if (grabfile("/proc/self/cmdline", hn, sizeof(hn)))
		p += lws_snprintf((char *)p, lws_ptr_diff_size_t(end, p),
				  ",cmdline=\"%s\"", hn);
#endif

	p += lws_snprintf(p, lws_ptr_diff_size_t(end, p), "} 1\n");

	if (lws_metrics_om_ac_stash(pss, (const char *)buf + LWS_PRE,
				    lws_ptr_diff_size_t(p, buf + LWS_PRE)))
		return 1;

	/* lws version */

	p = start;
	p += lws_snprintf(p, lws_ptr_diff_size_t(end, p),
			  "# TYPE lws_info info\n"
			  "# HELP lws_info Version of lws producing this\n"
			  "lws_info{version=\"%s\"} 1\n", LWS_BUILD_HASH);
	if (lws_metrics_om_ac_stash(pss, (const char *)buf + LWS_PRE,
				    lws_ptr_diff_size_t(p, buf + LWS_PRE)))
		return 1;

	/* system scalars */

#if defined(__linux__)
	if (grabfile("/proc/loadavg", hn, sizeof(hn))) {
		char *sp = strchr(hn, ' ');
		if (sp) {
			p = start;
			p += lws_snprintf(p, lws_ptr_diff_size_t(end, p),
					  "load_1m %.*s\n",
					  lws_ptr_diff(sp, hn), hn);
			if (lws_metrics_om_ac_stash(pss,
						    (char *)buf + LWS_PRE,
						    lws_ptr_diff_size_t(p,
								start)))
				return 1;
		}
	}
#endif

	if (lws_metrics_foreach(ctx, pss, append_om_metric))
		return 1;

	p = start;
	p += lws_snprintf(p, lws_ptr_diff_size_t(end, p),
			  "# EOF\n");
	if (lws_metrics_om_ac_stash(pss, (char *)buf + LWS_PRE,
				    lws_ptr_diff_size_t(p, buf + LWS_PRE)))
		return 1;

	pss->walk = pss->ac;

	return 0;
}

#if defined(LWS_WITH_SERVER)

/* 1) direct http export for scraper */

static int
callback_lws_openmetrics_export(struct lws *wsi,
				enum lws_callback_reasons reason,
				void *user, void *in, size_t len)
{
	unsigned char buf[1224], *start = buf + LWS_PRE, *p = start,
		      *end = buf + sizeof(buf) - 1, *ip;
	struct lws_context *cx = lws_get_context(wsi);
	struct pss *pss = (struct pss *)user;
	unsigned int m, wm;

	switch (reason) {
	case LWS_CALLBACK_HTTP:

		ome_prepare(cx, pss);

		p = start;
		if (lws_add_http_common_headers(wsi, HTTP_STATUS_OK,
						"application/openmetrics-text; "
						"version=1.0.0; charset=utf-8",
						pss->tot, &p, end) ||
		    lws_finalize_write_http_header(wsi, start, &p, end))
			return 1;

		lws_callback_on_writable(wsi);

		return 0;

	case LWS_CALLBACK_CLOSED_HTTP:
		lwsac_free(&pss->ac);
		break;

	case LWS_CALLBACK_HTTP_WRITEABLE:
		if (!pss->walk)
			return 0;

		do {
			ip = (uint8_t *)pss->walk +
				lwsac_sizeof(pss->walk == pss->ac) + LWS_PRE;
			m = (unsigned int)((ip[0] << 8) | ip[1]);

			/* coverity */
			if (m > lwsac_get_tail_pos(pss->walk) -
				lwsac_sizeof(pss->walk == pss->ac))
				return -1;

			if (lws_ptr_diff_size_t(end, p) < m)
				break;

			memcpy(p, ip + 2, m);
			p += m;

			pss->walk = lwsac_get_next(pss->walk);
		} while (pss->walk);

		if (!lws_ptr_diff_size_t(p, start)) {
			lwsl_err("%s: stuck\n", __func__);
			return -1;
		}

		wm = pss->walk ? LWS_WRITE_HTTP : LWS_WRITE_HTTP_FINAL;

		if (lws_write(wsi, start, lws_ptr_diff_size_t(p, start),
			      (enum lws_write_protocol)wm) < 0)
			return 1;

		if (!pss->walk) {
			 if (lws_http_transaction_completed(wsi))
				return -1;
		} else
			lws_callback_on_writable(wsi);

		return 0;

	default:
		break;
	}

	return lws_callback_http_dummy(wsi, reason, user, in, len);
}

static struct pss *
omc_lws_om_get_other_side_pss_client(struct vhd *vhd, struct pss *pss)
{
	/*
	 * Search through our partner's clients list looking for one with the
	 * same proxy path
	 */
	lws_start_foreach_dll(struct lws_dll2 *, d,
			vhd->bind_partner_vhd->clients.head) {
		struct pss *apss = lws_container_of(d, struct pss, list);

		if (!strcmp(pss->proxy_path, apss->proxy_path))
			return apss;

	} lws_end_foreach_dll(d);

	return NULL;
}

/* 2) "lws-openmetrics-prox-agg": http server export via proxy to connected clients */

static int
callback_lws_openmetrics_prox_agg(struct lws *wsi,
				  enum lws_callback_reasons reason,
				  void *user, void *in, size_t len)
{
	unsigned char buf[1224], *start = buf + LWS_PRE, *p = start,
		      *end = buf + sizeof(buf) - 1, *ip;
	struct vhd *vhd = (struct vhd *)lws_protocol_vh_priv_get(
				lws_get_vhost(wsi), lws_get_protocol(wsi));
	struct lws_context *cx = lws_get_context(wsi);
	struct pss *pss = (struct pss *)user, *partner_pss;
	unsigned int m, wm;

	switch (reason) {

	case LWS_CALLBACK_PROTOCOL_INIT:
		lwsl_notice("%s: PROTOCOL_INIT on %s\n", __func__, lws_vh_tag(lws_get_vhost(wsi)));
		/*
		 * We get told what to do when we are bound to the vhost
		 */
		vhd = lws_protocol_vh_priv_zalloc(lws_get_vhost(wsi),
				lws_get_protocol(wsi), sizeof(struct vhd));
		if (!vhd) {
			lwsl_err("%s: vhd alloc failed\n", __func__);
			return 0;
		}

		vhd->cx = cx;

		/*
		 * Try to bind to the counterpart server in the proxy, binding
		 * to the right one by having a common bind name set in a pvo.
		 * We don't know who will get instantiated last, so both parts
		 * try to bind if not already bound
		 */

		if (!lws_pvo_get_str(in, "proxy-side-bind-name",
				     &vhd->proxy_side_bind_name)) {
			/*
			 * Attempt to find the vhd that belongs to a vhost
			 * that has instantiated protocol
			 * "lws-openmetrics-prox-server", and has set pvo
			 * "proxy-side-bind-name" on it to whatever our
			 * vhd->proxy_side_bind_name was also set to.
			 *
			 * If found, inform the two sides of the same proxy
			 * what their partner vhd is
			 */
			lws_strncpy(vhd->sanity, "isagg", sizeof(vhd->sanity));
			vhd->bind_partner_vhd = lws_vhd_find_by_pvo(cx,
						"lws-openmetrics-prox-server",
						"proxy-side-bind-name",
						vhd->proxy_side_bind_name);
			if (vhd->bind_partner_vhd) {
				assert(!strcmp(vhd->bind_partner_vhd->sanity, "isws"));
				lwsl_notice("%s: proxy binding OK\n", __func__);
				vhd->bind_partner_vhd->bind_partner_vhd = vhd;
			}
		} else {
			lwsl_warn("%s: proxy-side-bind-name required\n", __func__);
			return 0;
		}

		break;

	case LWS_CALLBACK_PROTOCOL_DESTROY:
		if (vhd)
			lws_sul_cancel(&vhd->sul);
		break;

	case LWS_CALLBACK_HTTP:

		/*
		 * The scraper has connected to us, the local side of the proxy,
		 * we need to match what it wants to
		 */

		if (!vhd->bind_partner_vhd)
			return 0;

		lws_strnncpy(pss->proxy_path, (const char *)in, len,
			     sizeof(pss->proxy_path));

		if (pss->list.owner) {
			lwsl_warn("%s: double HTTP?\n", __func__);
			return 0;
		}

		pss->wsi = wsi;

		lws_start_foreach_dll(struct lws_dll2 *, d,
				      vhd->bind_partner_vhd->clients.head) {
			struct pss *apss = lws_container_of(d, struct pss, list);

			if (!strcmp((const char *)in, apss->proxy_path)) {
				apss->trigger = 1;
				lws_callback_on_writable(apss->wsi);

				/* let's add him on the http server vhd list */

				lws_dll2_add_tail(&pss->list, &vhd->clients);
				return 0;
			}

		} lws_end_foreach_dll(d);

		return 0;

	case LWS_CALLBACK_CLOSED_HTTP:
		lwsac_free(&pss->ac);
		lws_dll2_remove(&pss->list);
		break;

	case LWS_CALLBACK_HTTP_WRITEABLE:

		if (!pss->walk)
			return 0;

		/* locate the wss side if it's still around */

		partner_pss = omc_lws_om_get_other_side_pss_client(vhd, pss);
		if (!partner_pss)
			return -1;

		do {
			ip = (uint8_t *)pss->walk +
				lwsac_sizeof(pss->walk == partner_pss->ac) + LWS_PRE;
			m = (unsigned int)((ip[0] << 8) | ip[1]);

			/* coverity */
			if (m > lwsac_get_tail_pos(pss->walk) -
				lwsac_sizeof(pss->walk == partner_pss->ac))
				return -1;

			if (lws_ptr_diff_size_t(end, p) < m)
				break;

			memcpy(p, ip + 2, m);
			p += m;

			pss->walk = lwsac_get_next(pss->walk);
		} while (pss->walk);

		if (!lws_ptr_diff_size_t(p, start)) {
			lwsl_err("%s: stuck\n", __func__);
			return -1;
		}

		wm = pss->walk ? LWS_WRITE_HTTP : LWS_WRITE_HTTP_FINAL;

		if (lws_write(wsi, start, lws_ptr_diff_size_t(p, start),
			      (enum lws_write_protocol)wm) < 0)
			return 1;

		if (!pss->walk) {
			lwsl_info("%s: whole msg proxied to scraper\n", __func__);
			lws_dll2_remove(&pss->list);
			lwsac_free(&partner_pss->ac);
//			if (lws_http_transaction_completed(wsi))
			return -1;
		} else
			lws_callback_on_writable(wsi);

		return 0;

	default:
		break;
	}

	return lws_callback_http_dummy(wsi, reason, user, in, len);
}

/* 3) "lws-openmetrics-prox-server": ws server side of metrics proxy, for
 *    ws clients to connect to */

static int
callback_lws_openmetrics_prox_server(struct lws *wsi,
				     enum lws_callback_reasons reason,
				     void *user, void *in, size_t len)
{
	unsigned char buf[1224], *start = buf + LWS_PRE, *p = start,
		      *end = buf + sizeof(buf) - 1;
	struct vhd *vhd = (struct vhd *)lws_protocol_vh_priv_get(
				lws_get_vhost(wsi), lws_get_protocol(wsi));
	struct lws_context *cx = lws_get_context(wsi);
	struct pss *pss = (struct pss *)user, *partner_pss;

	switch (reason) {

	case LWS_CALLBACK_PROTOCOL_INIT:
		/*
		 * We get told what to do when we are bound to the vhost
		 */

		lwsl_notice("%s: PROTOCOL_INIT on %s\n", __func__, lws_vh_tag(lws_get_vhost(wsi)));

		vhd = lws_protocol_vh_priv_zalloc(lws_get_vhost(wsi),
				lws_get_protocol(wsi), sizeof(struct vhd));
		if (!vhd) {
			lwsl_err("%s: vhd alloc failed\n", __func__);
			return 0;
		}

		vhd->cx = cx;

		/*
		 * Try to bind to the counterpart server in the proxy, binding
		 * to the right one by having a common bind name set in a pvo.
		 * We don't know who will get instantiated last, so both parts
		 * try to bind if not already bound
		 */

		if (!lws_pvo_get_str(in, "proxy-side-bind-name",
				     &vhd->proxy_side_bind_name)) {
			/*
			 * Attempt to find the vhd that belongs to a vhost
			 * that has instantiated protocol
			 * "lws-openmetrics-prox-server", and has set pvo
			 * "proxy-side-bind-name" on it to whatever our
			 * vhd->proxy_side_bind_name was also set to.
			 *
			 * If found, inform the two sides of the same proxy
			 * what their partner vhd is
			 */
			lws_strncpy(vhd->sanity, "isws", sizeof(vhd->sanity));
			vhd->bind_partner_vhd = lws_vhd_find_by_pvo(cx,
						"lws-openmetrics-prox-agg",
						"proxy-side-bind-name",
						vhd->proxy_side_bind_name);
			if (vhd->bind_partner_vhd) {
				assert(!strcmp(vhd->bind_partner_vhd->sanity, "isagg"));
				lwsl_notice("%s: proxy binding OK\n", __func__);
				vhd->bind_partner_vhd->bind_partner_vhd = vhd;
			}
		} else {
			lwsl_warn("%s: proxy-side-bind-name required\n", __func__);
			return 0;
		}

		break;

	case LWS_CALLBACK_PROTOCOL_DESTROY:
		break;

	case LWS_CALLBACK_ESTABLISHED:
		/*
		 * a client has joined... we need to add his pss to our list
		 * of live, joined clients
		 */

		/* mark us as waiting for the reference name from the client */
		pss->greet = 1;
		pss->wsi = wsi;
		lws_validity_confirmed(wsi);

		return 0;

	case LWS_CALLBACK_CLOSED:
		/*
		 * a client has parted
		 */
		lws_dll2_remove(&pss->list);
		lwsl_warn("%s: client %s left (%u)\n", __func__,
				pss->proxy_path,
				(unsigned int)vhd->clients.count);
		lwsac_free(&pss->ac);

		/* let's kill the scraper connection accordingly, if still up */
		partner_pss = omc_lws_om_get_other_side_pss_client(vhd, pss);
		if (partner_pss)
			lws_wsi_close(partner_pss->wsi, LWS_TO_KILL_ASYNC);
		break;

	case LWS_CALLBACK_RECEIVE:
		if (pss->greet) {
			pss->greet = 0;
			lws_strnncpy(pss->proxy_path, (const char *)in, len,
				     sizeof(pss->proxy_path));

			lws_validity_confirmed(wsi);
			lwsl_notice("%s: received greet '%s'\n", __func__,
				    pss->proxy_path);
			/*
			 * we need to add his pss to our list of configured,
			 * live, joined clients
			 */
			lws_dll2_add_tail(&pss->list, &vhd->clients);
			return 0;
		}

		/*
		 * He's sending us his results... let's collect chunks into the
		 * pss lwsac before worrying about anything else
		 */

		if (lws_is_first_fragment(wsi))
			pss->tot = 0;

		lws_metrics_om_ac_stash(pss, (const char *)in, len);

		if (lws_is_final_fragment(wsi)) {
			struct pss *partner_pss;

			lwsl_info("%s: ws side received complete msg\n",
					__func__);

			/* the lwsac is complete */
			pss->walk = pss->ac;
			partner_pss = omc_lws_om_get_other_side_pss_client(vhd, pss);
			if (!partner_pss) {
				lwsl_notice("%s: no partner A\n", __func__);
				return -1;
			}

			/* indicate to scraper side we want to issue now */

			p = start;
			if (lws_add_http_common_headers(partner_pss->wsi, HTTP_STATUS_OK,
							"application/openmetrics-text; "
							"version=1.0.0; charset=utf-8",
							pss->tot, &p, end) ||
			    lws_finalize_write_http_header(partner_pss->wsi,
							    start, &p, end))
				return -1;

			/* indicate to scraper side we want to issue now */

			partner_pss->walk = pss->ac;
			partner_pss->trigger = 1;
			lws_callback_on_writable(partner_pss->wsi);
		}

		return 0;

	case LWS_CALLBACK_SERVER_WRITEABLE:
		if (!pss->trigger)
			return 0;

		pss->trigger = 0;

		partner_pss = omc_lws_om_get_other_side_pss_client(vhd, pss);
		if (!partner_pss) {
			lwsl_err("%s: no partner\n", __func__);
			return 0;
		}

		lwsl_info("%s: sending trigger to client\n", __func__);

		*start = 'x';
		if (lws_write(wsi, start, 1,
			      (enum lws_write_protocol)LWS_WRITE_TEXT) < 0)
			return 1;

		lws_validity_confirmed(wsi);

		return 0;

	default:
		break;
	}

	return lws_callback_http_dummy(wsi, reason, user, in, len);
}
#endif

#if defined(LWS_WITH_CLIENT) && defined(LWS_ROLE_WS)

/* 4) ws client that keeps wss connection up to metrics proxy ws server */

static int
callback_lws_openmetrics_prox_client(struct lws *wsi,
				     enum lws_callback_reasons reason,
				     void *user, void *in, size_t len)
{
	unsigned char buf[1224], *start = buf + LWS_PRE, *p = start,
		      *end = buf + sizeof(buf) - 1, *ip;
	struct vhd *vhd = (struct vhd *)lws_protocol_vh_priv_get(
				lws_get_vhost(wsi), lws_get_protocol(wsi));
	struct lws_context *cx = lws_get_context(wsi);
	struct pss *pss = (struct pss *)user;
	unsigned int m, wm;
	const char *cp;
	char first;

	switch (reason) {

	case LWS_CALLBACK_PROTOCOL_INIT:

		lwsl_notice("%s: PROTOCOL_INIT on %s\n", __func__,
					lws_vh_tag(lws_get_vhost(wsi)));


		/*
		 * We get told what to do when we are bound to the vhost
		 */
		vhd = lws_protocol_vh_priv_zalloc(lws_get_vhost(wsi),
				lws_get_protocol(wsi), sizeof(struct vhd));
		if (!vhd)
			return 0;

		vhd->cx = cx;
		vhd->vhost = lws_get_vhost(wsi);

		/* the proxy server uri */

		if (lws_pvo_get_str(in, "ws-server-uri", &cp) || !cp) {
			lwsl_warn("%s: ws-server-uri pvo required\n", __func__);

			return 0;
		}
		lws_strncpy(vhd->ws_server_uri, cp, sizeof(vhd->ws_server_uri));

		/* how we should be referenced at the proxy */

		if (lws_pvo_get_str(in, "metrics-proxy-path", &cp)) {
			lwsl_err("%s: metrics-proxy-path pvo required\n", __func__);

			return 1;
		}
		lws_strncpy(vhd->metrics_proxy_path, cp, sizeof(vhd->metrics_proxy_path));

		/* the shared secret to authenticate us as allowed to join */

		if (lws_pvo_get_str(in, "ba-secret", &cp)) {
			lwsl_err("%s: ba-secret pvo required\n", __func__);

			return 1;
		}
		lws_strncpy(vhd->ba_secret, cp, sizeof(vhd->ba_secret));

		lwsl_notice("%s: scheduling connect %s %s %s\n", __func__,
				vhd->ws_server_uri, vhd->metrics_proxy_path, vhd->ba_secret);

		lws_validity_confirmed(wsi);
		lws_sul_schedule(cx, 0, &vhd->sul, omc_connect_client, 1);
		break;

	case LWS_CALLBACK_PROTOCOL_DESTROY:
		if (vhd)
			lws_sul_cancel(&vhd->sul);
		break;

	case LWS_CALLBACK_CLIENT_APPEND_HANDSHAKE_HEADER:
	{
		unsigned char **pp = (unsigned char **)in, *pend = (*pp) + len;
		char b[128];

		/* authorize ourselves to the metrics proxy using basic auth */

		if (lws_http_basic_auth_gen("metricsclient", vhd->ba_secret,
					    b, sizeof(b)))
			break;

		if (lws_add_http_header_by_token(wsi,
						 WSI_TOKEN_HTTP_AUTHORIZATION,
						 (unsigned char *)b,
						 (int)strlen(b), pp, pend))
			return -1;

		break;
	}

	case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
		lwsl_err("CLIENT_CONNECTION_ERROR: %s\n",
			 in ? (char *)in : "(null)");
		goto do_retry;

	case LWS_CALLBACK_CLIENT_ESTABLISHED:
		lwsl_warn("%s: connected to ws metrics agg server\n", __func__);
		pss->greet = 1;
		lws_callback_on_writable(wsi);
		lws_validity_confirmed(wsi);
		return 0;

	case LWS_CALLBACK_CLIENT_CLOSED:
		lwsl_notice("%s: client closed\n", __func__);
		lwsac_free(&pss->ac);
		goto do_retry;

	case LWS_CALLBACK_CLIENT_RECEIVE:
		/*
		 * Proxy serverside sends us something to trigger us to create
		 * our metrics message and send it back over the ws link
		 */
		ome_prepare(cx, pss);
		pss->walk = pss->ac;
		lws_callback_on_writable(wsi);
		lwsl_info("%s: dump requested\n", __func__);
		break;

	case LWS_CALLBACK_CLIENT_WRITEABLE:
		if (pss->greet) {
			/*
			 * At first after establishing the we link, we send a
			 * message indicating to the metrics proxy how we
			 * should be referred to by the scraper to particularly
			 * select to talk to us
			 */
			lwsl_info("%s: sending greet '%s'\n", __func__,
					vhd->metrics_proxy_path);
			lws_strncpy((char *)start, vhd->metrics_proxy_path,
					sizeof(buf) - LWS_PRE);
			if (lws_write(wsi, start,
				      strlen(vhd->metrics_proxy_path),
				      LWS_WRITE_TEXT) < 0)
				return 1;

			lws_validity_confirmed(wsi);

			pss->greet = 0;
			return 0;
		}

		if (!pss->walk)
			return 0;

		/*
		 * We send the metrics dump in a single logical ws message,
		 * using ws fragmentation to split it around 1 mtu boundary
		 * and keep coming back until it's finished
		 */

		first = pss->walk == pss->ac;

		do {
			ip = (uint8_t *)pss->walk +
				lwsac_sizeof(pss->walk == pss->ac) + LWS_PRE;
			m = (unsigned int)((ip[0] << 8) | ip[1]);

			/* coverity */
			if (m > lwsac_get_tail_pos(pss->walk) -
				lwsac_sizeof(pss->walk == pss->ac)) {
				lwsl_err("%s: size blow\n", __func__);
				return -1;
			}

			if (lws_ptr_diff_size_t(end, p) < m)
				break;

			memcpy(p, ip + 2, m);
			p += m;

			pss->walk = lwsac_get_next(pss->walk);
		} while (pss->walk);

		if (!lws_ptr_diff_size_t(p, start)) {
			lwsl_err("%s: stuck\n", __func__);
			return -1;
		}

		wm = (unsigned int)lws_write_ws_flags(LWS_WRITE_TEXT, first,
						      !pss->walk);

		if (lws_write(wsi, start, lws_ptr_diff_size_t(p, start),
			      (enum lws_write_protocol)wm) < 0) {
			lwsl_notice("%s: write fail\n", __func__);
			return 1;
		}

		lws_validity_confirmed(wsi);
		lwsl_info("%s: forwarded %d\n", __func__, lws_ptr_diff(p, start));

		if (!pss->walk) {
			lwsl_info("%s: dump send completed\n", __func__);
			lwsac_free(&pss->ac);
		} else
			lws_callback_on_writable(wsi);

		return 0;

	default:
		break;
	}

	return lws_callback_http_dummy(wsi, reason, user, in, len);

do_retry:
	if (!lws_retry_sul_schedule(cx, 0, &vhd->sul, &retry,
				    omc_connect_client, &vhd->retry_count))
		return 0;

	vhd->retry_count = 0;
	lws_retry_sul_schedule(cx, 0, &vhd->sul, &retry,
			       omc_connect_client, &vhd->retry_count);

	return 0;
}
#endif


LWS_VISIBLE const struct lws_protocols lws_openmetrics_export_protocols[] = {
#if defined(LWS_WITH_SERVER)
	{ /* for scraper directly: http export on listen socket */
		"lws-openmetrics",
		callback_lws_openmetrics_export,
		sizeof(struct pss),
		1024, 0, NULL, 0
	},
	{ /* for scraper via ws proxy: http export on listen socket */
		"lws-openmetrics-prox-agg",
		callback_lws_openmetrics_prox_agg,
		sizeof(struct pss),
		1024, 0, NULL, 0
	},
	{ /* metrics proxy server side: ws server for clients to connect to */
		"lws-openmetrics-prox-server",
		callback_lws_openmetrics_prox_server,
		sizeof(struct pss),
		1024, 0, NULL, 0
	},
#endif
#if defined(LWS_WITH_CLIENT) && defined(LWS_ROLE_WS)
	{ /* client to metrics proxy: ws client to connect to metrics proxy*/
		"lws-openmetrics-prox-client",
		callback_lws_openmetrics_prox_client,
		sizeof(struct pss),
		1024, 0, NULL, 0
	},
#endif
};

LWS_VISIBLE const lws_plugin_protocol_t lws_openmetrics_export = {
	.hdr = {
		"lws OpenMetrics export",
		"lws_protocol_plugin",
		LWS_BUILD_HASH,
		LWS_PLUGIN_API_MAGIC
	},

	.protocols = lws_openmetrics_export_protocols,
	.count_protocols = LWS_ARRAY_SIZE(lws_openmetrics_export_protocols),
};
