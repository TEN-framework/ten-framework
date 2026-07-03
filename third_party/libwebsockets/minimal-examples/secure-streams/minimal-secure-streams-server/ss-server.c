/*
 * lws-minimal-secure-streams-server
 *
 * Written in 2010-2020 by Andy Green <andy@warmcat.com>
 *
 * This file is made available under the Creative Commons CC0 1.0
 * Universal Public Domain Dedication.
 */

#include <libwebsockets.h>
#include <assert.h>

extern int interrupted, bad, multipart;

static const char *html =
		/* normally we serve this... */
	"<head><meta content=\"text/html;charset=utf-8\" "
			"http-equiv=\"Content-Type\"><script>"
	" var ws = new WebSocket(\"wss://localhost:7681\", \"mywsprotocol\");"
	"try { ws.onopen = function() { console.log(\"open\"); }; "
		"ws.onmessage = function got_packet(msg) { "
		   "var s=\"\"; s += msg.data; "
		   "document.getElementById(\"wsd\").innerHTML = s; };"
		"} catch(exception) {"
		"alert(\"<p>Error\" + exception); }"
	"</script></head><html><body>"
	  "Hello from the web server<br>"
	  "<div id=\"wsd\"></div>"
	"</body></html>",

*multipart_html =
	/*
	 * If you use -m commandline switch we send this instead, as
	 * multipart/form-data
	 */
	"--aBoundaryString\r\n"
	"Content-Disposition: form-data; name=\"myFile\"; filename=\"xxx.txt\"\r\n"
	"Content-Type: text/plain\r\n"
	"\r\n"
	"The file contents\r\n"
	"--aBoundaryString\r\n"
	"Content-Disposition: form-data; name=\"myField\"\r\n"
	"\r\n"
	"(data)\r\n"
	"--aBoundaryString--\r\n";


typedef struct myss {
	struct lws_ss_handle 		*ss;
	void				*opaque_data;
	/* ... application specific state ... */

	lws_sorted_usec_list_t		sul;
	int				count;
	char				upgraded;

} myss_srv_t;

/*
 * This is the Secure Streams Server RX and TX for HTTP(S)
 */

static lws_ss_state_return_t
myss_srv_rx(void *userobj, const uint8_t *buf, size_t len, int flags)
{
//	myss_srv_t *m = (myss_srv_t *)userobj;

	lwsl_user("%s: len %d, flags: %d\n", __func__, (int)len, flags);
	lwsl_hexdump_info(buf, len);

	/*
	 * If we received the whole message, for our example it means
	 * we are done.
	 */
	if (flags & LWSSS_FLAG_EOM) {
		bad = 0;
		interrupted = 1;
	}

	return 0;
}

static lws_ss_state_return_t
myss_srv_tx(void *userobj, lws_ss_tx_ordinal_t ord, uint8_t *buf, size_t *len,
	int *flags)
{
	myss_srv_t *m = (myss_srv_t *)userobj;
	const char *send = html;

	if (m->upgraded)
		return LWSSSSRET_TX_DONT_SEND;

	if (multipart)
		send = multipart_html;

	*flags = LWSSS_FLAG_SOM | LWSSS_FLAG_EOM;

	lws_strncpy((char *)buf, send, *len);
	*len = strlen(send);

	return 0;
}

/*
 * This is the Secure Streams Server RX and TX for WS(S)... when we get a
 * state that the underlying connection upgraded protocol, we switch the stream
 * rx and tx handlers to here.
 */

static lws_ss_state_return_t
myss_ws_rx(void *userobj, const uint8_t *buf, size_t len, int flags)
{
//	myss_srv_t *m = (myss_srv_t *)userobj;

	lwsl_user("%s: len %d, flags: %d\n", __func__, (int)len, flags);
	lwsl_hexdump_info(buf, len);

	/*
	 * If we received the whole message, for our example it means
	 * we are done.
	 */
	if (flags & LWSSS_FLAG_EOM) {
		bad = 0;
		interrupted = 1;
	}

	return 0;
}

/* this is the callback that mediates sending the incrementing number */

static void
spam_sul_cb(struct lws_sorted_usec_list *sul)
{
	myss_srv_t *m = lws_container_of(sul, myss_srv_t, sul);

	if (!lws_ss_request_tx(m->ss))
		lws_sul_schedule(lws_ss_get_context(m->ss), 0, &m->sul, spam_sul_cb,
			 100 * LWS_US_PER_MS);
}

static lws_ss_state_return_t
myss_ws_tx(void *userobj, lws_ss_tx_ordinal_t ord, uint8_t *buf, size_t *len,
	int *flags)
{
	myss_srv_t *m = (myss_srv_t *)userobj;

	*flags = LWSSS_FLAG_SOM | LWSSS_FLAG_EOM;

	*len = (unsigned int)lws_snprintf((char *)buf, *len, "hello from ws %d", m->count++);

	lws_sul_schedule(lws_ss_get_context(m->ss), 0, &m->sul, spam_sul_cb,
			 100 * LWS_US_PER_MS);

	return 0;
}

static lws_ss_state_return_t
myss_srv_state(void *userobj, void *sh, lws_ss_constate_t state,
	   lws_ss_tx_ordinal_t ack)
{
	myss_srv_t *m = (myss_srv_t *)userobj;

	lwsl_user("%s: %p %s, ord 0x%x\n", __func__, m->ss,
		  lws_ss_state_name((int)state), (unsigned int)ack);

	switch (state) {
	case LWSSSCS_DISCONNECTED:
		lws_sul_cancel(&m->sul);
		break;
	case LWSSSCS_CREATING:
		return lws_ss_request_tx(m->ss);

	case LWSSSCS_ALL_RETRIES_FAILED:
		/* if we're out of retries, we want to close the app and FAIL */
		interrupted = 1;
		break;

	case LWSSSCS_SERVER_TXN:
		/*
		 * The underlying protocol started a transaction, let's
		 * describe how we want to complete it.  We can defer this until
		 * later, eg, after we have consumed any rx that's coming with
		 * the client's transaction initiation phase, but in this
		 * example we know what we want to do already.
		 *
		 * We do want to ack the transaction...
		 */
		lws_ss_server_ack(m->ss, 0);
		/*
		 * ... it's going to be either text/html or multipart ...
		 */
		if (multipart) {
			if (lws_ss_set_metadata(m->ss, "mime",
			   "multipart/form-data; boundary=aBoundaryString", 45))
				return LWSSSSRET_DISCONNECT_ME;
		} else
			if (lws_ss_set_metadata(m->ss, "mime", "text/html", 9))
				return LWSSSSRET_DISCONNECT_ME;
		/*
		 * ...it's going to be whatever size it is (and request tx)
		 */
		return lws_ss_request_tx_len(m->ss, (unsigned long)
				(multipart ? strlen(multipart_html) :
							 strlen(html)));

	case LWSSSCS_SERVER_UPGRADE:

		/*
		 * This is sent when the underlying protocol has experienced
		 * an upgrade, eg, http->ws... it's a one-way upgrade on this
		 * stream, change the handlers to deal with the kind of
		 * messages we send on ws
		 */

		m->upgraded = 1;
		lws_ss_change_handlers(m->ss, myss_ws_rx, myss_ws_tx, NULL);
		return lws_ss_request_tx(m->ss); /* we want to start sending numbers */

	default:
		break;
	}

	return 0;
}

const lws_ss_info_t ssi_server = {
	.handle_offset			= offsetof(myss_srv_t, ss),
	.opaque_user_data_offset	= offsetof(myss_srv_t, opaque_data),
	.streamtype			= "myserver",
	.rx				= myss_srv_rx,
	.tx				= myss_srv_tx,
	.state				= myss_srv_state,
	.user_alloc			= sizeof(myss_srv_t),
};
