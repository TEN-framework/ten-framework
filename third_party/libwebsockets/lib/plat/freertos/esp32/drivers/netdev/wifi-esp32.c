/*
 * libwebsockets - esp32 wifi -> lws_netdev_wifi
 *
 * Copyright (C) 2010 - 2020 Andy Green <andy@warmcat.com>
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
 *
 * These are the esp platform wifi-specific netdev pieces.  Nothing else should
 * know any esp-specific apis.
 *
 * Operations happen via the generic lws_detdev instantiation for the platform
 * wifi device, which point in here for operations.  We also set up native OS
 * event hooks per device for wifi and IP stack events, and post them as lws_smd
 * NETWORK events on the if in the "platform private" namespace.  We then
 * service the events in the lws event loop thread context, which may again
 * generate lws_smd NETWORK events in the public namespace depending on what
 * happened.
 *
 * Scan requests go through a sul to make sure we don't get "piling on" from
 * scheduled, timed scans.  Scan results go through the lws_smd "washing" and
 * are actually parsed in lws thread context, where they are converted to lws
 * netdev scan results and processed by generic code.
 */

#include "private-lib-core.h"

#include "esp_system.h"
#include "esp_spi_flash.h"
#include "esp_wifi.h"
#include <nvs_flash.h>
#include <esp_netif.h>

/*
 * lws_netdev_instance_t:
 *   lws_netdev_instance_wifi_t:
 *     lws_netdev_instance_wifi_esp32_t
 */

typedef struct lws_netdev_instance_wifi_esp32 {
	lws_netdev_instance_wifi_t		wnd;
	esp_event_handler_instance_t		instance_any_id;
	esp_event_handler_instance_t		instance_got_ip;
	wifi_config_t				sta_config;
} lws_netdev_instance_wifi_esp32_t;

/*
static wifi_config_t config = {
	.ap = {
	    .channel = 6,
	    .authmode = WIFI_AUTH_OPEN,
	    .max_connection = 1,
	} };
	*/

/*
 * Platform-specific connect / associate
 */

int
lws_netdev_wifi_connect_plat(lws_netdev_instance_t *nd, const char *ssid,
			     const char *passphrase, uint8_t *bssid)
{
	lws_netdev_instance_wifi_esp32_t *wnde32 =
					(lws_netdev_instance_wifi_esp32_t *)nd;

	wnde32->wnd.inst.ops->up(&wnde32->wnd.inst);

	wnde32->wnd.flags |= LNDIW_MODE_STA;
	esp_wifi_set_mode(WIFI_MODE_STA);

#if 0
	/* we will do our own dhcp */
	tcpip_adapter_dhcpc_stop(TCPIP_ADAPTER_IF_STA);
#endif

	lws_strncpy((char *)wnde32->sta_config.sta.ssid, ssid,
		    sizeof(wnde32->sta_config.sta.ssid));
	lws_strncpy((char *)wnde32->sta_config.sta.password, passphrase,
		    sizeof(wnde32->sta_config.sta.password));

	esp_wifi_set_config(WIFI_IF_STA, &wnde32->sta_config);
	esp_wifi_connect();

	return 0;
}

/*
 * This is called from the SMD / lws thread context, after we heard there were
 * scan results on this netdev
 */

static void
lws_esp32_scan_update(lws_netdev_instance_wifi_t *wnd)
{
//	lws_netdevs_t *netdevs = lws_netdevs_from_ndi(&wnd->inst);
	wifi_ap_record_t ap_records[LWS_WIFI_MAX_SCAN_TRACK], *ar;
	uint32_t now = lws_now_secs();
	uint16_t count_ap_records;
	int n;

	count_ap_records = LWS_ARRAY_SIZE(ap_records);
	if (esp_wifi_scan_get_ap_records(&count_ap_records, ap_records)) {
		lwsl_err("%s: failed\n", __func__);
		return;
	}

	if (!count_ap_records)
		return;

	if (wnd->state != LWSNDVWIFI_STATE_SCAN)
		return;

	/*
	 * ... let's collect the OS-specific scan results, and convert then to
	 * lws_netdev sorted by rssi.  If we already have it in the scan list,
	 * keep it and keep a little ringbuffer of its rssi along with an
	 * averaging.  If it's new, add it into the linked-list sorted by rssi.
	 */

	ar = &ap_records[0];
	for (n = 0; n < count_ap_records; n++) {
		lws_wifi_sta_t *w;
		int m;

		m = strlen((const char *)ar->ssid);
		if (!m)
			goto next;

		/*
		 * We know this guy from before?
		 */

		w = lws_netdev_wifi_scan_find(wnd, (const char *)ar->ssid,
						ar->bssid);
		if (!w) {
			w = lws_zalloc(sizeof(*w) + m + 1, __func__);
			if (!w)
				goto next;

			w->ssid = (char *)&w[1];
			memcpy(w->ssid, ar->ssid, m + 1);
			w->ssid_len = m;

			memcpy(w->bssid, ar->bssid, 6);

			lws_dll2_add_sorted(&w->list, &wnd->scan,
					    lws_netdev_wifi_rssi_sort_compare);
		}

		if (w->rssi_count == LWS_ARRAY_SIZE(w->rssi))
			w->rssi_avg -= w->rssi[w->rssi_next];
		else
			w->rssi_count++;
		w->rssi[w->rssi_next] = ar->rssi;
		w->rssi_avg += w->rssi[w->rssi_next++];
		w->rssi_next = w->rssi_next & (LWS_ARRAY_SIZE(w->rssi) - 1);

		w->ch = ar->primary;
		w->authmode = ar->authmode;
		w->last_seen = now;

next:
		ar++;
	}

	/*
	 * We can do the rest of it using the generic scan list and credentials
	 */

	lws_netdev_wifi_scan_select(wnd);
}

static wifi_scan_config_t scan_config = {
        .ssid = 0,
        .bssid = 0,
        .channel = 0,
        .show_hidden = true
};

void
lws_netdev_wifi_scan_plat(lws_netdev_instance_t *nd)
{
	lws_netdev_instance_wifi_t *wnd = (lws_netdev_instance_wifi_t *)nd;

	if (esp_wifi_scan_start(&scan_config, false))
		lwsl_err("%s: %s scan failed\n", __func__, wnd->inst.name);
}

/*
 * Platform-private interface events turn up here after going through SMD and
 * passed down by matching network interface name via generic lws_netdev.  All
 * that messing around gets us from an OS-specific thread with an event to back
 * here in lws event loop thread context, with the same event bound to a the
 * netdev it belongs to.
 */

int
lws_netdev_wifi_event_plat(struct lws_netdev_instance *nd, lws_usec_t timestamp,
			   void *buf, size_t len)
{
	lws_netdev_instance_wifi_t *wnd = (lws_netdev_instance_wifi_t *)nd;
	struct lws_context *ctx = netdev_instance_to_ctx(&wnd->inst);
	size_t al;

	/*
	 * netdev-private sync messages?
	 */

	if (!lws_json_simple_strcmp(buf, len, "\"type\":", "priv")) {
		const char *ev = lws_json_simple_find(buf, len, "\"ev\":", &al);

		if (!ev)
			return 0;

		lwsl_notice("%s: smd priv ev %.*s\n", __func__, (int)al, ev);

		switch (atoi(ev)) {
		case WIFI_EVENT_STA_START:
			wnd->state = LWSNDVWIFI_STATE_INITIAL;
			if (!lws_netdev_wifi_redo_last(wnd))
				break;

			/*
			 * if the "try last successful" one fails, start the
			 * scan by falling through
			 */

		case WIFI_EVENT_STA_DISCONNECTED:
			lws_smd_msg_printf(ctx, LWSSMDCL_NETWORK,
					   "{\"type\":\"linkdown\","
					   "\"if\":\"%s\"}", wnd->inst.name);
			wnd->state = LWSNDVWIFI_STATE_SCAN;
			/*
			 * We do it via the sul so we don't get timed scans
			 * on top of each other
			 */
			lws_sul_schedule(ctx, 0, &wnd->sul_scan,
					 lws_netdev_wifi_scan, 1);
			break;

		case WIFI_EVENT_STA_CONNECTED:
			lws_smd_msg_printf(ctx, LWSSMDCL_NETWORK,
					   "{\"type\":\"linkup\","
					   "\"if\":\"%s\"}", wnd->inst.name);
			break;

		case WIFI_EVENT_SCAN_DONE:
			lws_esp32_scan_update(wnd);
			break;
		default:
			return 0;
		}

		return 0;
	}

	return 0;
}

/*
 * This is coming from a thread context unrelated to lws... the first order is
 * to turn these into lws_smd events synchronized on lws thread, since we want
 * to change correspsonding lws netdev object states without locking.
 */

static void
_event_handler_wifi(void *arg, esp_event_base_t event_base, int32_t event_id,
		   void *event_data)
{
	lws_netdev_instance_wifi_t *wnd = (lws_netdev_instance_wifi_t *)arg;
	struct lws_context *ctx = netdev_instance_to_ctx(&wnd->inst);

	switch (event_id) {
	case WIFI_EVENT_STA_START:
	case WIFI_EVENT_STA_DISCONNECTED:
	case WIFI_EVENT_SCAN_DONE:
	case WIFI_EVENT_STA_CONNECTED:
		/*
		 * These are events in the platform's private namespace,
		 * interpreted only by the lws_smd handler above, ** in the lws
		 * event thread context **.  The point of this is to requeue the
		 * event in the lws thread context like a bottom-half.
		 *
		 * To save on registrations, the context's NETWORK smd
		 * participant passes messages to lws_netdev, who passes ones
		 * that have if matching the netdev name to that netdev's
		 * (*event) handler.
		 *
		 * The other handler may emit generic network state SMD events
		 * for other things to consume.
		 */

		lws_smd_msg_printf(ctx, LWSSMDCL_NETWORK,
				   "{\"type\":\"priv\",\"if\":\"%s\",\"ev\":%d}",
				   wnd->inst.name, (int)event_id);
		break;
	default:
		return;
	}
}

#if 0
static int
espip_to_sa46(lws_sockaddr46 *sa46, esp_ip_addr_t *eip)
{
	memset(sa46, 0, sizeof(sa46));

	switch (eip->type) {
	case ESP_IPADDR_TYPE_V4:
		sa46->sa4.sin_family = AF_INET;
		memcpy(sa46->sa4.sin_addr, &eip->u_addr.ip4.addr, );
		return;
	case ESP_IPADDR_TYPE_V6:
	}
}
#endif

/*
 * This is coming from a thread context unrelated to lws
 */

static void
_event_handler_ip(void *arg, esp_event_base_t event_base, int32_t event_id,
	      void *event_data)
{
	lws_netdev_instance_wifi_t *wnd = (lws_netdev_instance_wifi_t *)arg;
	lws_netdevs_t *netdevs = lws_netdevs_from_ndi(&wnd->inst);
	struct lws_context *ctx = lws_context_from_netdevs(netdevs);

	if (event_id == IP_EVENT_STA_GOT_IP) {
		ip_event_got_ip_t *e = (ip_event_got_ip_t *)event_data;
		char ip[16];
#if 0
		tcpip_adapter_dns_info_t e32ip;

		/*
		 * Since atm we get this via DHCP, presumably we can get ahold
		 * of related info set by the router
		 */

		if (tcpip_adapter_get_dns_info(TCPIP_ADAPTER_IF_STA,
					   TCPIP_ADAPTER_DNS_MAIN,
					   /* also _BACKUP, _FALLBACK */
					   &e32ip)) {
			lwsl_err("%s: there's no dns server set\n", __func__);
			e32ip.ip.u_addr.ipv4 = 0x08080808;
			e32ip.ip.type = ESP_IPADDR_TYPE_V4;
		}

		netdevs->sa46_dns_resolver.
#endif

		lws_write_numeric_address((void *)&e->ip_info.ip, 4, ip,
				sizeof(ip));
		lws_smd_msg_printf(ctx, LWSSMDCL_NETWORK,
				   "{\"type\":\"ipacq\",\"if\":\"%s\","
				   "\"ipv4\":\"%s\"}", wnd->inst.name, ip);
	}
}

/*
 * This is the platform (esp-idf) init for any kind of networking to be
 * available at all
 */
int
lws_netdev_plat_init(void)
{
        nvs_flash_init();
	esp_netif_init();
	ESP_ERROR_CHECK(esp_event_loop_create_default());

	return 0;
}

/*
 * This is the platform (esp-idf) init for any wifi to be available at all
 */
int
lws_netdev_plat_wifi_init(void)
{
	wifi_init_config_t wic = WIFI_INIT_CONFIG_DEFAULT();
	int n;

	esp_netif_create_default_wifi_sta();

	n = esp_wifi_init(&wic);
	if (n) {
		lwsl_err("%s: wifi init fail: %d\n", __func__, n);
		return 1;
	}

	return 0;
}


struct lws_netdev_instance *
lws_netdev_wifi_create_plat(struct lws_context *ctx,
			    const lws_netdev_ops_t *ops,
			    const char *name, void *platinfo)
{
	lws_netdev_instance_wifi_esp32_t *wnde32 = lws_zalloc(
						sizeof(*wnde32), __func__);

	if (!wnde32)
		return NULL;

	wnde32->wnd.inst.type = LWSNDTYP_WIFI;
	lws_netdev_instance_create(&wnde32->wnd.inst, ctx, ops, name, platinfo);

	return &wnde32->wnd.inst;
}

int
lws_netdev_wifi_configure_plat(struct lws_netdev_instance *nd,
			       lws_netdev_config_t *config)
{
	return 0;
}

int
lws_netdev_wifi_up_plat(struct lws_netdev_instance *nd)
{
	lws_netdev_instance_wifi_esp32_t *wnde32 =
					(lws_netdev_instance_wifi_esp32_t *)nd;
	struct lws_context *ctx = netdev_instance_to_ctx(&wnde32->wnd.inst);

	if (wnde32->wnd.flags & LNDIW_UP)
		return 0;

	ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
			  IP_EVENT_STA_GOT_IP, &_event_handler_ip, nd,
			  &wnde32->instance_got_ip));

	ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
			  ESP_EVENT_ANY_ID, &_event_handler_wifi, nd,
			  &wnde32->instance_any_id));

	esp_wifi_start();
	wnde32->wnd.flags |= LNDIW_UP;

	lws_smd_msg_printf(ctx, LWSSMDCL_NETWORK,
			   "{\"type\":\"up\",\"if\":\"%s\"}",
			   wnde32->wnd.inst.name);

	return 0;
}

int
lws_netdev_wifi_down_plat(struct lws_netdev_instance *nd)
{
	lws_netdev_instance_wifi_esp32_t *wnde32 =
					(lws_netdev_instance_wifi_esp32_t *)nd;
	struct lws_context *ctx = netdev_instance_to_ctx(&wnde32->wnd.inst);

	if (!(wnde32->wnd.flags & LNDIW_UP))
		return 0;

	lws_smd_msg_printf(ctx, LWSSMDCL_NETWORK,
			   "{\"type\":\"down\",\"if\":\"%s\"}",
			   wnde32->wnd.inst.name);

	esp_wifi_stop();

	esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP,
						&wnde32->instance_got_ip);
	esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID,
						&wnde32->instance_any_id);

	wnde32->wnd.flags &= ~LNDIW_UP;

	return 0;
}

void
lws_netdev_wifi_destroy_plat(struct lws_netdev_instance **pnd)
{
	lws_free(*pnd);
	*pnd = NULL;
}
