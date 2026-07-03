/* set of parsable strings -- ALL LOWER CASE */

static const char * const set[] = {
	"get ",
	"post ",
#if defined(LWS_WITH_HTTP_UNCOMMON_HEADERS) || defined(LWS_HTTP_HEADERS_ALL)
	"options ",
#endif
	"host:",
	"connection:",
	"upgrade:",
	"origin:",
#if defined(LWS_ROLE_WS) || defined(LWS_HTTP_HEADERS_ALL)
	"sec-websocket-draft:",
#endif
	"\x0d\x0a",

#if defined(LWS_ROLE_WS) || defined(LWS_HTTP_HEADERS_ALL)
	"sec-websocket-extensions:",
	"sec-websocket-key1:",
	"sec-websocket-key2:",
	"sec-websocket-protocol:",

	"sec-websocket-accept:",
	"sec-websocket-nonce:",
#endif
	"http/1.1 ",
#if defined(LWS_ROLE_H2) || defined(LWS_HTTP_HEADERS_ALL)
	"http2-settings:",
#endif

	"accept:",
#if defined(LWS_WITH_HTTP_UNCOMMON_HEADERS) || defined(LWS_HTTP_HEADERS_ALL)
	"access-control-request-headers:",
#endif
	"if-modified-since:",
	"if-none-match:",
	"accept-encoding:",
	"accept-language:",
	"pragma:",
	"cache-control:",
	"authorization:",
	"cookie:",
	"content-length:",
	"content-type:",
	"date:",
	"range:",
#if defined(LWS_WITH_HTTP_UNCOMMON_HEADERS) || defined(LWS_ROLE_H2) || defined(LWS_HTTP_HEADERS_ALL)
	"referer:",
#endif
#if defined(LWS_ROLE_WS) || defined(LWS_HTTP_HEADERS_ALL)
	"sec-websocket-key:",
	"sec-websocket-version:",
	"sec-websocket-origin:",
#endif
#if defined(LWS_ROLE_H2) || defined(LWS_HTTP_HEADERS_ALL)
	":authority",
	":method",
	":path",
	":scheme",
	":status",
#endif
#if defined(LWS_WITH_HTTP_UNCOMMON_HEADERS) || defined(LWS_ROLE_H2) || defined(LWS_HTTP_HEADERS_ALL)
	"accept-charset:",
#endif
	"accept-ranges:",
#if defined(LWS_WITH_HTTP_UNCOMMON_HEADERS) || defined(LWS_ROLE_H2) || defined(LWS_HTTP_HEADERS_ALL)
	"access-control-allow-origin:",
#endif
	"age:",
	"allow:",
	"content-disposition:",
	"content-encoding:",
	"content-language:",
	"content-location:",
	"content-range:",
	"etag:",
	"expect:",
	"expires:",
	"from:",
	"if-match:",
	"if-range:",
	"if-unmodified-since:",
	"last-modified:",
	"link:",
	"location:",
#if defined(LWS_WITH_HTTP_UNCOMMON_HEADERS) || defined(LWS_ROLE_H2) || defined(LWS_HTTP_HEADERS_ALL)
	"max-forwards:",
	"proxy-authenticate:",
	"proxy-authorization:",
#endif
	"refresh:",
	"retry-after:",
	"server:",
	"set-cookie:",
#if defined(LWS_WITH_HTTP_UNCOMMON_HEADERS) || defined(LWS_ROLE_H2) || defined(LWS_HTTP_HEADERS_ALL)
	"strict-transport-security:",
#endif
	"transfer-encoding:",
#if defined(LWS_WITH_HTTP_UNCOMMON_HEADERS) || defined(LWS_ROLE_H2) || defined(LWS_HTTP_HEADERS_ALL)
	"user-agent:",
	"vary:",
	"via:",
	"www-authenticate:",
#endif
#if defined(LWS_WITH_HTTP_UNCOMMON_HEADERS) || defined(LWS_HTTP_HEADERS_ALL)
	"patch",
	"put",
	"delete",
#endif

	"uri-args", /* fake header used for uri-only storage */

#if defined(LWS_WITH_HTTP_UNCOMMON_HEADERS) || defined(LWS_HTTP_HEADERS_ALL)
	"proxy ",
	"x-real-ip:",
#endif
	"http/1.0 ",

	"x-forwarded-for:",
	"connect ",
	"head ",
#if defined(LWS_WITH_HTTP_UNCOMMON_HEADERS) || defined(LWS_ROLE_H2) || defined(LWS_HTTP_HEADERS_ALL)
	"te:",		/* http/2 wants it to reject it */
	"replay-nonce:", /* ACME */
#endif
#if defined(LWS_ROLE_H2) || defined(LWS_HTTP_HEADERS_ALL)
	":protocol",		/* defined in mcmanus-httpbis-h2-ws-02 */
#endif

	"x-auth-token:",
	"x-amzn-dss-signature:",

	"", /* not matchable */

};
