/*
 * libwebsockets-test-server - libwebsockets test implementation
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

#include <time.h>
#include <string.h>
#ifdef WIN32
#include <io.h>
#include <gettimeofday.h>
#endif


typedef enum {
	WALK_NONE,
	WALK_INITIAL,
	WALK_LIST,
	WALK_FINAL
} e_walk;

struct per_session_data__lws_status {
	struct per_session_data__lws_status *next;
	struct lws *wsi;
	time_t time_est;
	char user_agent[256];

	e_walk walk;
	struct per_session_data__lws_status *walk_next;
	unsigned char subsequent:1;
	unsigned char changed_partway:1;
	unsigned char wss_over_h2:1;
};

struct per_vhost_data__lws_status {
	struct per_session_data__lws_status *live_pss_list;
	struct lws_context *context;
	struct lws_vhost *vhost;
	const struct lws_protocols *protocol;
	int count_live_pss;
};

static void
trigger_resend(struct per_vhost_data__lws_status *vhd)
{
	lws_start_foreach_ll(struct per_session_data__lws_status *, pss,
			     vhd->live_pss_list) {
		if (pss->walk == WALK_NONE) {
			pss->subsequent = 0;
			pss->walk_next = vhd->live_pss_list;
			pss->walk = WALK_INITIAL;
		} else
			pss->changed_partway = 1;
	} lws_end_foreach_ll(pss, next);

	lws_callback_on_writable_all_protocol(vhd->context, vhd->protocol);
}

/* lws-status protocol */

int
callback_lws_status(struct lws *wsi, enum lws_callback_reasons reason,
		    void *user, void *in, size_t len)
{
	struct per_session_data__lws_status *pss =
			(struct per_session_data__lws_status *)user;
	struct per_vhost_data__lws_status *vhd =
			(struct per_vhost_data__lws_status *)
			lws_protocol_vh_priv_get(lws_get_vhost(wsi),
					lws_get_protocol(wsi));
	char buf[LWS_PRE + 384], ip[24], *start = buf + LWS_PRE - 1, *p = start,
	     *end = buf + sizeof(buf) - 1;
	int n, m;

	switch (reason) {

	case LWS_CALLBACK_PROTOCOL_INIT:
		vhd = lws_protocol_vh_priv_zalloc(lws_get_vhost(wsi),
				lws_get_protocol(wsi),
				sizeof(struct per_vhost_data__lws_status));
		if (!vhd) {
			lwsl_notice("%s: PROTOCOL_INIT failed\n", __func__);
			return 0;
		}
		vhd->context = lws_get_context(wsi);
		vhd->protocol = lws_get_protocol(wsi);
		vhd->vhost = lws_get_vhost(wsi);
		break;

	case LWS_CALLBACK_ESTABLISHED:

		/*
		 * This shows how to stage sending a single ws message in
		 * multiple fragments.  In this case, it lets us trade off
		 * memory needed to make the data vs time to send it.
		 */

		vhd->count_live_pss++;
		pss->next = vhd->live_pss_list;
		vhd->live_pss_list = pss;

		pss->wss_over_h2 = !!len;

		time(&pss->time_est);
		pss->wsi = wsi;

#if defined(LWS_WITH_HTTP_UNCOMMON_HEADERS)
		if (lws_hdr_copy(wsi, pss->user_agent, sizeof(pss->user_agent),
			     WSI_TOKEN_HTTP_USER_AGENT) < 0) /* too big */
#endif
			strcpy(pss->user_agent, "unknown");
		trigger_resend(vhd);
		break;

	case LWS_CALLBACK_SERVER_WRITEABLE:
		switch (pss->walk) {
		case WALK_INITIAL:
			n = LWS_WRITE_TEXT | LWS_WRITE_NO_FIN;
			p += lws_snprintf(p, lws_ptr_diff_size_t(end, p),
				      "{ \"version\":\"%s\","
				      " \"wss_over_h2\":\"%d\","
				      " \"hostname\":\"%s\","
				      " \"wsi\":\"%d\", \"conns\":[",
				      lws_get_library_version(),
				      pss->wss_over_h2,
				      lws_canonical_hostname(vhd->context),
				      vhd->count_live_pss);
			pss->walk = WALK_LIST;
			pss->walk_next = vhd->live_pss_list;
			break;
		case WALK_LIST:
			n = LWS_WRITE_CONTINUATION | LWS_WRITE_NO_FIN;
			if (!pss->walk_next)
				goto walk_final;

			if (pss->subsequent)
				*p++ = ',';
			pss->subsequent = 1;

			m = 0;
			lws_start_foreach_ll(struct per_session_data__lws_status *,
					     pss2, vhd->live_pss_list) {
				if (pss2 == pss->walk_next) {
					m = 1;
					break;
				}
			} lws_end_foreach_ll(pss2, next);

			if (!m) {
				/* our next guy went away */
				pss->walk = WALK_FINAL;
				pss->changed_partway = 1;
				break;
			}

			strcpy(ip, "unknown");
			lws_get_peer_simple(pss->walk_next->wsi, ip, sizeof(ip));
			p += lws_snprintf(p, lws_ptr_diff_size_t(end, p),
					"{\"peer\":\"%s\",\"time\":\"%ld\","
					"\"ua\":\"%s\"}",
					ip, (unsigned long)pss->walk_next->time_est,
					pss->walk_next->user_agent);
			pss->walk_next = pss->walk_next->next;
			if (!pss->walk_next)
				pss->walk = WALK_FINAL;
			break;
		case WALK_FINAL:
walk_final:
			n = LWS_WRITE_CONTINUATION;
			p += lws_snprintf(p, 4, "]}");
			if (pss->changed_partway) {
				pss->changed_partway = 0;
				pss->subsequent = 0;
				pss->walk_next = vhd->live_pss_list;
				pss->walk = WALK_INITIAL;
			} else
				pss->walk = WALK_NONE;
			break;
		default:
			return 0;
		}

		m = lws_write(wsi, (unsigned char *)start, lws_ptr_diff_size_t(p, start), (unsigned int)n);
		if (m < 0) {
			lwsl_err("ERROR %d writing to di socket\n", m);
			return -1;
		}

		if (pss->walk != WALK_NONE)
			lws_callback_on_writable(wsi);
		break;

	case LWS_CALLBACK_RECEIVE:
		lwsl_notice("pmd test: RX len %d\n", (int)len);
		break;

	case LWS_CALLBACK_CLOSED:
		// lwsl_debug("****** LWS_CALLBACK_CLOSED\n");
		lws_start_foreach_llp(struct per_session_data__lws_status **,
			ppss, vhd->live_pss_list) {
			if (*ppss == pss) {
				*ppss = pss->next;
				break;
			}
		} lws_end_foreach_llp(ppss, next);

		trigger_resend(vhd);
		break;

	default:
		break;
	}

	return 0;
}

#define LWS_PLUGIN_PROTOCOL_LWS_STATUS \
	{ \
		"lws-status", \
		callback_lws_status, \
		sizeof(struct per_session_data__lws_status), \
		512, /* rx buf size must be >= permessage-deflate rx size */ \
		0, NULL, 0 \
	}

#if !defined (LWS_PLUGIN_STATIC)

LWS_VISIBLE const struct lws_protocols lws_status_protocols[] = {
	LWS_PLUGIN_PROTOCOL_LWS_STATUS
};

LWS_VISIBLE const lws_plugin_protocol_t lws_status = {
	.hdr = {
		"lws status",
		"lws_protocol_plugin",
		LWS_BUILD_HASH,
		LWS_PLUGIN_API_MAGIC
	},

	.protocols = lws_status_protocols,
	.count_protocols = LWS_ARRAY_SIZE(lws_status_protocols),
	.extensions = NULL,
	.count_extensions = 0,
};

#endif
