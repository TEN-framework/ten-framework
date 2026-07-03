/*
 * lws-minimal-secure-streams
 *
 * Written in 2010-2020 by Andy Green <andy@warmcat.com>
 *
 * This file is made available under the Creative Commons CC0 1.0
 * Universal Public Domain Dedication.
 *
 *
 * This demonstrates various kinds of successful and failed connection
 * situations in order to confirm the correct states are coming.
 *
 * You can control how much bulk data is requested from the peer using
 * --amount xxx, the default without that is 12345 bytes.
 */

#include <libwebsockets.h>
#include <string.h>
#include <signal.h>

static int interrupted, tests, tests_pass, tests_fail;
static lws_sorted_usec_list_t sul_next_test;
static lws_state_notify_link_t nl;
struct lws_context *context;
size_t amount = 12345;

static void
tests_start_next(lws_sorted_usec_list_t *sul);

/*
 * If the -proxy app is fulfilling our connection, then we don't need to have
 * the policy in the client.
 *
 * When we build with LWS_SS_USE_SSPC, the apis hook up to a proxy process over
 * a Unix Domain Socket.  To test that, you need to separately run the
 * ./lws-minimal-secure-streams-proxy test app on the same machine.
 */

#if !defined(LWS_SS_USE_SSPC)
static const char * const default_ss_policy =
	"{"
	  "\"release\":"			"\"01234567\","
	  "\"product\":"			"\"myproduct\","
	  "\"schema-version\":"			"1,"
#if defined(VIA_LOCALHOST_SOCKS)
	  "\"via-socks5\":"                     "\"127.0.0.1:1080\","
#endif

	  "\"retry\": ["	/* named backoff / retry strategies */
		"{\"default\": {"
			"\"backoff\": [	 1000, 1000, 1000, 1000"
				"],"
			"\"conceal\":"		"4,"
			"\"jitterpc\":"		"20,"
			"\"svalidping\":"	"30,"
			"\"svalidhup\":"	"35"
		"}}"
	  "],"
	  "\"certs\": [" /* named individual certificates in BASE64 DER */
		/*
		 * Let's Encrypt certs for warmcat.com / libwebsockets.org
		 *
		 * We fetch the real policy from there using SS and switch to
		 * using that.
		 */
		"{\"isrg_root_x1\": \""
	"MIIFazCCA1OgAwIBAgIRAIIQz7DSQONZRGPgu2OCiwAwDQYJKoZIhvcNAQELBQAw"
	"TzELMAkGA1UEBhMCVVMxKTAnBgNVBAoTIEludGVybmV0IFNlY3VyaXR5IFJlc2Vh"
	"cmNoIEdyb3VwMRUwEwYDVQQDEwxJU1JHIFJvb3QgWDEwHhcNMTUwNjA0MTEwNDM4"
	"WhcNMzUwNjA0MTEwNDM4WjBPMQswCQYDVQQGEwJVUzEpMCcGA1UEChMgSW50ZXJu"
	"ZXQgU2VjdXJpdHkgUmVzZWFyY2ggR3JvdXAxFTATBgNVBAMTDElTUkcgUm9vdCBY"
	"MTCCAiIwDQYJKoZIhvcNAQEBBQADggIPADCCAgoCggIBAK3oJHP0FDfzm54rVygc"
	"h77ct984kIxuPOZXoHj3dcKi/vVqbvYATyjb3miGbESTtrFj/RQSa78f0uoxmyF+"
	"0TM8ukj13Xnfs7j/EvEhmkvBioZxaUpmZmyPfjxwv60pIgbz5MDmgK7iS4+3mX6U"
	"A5/TR5d8mUgjU+g4rk8Kb4Mu0UlXjIB0ttov0DiNewNwIRt18jA8+o+u3dpjq+sW"
	"T8KOEUt+zwvo/7V3LvSye0rgTBIlDHCNAymg4VMk7BPZ7hm/ELNKjD+Jo2FR3qyH"
	"B5T0Y3HsLuJvW5iB4YlcNHlsdu87kGJ55tukmi8mxdAQ4Q7e2RCOFvu396j3x+UC"
	"B5iPNgiV5+I3lg02dZ77DnKxHZu8A/lJBdiB3QW0KtZB6awBdpUKD9jf1b0SHzUv"
	"KBds0pjBqAlkd25HN7rOrFleaJ1/ctaJxQZBKT5ZPt0m9STJEadao0xAH0ahmbWn"
	"OlFuhjuefXKnEgV4We0+UXgVCwOPjdAvBbI+e0ocS3MFEvzG6uBQE3xDk3SzynTn"
	"jh8BCNAw1FtxNrQHusEwMFxIt4I7mKZ9YIqioymCzLq9gwQbooMDQaHWBfEbwrbw"
	"qHyGO0aoSCqI3Haadr8faqU9GY/rOPNk3sgrDQoo//fb4hVC1CLQJ13hef4Y53CI"
	"rU7m2Ys6xt0nUW7/vGT1M0NPAgMBAAGjQjBAMA4GA1UdDwEB/wQEAwIBBjAPBgNV"
	"HRMBAf8EBTADAQH/MB0GA1UdDgQWBBR5tFnme7bl5AFzgAiIyBpY9umbbjANBgkq"
	"hkiG9w0BAQsFAAOCAgEAVR9YqbyyqFDQDLHYGmkgJykIrGF1XIpu+ILlaS/V9lZL"
	"ubhzEFnTIZd+50xx+7LSYK05qAvqFyFWhfFQDlnrzuBZ6brJFe+GnY+EgPbk6ZGQ"
	"3BebYhtF8GaV0nxvwuo77x/Py9auJ/GpsMiu/X1+mvoiBOv/2X/qkSsisRcOj/KK"
	"NFtY2PwByVS5uCbMiogziUwthDyC3+6WVwW6LLv3xLfHTjuCvjHIInNzktHCgKQ5"
	"ORAzI4JMPJ+GslWYHb4phowim57iaztXOoJwTdwJx4nLCgdNbOhdjsnvzqvHu7Ur"
	"TkXWStAmzOVyyghqpZXjFaH3pO3JLF+l+/+sKAIuvtd7u+Nxe5AW0wdeRlN8NwdC"
	"jNPElpzVmbUq4JUagEiuTDkHzsxHpFKVK7q4+63SM1N95R1NbdWhscdCb+ZAJzVc"
	"oyi3B43njTOQ5yOf+1CceWxG1bQVs5ZufpsMljq4Ui0/1lvh+wjChP4kqKOJ2qxq"
	"4RgqsahDYVvTH9w7jXbyLeiNdd8XM2w9U/t7y0Ff/9yi0GE44Za4rF2LN9d11TPA"
	"mRGunUHBcnWEvgJBQl9nJEiU0Zsnvgc/ubhPgXRR4Xq37Z0j4r7g1SgEEzwxA57d"
	"emyPxgcYxn/eR44/KJ4EBs+lVDR3veyJm+kXQ99b21/+jh5Xos1AnX5iItreGCc="
	  "\"},{"
	"\"digicert_global_root_g2\": \"MIIDjjCCAnagAwIBAgIQAzrx5qcRqaC7K"
	"GSxHQn65TANBgkqhkiG9w0BAQsFADBhMQswCQYDVQQGEwJVUzEVMBMGA1UEChMMR"
	"GlnaUNlcnQgSW5jMRkwFwYDVQQLExB3d3cuZGlnaWNlcnQuY29tMSAwHgYDVQQDE"
	"xdEaWdpQ2VydCBHbG9iYWwgUm9vdCBHMjAeFw0xMzA4MDExMjAwMDBaFw0zODAxM"
	"TUxMjAwMDBaMGExCzAJBgNVBAYTAlVTMRUwEwYDVQQKEwxEaWdpQ2VydCBJbmMxG"
	"TAXBgNVBAsTEHd3dy5kaWdpY2VydC5jb20xIDAeBgNVBAMTF0RpZ2lDZXJ0IEdsb"
	"2JhbCBSb290IEcyMIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAuzfNN"
	"Nx7a8myaJCtSnX/RrohCgiN9RlUyfuI2/Ou8jqJkTx65qsGGmvPrC3oXgkkRLpim"
	"n7Wo6h+4FR1IAWsULecYxpsMNzaHxmx1x7e/dfgy5SDN67sH0NO3Xss0r0upS/kq"
	"bitOtSZpLYl6ZtrAGCSYP9PIUkY92eQq2EGnI/yuum06ZIya7XzV+hdG82MHauVB"
	"JVJ8zUtluNJbd134/tJS7SsVQepj5WztCO7TG1F8PapspUwtP1MVYwnSlcUfIKdz"
	"XOS0xZKBgyMUNGPHgm+F6HmIcr9g+UQvIOlCsRnKPZzFBQ9RnbDhxSJITRNrw9FD"
	"KZJobq7nMWxM4MphQIDAQABo0IwQDAPBgNVHRMBAf8EBTADAQH/MA4GA1UdDwEB/"
	"wQEAwIBhjAdBgNVHQ4EFgQUTiJUIBiV5uNu5g/6+rkS7QYXjzkwDQYJKoZIhvcNA"
	"QELBQADggEBAGBnKJRvDkhj6zHd6mcY1Yl9PMWLSn/pvtsrF9+wX3N3KjITOYFnQ"
	"oQj8kVnNeyIv/iPsGEMNKSuIEyExtv4NeF22d+mQrvHRAiGfzZ0JFrabA0UWTW98"
	"kndth/Jsw1HKj2ZL7tcu7XUIOGZX1NGFdtom/DzMNU+MeKNhJ7jitralj41E6Vf8"
	"PlwUHBHQRFXGU7Aj64GxJUTFy8bJZ918rGOmaFvE7FBcf6IKshPECBV1/MUReXgR"
	"PTqh5Uykw7+U0b6LJ3/iyK5S9kJRaTepLiaWN0bfVKfjllDiIGknibVb63dDcY3f"
	"e0Dkhvld1927jyNxF1WW6LZZm6zNTflMrY=\""
	"}, {"
		"\"digicert_global_ca_g2\": \"MIIEizCCA3OgAwIBAgIQDI7gyQ1"
	"qiRWIBAYe4kH5rzANBgkqhkiG9w0BAQsFADBhMQswCQYDVQQGEwJVUzEVMBMGA1U"
	"EChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3d3cuZGlnaWNlcnQuY29tMSAwHgY"
	"DVQQDExdEaWdpQ2VydCBHbG9iYWwgUm9vdCBHMjAeFw0xMzA4MDExMjAwMDBaFw0"
	"yODA4MDExMjAwMDBaMEQxCzAJBgNVBAYTAlVTMRUwEwYDVQQKEwxEaWdpQ2VydCB"
	"JbmMxHjAcBgNVBAMTFURpZ2lDZXJ0IEdsb2JhbCBDQSBHMjCCASIwDQYJKoZIhvc"
	"NAQEBBQADggEPADCCAQoCggEBANNIfL7zBYZdW9UvhU5L4IatFaxhz1uvPmoKR/u"
	"adpFgC4przc/cV35gmAvkVNlW7SHMArZagV+Xau4CLyMnuG3UsOcGAngLH1ypmTb"
	"+u6wbBfpXzYEQQGfWMItYNdSWYb7QjHqXnxr5IuYUL6nG6AEfq/gmD6yOTSwyOR2"
	"Bm40cZbIc22GoiS9g5+vCShjEbyrpEJIJ7RfRACvmfe8EiRROM6GyD5eHn7OgzS+"
	"8LOy4g2gxPR/VSpAQGQuBldYpdlH5NnbQtwl6OErXb4y/E3w57bqukPyV93t4CTZ"
	"edJMeJfD/1K2uaGvG/w/VNfFVbkhJ+Pi474j48V4Rd6rfArMCAwEAAaOCAVowggF"
	"WMBIGA1UdEwEB/wQIMAYBAf8CAQAwDgYDVR0PAQH/BAQDAgGGMDQGCCsGAQUFBwE"
	"BBCgwJjAkBggrBgEFBQcwAYYYaHR0cDovL29jc3AuZGlnaWNlcnQuY29tMHsGA1U"
	"dHwR0MHIwN6A1oDOGMWh0dHA6Ly9jcmw0LmRpZ2ljZXJ0LmNvbS9EaWdpQ2VydEd"
	"sb2JhbFJvb3RHMi5jcmwwN6A1oDOGMWh0dHA6Ly9jcmwzLmRpZ2ljZXJ0LmNvbS9"
	"EaWdpQ2VydEdsb2JhbFJvb3RHMi5jcmwwPQYDVR0gBDYwNDAyBgRVHSAAMCowKAY"
	"IKwYBBQUHAgEWHGh0dHBzOi8vd3d3LmRpZ2ljZXJ0LmNvbS9DUFMwHQYDVR0OBBY"
	"EFCRuKy3QapJRUSVpAaqaR6aJ50AgMB8GA1UdIwQYMBaAFE4iVCAYlebjbuYP+vq"
	"5Eu0GF485MA0GCSqGSIb3DQEBCwUAA4IBAQALOYSR+ZfrqoGvhOlaOJL84mxZvzb"
	"IRacxAxHhBsCsMsdaVSnaT0AC9aHesO3ewPj2dZ12uYf+QYB6z13jAMZbAuabeGL"
	"J3LhimnftiQjXS8X9Q9ViIyfEBFltcT8jW+rZ8uckJ2/0lYDblizkVIvP6hnZf1W"
	"ZUXoOLRg9eFhSvGNoVwvdRLNXSmDmyHBwW4coatc7TlJFGa8kBpJIERqLrqwYEle"
	"sA8u49L3KJg6nwd3jM+/AVTANlVlOnAM2BvjAjxSZnE0qnsHhfTuvcqdFuhOWKU4"
	"Z0BqYBvQ3lBetoxi6PrABDJXWKTUgNX31EGDk92hiHuwZ4STyhxGs6QiA\""
	"},"
	"{\"amazon_root_ca_1\": \"MIIDQTCCAimgAwIBAgITBmyfz5m/jAo54vB4ikP"
	"mljZbyjANBgkqhkiG9w0BAQsFADA5MQswCQYDVQQGEwJVUzEPMA0GA1UEChMGQW1"
	"hem9uMRkwFwYDVQQDExBBbWF6b24gUm9vdCBDQSAxMB4XDTE1MDUyNjAwMDAwMFo"
	"XDTM4MDExNzAwMDAwMFowOTELMAkGA1UEBhMCVVMxDzANBgNVBAoTBkFtYXpvbjE"
	"ZMBcGA1UEAxMQQW1hem9uIFJvb3QgQ0EgMTCCASIwDQYJKoZIhvcNAQEBBQADggE"
	"PADCCAQoCggEBALJ4gHHKeNXjca9HgFB0fW7Y14h29Jlo91ghYPl0hAEvrAIthtO"
	"gQ3pOsqTQNroBvo3bSMgHFzZM9O6II8c+6zf1tRn4SWiw3te5djgdYZ6k/oI2peV"
	"KVuRF4fn9tBb6dNqcmzU5L/qwIFAGbHrQgLKm+a/sRxmPUDgH3KKHOVj4utWp+Uh"
	"nMJbulHheb4mjUcAwhmahRWa6VOujw5H5SNz/0egwLX0tdHA114gk957EWW67c4c"
	"X8jJGKLhD+rcdqsq08p8kDi1L93FcXmn/6pUCyziKrlA4b9v7LWIbxcceVOF34Gf"
	"ID5yHI9Y/QCB/IIDEgEw+OyQmjgSubJrIqg0CAwEAAaNCMEAwDwYDVR0TAQH/BAU"
	"wAwEB/zAOBgNVHQ8BAf8EBAMCAYYwHQYDVR0OBBYEFIQYzIU07LwMlJQuCFmcx7I"
	"QTgoIMA0GCSqGSIb3DQEBCwUAA4IBAQCY8jdaQZChGsV2USggNiMOruYou6r4lK5"
	"IpDB/G/wkjUu0yKGX9rbxenDIU5PMCCjjmCXPI6T53iHTfIUJrU6adTrCC2qJeHZ"
	"ERxhlbI1Bjjt/msv0tadQ1wUsN+gDS63pYaACbvXy8MWy7Vu33PqUXHeeE6V/Uq2"
	"V8viTO96LXFvKWlJbYK8U90vvo/ufQJVtMVT8QtPHRh8jrdkPSHCa2XV4cdFyQzR"
	"1bldZwgJcJmApzyMZFo6IQ6XU5MsI+yMRQ+hDKXJioaldXgjUkK642M4UwtBV8ob"
	"2xJNDd2ZhwLnoQdeXeGADbkpyrqXRfboQnoZsG4q5WTP468SQvvG5\"}"
	  "],"
	  "\"trust_stores\": [" /* named cert chains */
		"{"
			"\"name\": \"api_amazon_com\","
			"\"stack\": [\"digicert_global_ca_g2\", \"digicert_global_root_g2\"]"
		"}, { \"name\": \"arca1\", \"stack\": [\"amazon_root_ca_1\"]},"
		"{"
			"\"name\": \"le_via_isrg\","
			"\"stack\": ["
				"\"isrg_root_x1\""
			"]"
		"}"
	  "],"
	  "\"s\": ["

		"{\"api_amazon_com_auth\": {"
			"\"endpoint\": \"api.amazon.com\","
			"\"port\": 443,"
			"\"protocol\": \"h1\","
			"\"http_method\": \"POST\","
			"\"http_url\": \"auth/o2/token\","
			"\"plugins\": [],"
			"\"opportunistic\": true,"
			"\"tls\": true,"
			"\"h2q_oflow_txcr\": true,"
			"\"http_www_form_urlencoded\": true,"
			"\"http_no_content_length\": true,"
			"\"retry\": \"default\","
			"\"tls_trust_store\": \"api_amazon_com\""
		"}},{"

		/*
		 * Just get a 200 from httpbin.org
		 * on h1:80, h1:443 and h2:443
		 *
		 * sanity check that we're working at all
		 */

		    "\"t_h1\": {"
			"\"endpoint\": \"httpbin.org\","
			"\"port\": 80,"
			"\"protocol\": \"h1\","
			"\"http_method\": \"GET\","
			"\"http_url\": \"/status/200\","
			"\"opportunistic\": true,"
			"\"retry\": \"default\""
		"}},{"
		    "\"t_h1_tls\": {"
			"\"endpoint\": \"httpbin.org\","
			"\"port\": 443,"
			"\"protocol\": \"h1\","
			"\"http_method\": \"GET\","
			"\"http_url\": \"/status/200\","
			"\"tls\": true,"
			"\"opportunistic\": true,"
			"\"retry\": \"default\","
			"\"tls_trust_store\": \"arca1\""
		"}},{"
		    "\"t_h2_tls\": {"
			"\"endpoint\": \"httpbin.org\","
			"\"port\": 443,"
			"\"protocol\": \"h2\","
			"\"http_method\": \"GET\","
			"\"http_url\": \"/status/200\","
			"\"tls\": true,"
			"\"nghttp2_quirk_end_stream\": true,"
			"\"h2q_oflow_txcr\": true,"
			"\"opportunistic\": true,"
			"\"retry\": \"default\","
			"\"tls_trust_store\": \"arca1\""
		"}},{"

		/*
		 * 10s delayed response from httpbin.org
		 * on h1:80, h1:443 and h2:443
		 *
		 * used to trigger timeout testing
		 */

		    "\"d_h1\": {"
			"\"endpoint\": \"httpbin.org\","
			"\"port\": 80,"
			"\"protocol\": \"h1\","
			"\"http_method\": \"GET\","
			"\"http_url\": \"/delay/10\","
			"\"opportunistic\": true,"
			"\"retry\": \"default\""
		"}},{"
		    "\"d_h1_tls\": {"
			"\"endpoint\": \"httpbin.org\","
			"\"port\": 443,"
			"\"protocol\": \"h1\","
			"\"http_method\": \"GET\","
			"\"http_url\": \"/delay/10\","
			"\"tls\": true,"
			"\"opportunistic\": true,"
			"\"retry\": \"default\","
			"\"tls_trust_store\": \"arca1\""
		"}},{"
		    "\"d_h2_tls\": {"
			"\"endpoint\": \"httpbin.org\","
			"\"port\": 443,"
			"\"protocol\": \"h2\","
			"\"http_method\": \"GET\","
			"\"http_url\": \"/delay/10\","
			"\"tls\": true,"
			"\"nghttp2_quirk_end_stream\": true,"
			"\"h2q_oflow_txcr\": true,"
			"\"opportunistic\": true,"
			"\"retry\": \"default\","
			"\"tls_trust_store\": \"arca1\""
		"}},{"

		/*
		 * get NXDOMAIN for bogus.nope
		 * on h1:80, h1:443 and h2:443
		 *
		 * Triggers unreachable and eventually all_retries_failed
		 */

		    "\"nxd_h1\": {"
			"\"endpoint\": \"bogus.nope\","
			"\"port\": 80,"
			"\"protocol\": \"h1\","
			"\"http_method\": \"GET\","
			"\"http_url\": \"/status/200\","
			"\"opportunistic\": true,"
			"\"retry\": \"default\""
		"}},{"
		    "\"nxd_h1_tls\": {"
			"\"endpoint\": \"bogus.nope\","
			"\"port\": 443,"
			"\"protocol\": \"h1\","
			"\"http_method\": \"GET\","
			"\"http_url\": \"/status/200\","
			"\"tls\": true,"
			"\"opportunistic\": true,"
			"\"retry\": \"default\","
			"\"tls_trust_store\": \"arca1\""
		"}},{"
		    "\"nxd_h2_tls\": {"
			"\"endpoint\": \"bogus.nope\","
			"\"port\": 443,"
			"\"protocol\": \"h2\","
			"\"http_method\": \"GET\","
			"\"http_url\": \"/status/200\","
			"\"tls\": true,"
			"\"nghttp2_quirk_end_stream\": true,"
			"\"h2q_oflow_txcr\": true,"
			"\"opportunistic\": true,"
			"\"retry\": \"default\","
			"\"tls_trust_store\": \"arca1\""
		"}},{"

		/*
		 * bulk payload transfer from httpbin.org
		 * on h1:80, h1:443 and h2:443
		 *
		 * Sanity check larger payload
		 */

		    "\"bulk_h1\": {"
			"\"endpoint\": \"httpbin.org\","
			"\"port\": 80,"
			"\"protocol\": \"h1\","
			"\"http_method\": \"GET\","
			"\"http_url\": \"range/${amount}\","
			"\"metadata\": [{"
					"\"amount\": \"\""
				"}],"
			"\"opportunistic\": true,"
			"\"retry\": \"default\""
		"}},{"
		    "\"bulk_h1_tls\": {"
			"\"endpoint\": \"httpbin.org\","
			"\"port\": 443,"
			"\"protocol\": \"h1\","
			"\"http_method\": \"GET\","
			"\"http_url\": \"range/${amount}\","
			"\"metadata\": [{"
					"\"amount\": \"\""
				"}],"
			"\"tls\": true,"
			"\"opportunistic\": true,"
			"\"retry\": \"default\","
			"\"tls_trust_store\": \"arca1\""
		"}},{"
		    "\"bulk_h2_tls\": {"
			"\"endpoint\": \"httpbin.org\","
			"\"port\": 443,"
			"\"protocol\": \"h2\","
			"\"http_method\": \"GET\","
			"\"http_url\": \"range/${amount}\","
			"\"metadata\": [{"
					"\"amount\": \"\""
				"}],"
			"\"tls\": true,"
			"\"nghttp2_quirk_end_stream\": true,"
			"\"h2q_oflow_txcr\": true,"
			"\"opportunistic\": true,"
			"\"retry\": \"default\","
			"\"tls_trust_store\": \"arca1\""

		"}},{"

		/*
		 * Various kinds of tls failure
		 *
		 * hostname.badcert.warmcat.com: serves valid cert but for
		 *				 warmcat.com
		 *
		 * warmcat.com:446: serves valid but expired cert
		 *
		 * I don't have an easy way to make the test for "not valid yet"
		 * cert without root
		 *
		 * invalidca.badcert.warmcat.com:  selfsigned cert for that
		 *				   hostname
		 */

		    "\"badcert_hostname\": {"
			"\"endpoint\": \"hostname.badcert.warmcat.com\","
			"\"port\": 443,"
			"\"protocol\": \"h1\","
			"\"http_method\": \"GET\","
			"\"http_url\": \"/\","
			"\"tls\": true,"
			"\"opportunistic\": true,"
			"\"retry\": \"default\","
			"\"tls_trust_store\": \"le_via_isrg\""
		"}},{"
		    "\"badcert_expired\": {"
			"\"endpoint\": \"warmcat.com\","
			"\"port\": 446,"
			"\"protocol\": \"h1\","
			"\"http_method\": \"GET\","
			"\"http_url\": \"/\","
			"\"tls\": true,"
			"\"opportunistic\": true,"
			"\"retry\": \"default\","
			"\"tls_trust_store\": \"le_via_isrg\""
		"}},{"
		    "\"badcert_selfsigned\": {"
			"\"endpoint\": \"invalidca.badcert.warmcat.com\","
			"\"port\": 443,"
			"\"protocol\": \"h1\","
			"\"http_method\": \"GET\","
			"\"http_url\": \"/\","
			"\"tls\": true,"
			"\"nghttp2_quirk_end_stream\": true,"
			"\"h2q_oflow_txcr\": true,"
			"\"opportunistic\": true,"
			"\"retry\": \"default\","
			"\"tls_trust_store\": \"le_via_isrg\""
                "}}"
	"]}"
;

#endif

/*
 * This is the sequence of test streams we are going to create, the ss timeout,
 * and a description of what we want to see to understand the test passed, or
 * failed.  If the test hits destruction without making a explicit pass or fail
 * decision before, that's a fail.  Or, depending on what state we put in
 * .must_see, we can count a state like UNREACHABLE as a pass.
 */

struct tests_seq {
	const char		*name;
	const char		*streamtype;
	uint64_t		timeout_us;
	lws_ss_constate_t	must_see;
	unsigned int		mask_unexpected;
	size_t			eom_pass;
} tests_seq[] = {

	/*
	 * We just get a 200 from httpbin.org as a sanity check first
	 */

	{
		"h1:80 just get 200",
		"t_h1", 5 * LWS_US_PER_SEC, LWSSSCS_QOS_ACK_REMOTE,
		(1 << LWSSSCS_TIMEOUT) | (1 << LWSSSCS_QOS_NACK_REMOTE) |
					 (1 << LWSSSCS_ALL_RETRIES_FAILED),
		0
	},
	{
		"h1:443 just get 200",
		"t_h1_tls", 5 * LWS_US_PER_SEC, LWSSSCS_QOS_ACK_REMOTE,
		(1 << LWSSSCS_TIMEOUT) | (1 << LWSSSCS_QOS_NACK_REMOTE) |
					 (1 << LWSSSCS_ALL_RETRIES_FAILED),
		0
	},
	{
		"h2:443 just get 200",
		"t_h2_tls", 5 * LWS_US_PER_SEC, LWSSSCS_QOS_ACK_REMOTE,
		(1 << LWSSSCS_TIMEOUT) | (1 << LWSSSCS_QOS_NACK_REMOTE) |
					 (1 << LWSSSCS_ALL_RETRIES_FAILED),
		0
	},

	/*
	 * We arranged that the server will delay 10s before sending the
	 * response, but set our ss timeout for 5s.  So we expect to see
	 * our timeout and not an ACK / 200.
	 */

	{
		"h1:80 timeout after connection",
		"d_h1", 5 * LWS_US_PER_SEC, LWSSSCS_TIMEOUT,
		(1 << LWSSSCS_QOS_ACK_REMOTE) | (1 << LWSSSCS_QOS_NACK_REMOTE) |
					 (1 << LWSSSCS_ALL_RETRIES_FAILED),
		0
	},
	{
		"h1:443 timeout after connection",
		"d_h1_tls", 5 * LWS_US_PER_SEC, LWSSSCS_TIMEOUT,
		(1 << LWSSSCS_QOS_ACK_REMOTE) | (1 << LWSSSCS_QOS_NACK_REMOTE) |
					 (1 << LWSSSCS_ALL_RETRIES_FAILED),
		0
	},
	{
		"h2:443 timeout after connection",
		"d_h2_tls", 5 * LWS_US_PER_SEC, LWSSSCS_TIMEOUT,
		(1 << LWSSSCS_QOS_ACK_REMOTE) | (1 << LWSSSCS_QOS_NACK_REMOTE) |
					 (1 << LWSSSCS_ALL_RETRIES_FAILED),
		0
	},

	/*
	 * We are talking to a nonexistant dns address "bogus.nope".  We expect
	 * in each case to hear that is unreachable, before any ss timeout.
	 */

	{
		"h1:80 NXDOMAIN",
		"nxd_h1", 65 * LWS_US_PER_SEC, LWSSSCS_UNREACHABLE,
		(1 << LWSSSCS_QOS_ACK_REMOTE) | (1 << LWSSSCS_QOS_NACK_REMOTE) |
		(1 << LWSSSCS_TIMEOUT) | (1 << LWSSSCS_ALL_RETRIES_FAILED),
		0
	},
	{
		"h1:443 NXDOMAIN",
		"nxd_h1_tls", 35 * LWS_US_PER_SEC, LWSSSCS_UNREACHABLE,
		(1 << LWSSSCS_QOS_ACK_REMOTE) | (1 << LWSSSCS_QOS_NACK_REMOTE) |
		(1 << LWSSSCS_TIMEOUT) | (1 << LWSSSCS_ALL_RETRIES_FAILED),
		0
	},
	{
		"h2:443 NXDOMAIN",
		"nxd_h2_tls", 35 * LWS_US_PER_SEC, LWSSSCS_UNREACHABLE,
		(1 << LWSSSCS_QOS_ACK_REMOTE) | (1 << LWSSSCS_QOS_NACK_REMOTE) |
		(1 << LWSSSCS_TIMEOUT) | (1 << LWSSSCS_ALL_RETRIES_FAILED),
		0
	},

	/*
	 * We are talking to a nonexistant dns address "bogus.nope".  We expect
	 * that if we stick around longer, retries will also end up all failing.
	 * We might see the timeout depending on blocking getaddrinfo
	 * behaviour.
	 */

	{
		"h1:80 NXDOMAIN exhaust retries",
		"nxd_h1", 65 * LWS_US_PER_SEC, LWSSSCS_ALL_RETRIES_FAILED,
		(1 << LWSSSCS_QOS_ACK_REMOTE) | (1 << LWSSSCS_QOS_NACK_REMOTE),
		0
	},
	{
		"h1:443 NXDOMAIN exhaust retries",
		"nxd_h1_tls", 65 * LWS_US_PER_SEC, LWSSSCS_ALL_RETRIES_FAILED,
		(1 << LWSSSCS_QOS_ACK_REMOTE) | (1 << LWSSSCS_QOS_NACK_REMOTE),
		0
	},
	{
		"h2:443 NXDOMAIN exhaust retries",
		"nxd_h2_tls", 65 * LWS_US_PER_SEC, LWSSSCS_ALL_RETRIES_FAILED,
		(1 << LWSSSCS_QOS_ACK_REMOTE) | (1 << LWSSSCS_QOS_NACK_REMOTE),
		0
	},

	/*
	 * Let's request some bulk data from httpbin.org
	 */

	{
		"h1:80 read bulk",
		"bulk_h1", 5 * LWS_US_PER_SEC, LWSSSCS_QOS_ACK_REMOTE,
		(1 << LWSSSCS_TIMEOUT) | (1 << LWSSSCS_QOS_NACK_REMOTE) |
		(1 << LWSSSCS_ALL_RETRIES_FAILED),
		12345
	},
	{
		"h1:443 read bulk",
		"bulk_h1_tls", 5 * LWS_US_PER_SEC, LWSSSCS_QOS_ACK_REMOTE,
		(1 << LWSSSCS_TIMEOUT) | (1 << LWSSSCS_QOS_NACK_REMOTE) |
		(1 << LWSSSCS_ALL_RETRIES_FAILED),
		12345
	},
	{
		"h2:443 read bulk",
		"bulk_h2_tls", 5 * LWS_US_PER_SEC, LWSSSCS_QOS_ACK_REMOTE,
		(1 << LWSSSCS_TIMEOUT) | (1 << LWSSSCS_QOS_NACK_REMOTE) |
		(1 << LWSSSCS_ALL_RETRIES_FAILED),
		12345
	},

	/*
	 * Let's fail at the tls negotiation various ways
	 */

	{
		"h1:badcert_hostname",
		"badcert_hostname", 6 * LWS_US_PER_SEC, LWSSSCS_ALL_RETRIES_FAILED,
		(1 << LWSSSCS_QOS_NACK_REMOTE),
		0
	},
	{
		"h1:badcert_expired",
		"badcert_expired", 6 * LWS_US_PER_SEC, LWSSSCS_ALL_RETRIES_FAILED,
		(1 << LWSSSCS_QOS_NACK_REMOTE),
		0
	},
	{
		"h1:badcert_selfsigned",
		"badcert_selfsigned", 6 * LWS_US_PER_SEC, LWSSSCS_ALL_RETRIES_FAILED,
		(1 << LWSSSCS_QOS_NACK_REMOTE),
		0
	},

};

typedef struct myss {
	struct lws_ss_handle 		*ss;
	void				*opaque_data;

	size_t				rx_seen;
	char				result_reported;
} myss_t;


/* secure streams payload interface */

static lws_ss_state_return_t
myss_rx(void *userobj, const uint8_t *buf, size_t len, int flags)
{
	myss_t *m = (myss_t *)userobj;

	m->rx_seen += len;

	if (flags & LWSSS_FLAG_EOM)
		lwsl_notice("%s: %s len %d, fl %d, received %u bytes\n",
				__func__, lws_ss_tag(m->ss), (int)len, flags,
				(unsigned int)m->rx_seen);

	return 0;
}

static lws_ss_state_return_t
myss_tx(void *userobj, lws_ss_tx_ordinal_t ord, uint8_t *buf, size_t *len,
	int *flags)
{
	//myss_t *m = (myss_t *)userobj;

	/* in this example, we don't send stuff */

	return LWSSSSRET_TX_DONT_SEND;
}

static lws_ss_state_return_t
myss_state(void *userobj, void *sh, lws_ss_constate_t state,
	   lws_ss_tx_ordinal_t ack)
{
	myss_t *m = (myss_t *)userobj;
	struct tests_seq *curr_test = (	struct tests_seq *)m->opaque_data;
	char buf[8];
	size_t sl;

	lwsl_info("%s: %s: %s (%d), ord 0x%x\n", __func__, lws_ss_tag(m->ss),
		  lws_ss_state_name((int)state), state, (unsigned int)ack);

	if (curr_test->mask_unexpected & (1u << state)) {
		/*
		 * We have definitively failed on an unexpected state received
		 */

		lwsl_warn("%s: failing on unexpected state %s\n",
				__func__, lws_ss_state_name((int)state));

fail:
		m->result_reported = 1;
		tests_fail++;
		/* we'll start the next test next time around the event loop */
		lws_sul_schedule(context, 0, &sul_next_test, tests_start_next, 1);

		return LWSSSSRET_OK;
	}

	if (state == curr_test->must_see) {

		if (curr_test->eom_pass != m->rx_seen) {
			lwsl_notice("%s: failing on rx %d, expected %d\n",
				    __func__, (int)m->rx_seen,
				    (int)curr_test->eom_pass);
			goto fail;
		}

		lwsl_warn("%s: saw expected state %s\n",
				__func__, lws_ss_state_name((int)state));
		m->result_reported = 1;
		tests_pass++;
		/* we'll start the next test next time around the event loop */
		lws_sul_schedule(context, 0, &sul_next_test, tests_start_next, 1);

		return LWSSSSRET_OK;
	}

	switch (state) {
	case LWSSSCS_CREATING:
		lws_ss_start_timeout(m->ss,
			(unsigned int)(curr_test->timeout_us / LWS_US_PER_MS));
		if (curr_test->eom_pass) {
			sl = (size_t)lws_snprintf(buf, sizeof(buf), "%u",
					(unsigned int)curr_test->eom_pass);
			if (lws_ss_set_metadata(m->ss, "amount", buf, sl))
				return LWSSSSRET_DISCONNECT_ME;
		}
		return lws_ss_client_connect(m->ss);

	case LWSSSCS_DESTROYING:
		if (!m->result_reported) {
			lwsl_user("%s: failing on unexpected destruction\n",
					__func__);

			tests_fail++;
			/* we'll start the next test next time around the event loop */
			lws_sul_schedule(context, 0, &sul_next_test, tests_start_next, 1);
		}
		break;

	default:
		break;
	}

	return LWSSSSRET_OK;
}

static void
tests_start_next(lws_sorted_usec_list_t *sul)
{
	struct tests_seq *ts;
	lws_ss_info_t ssi;
	static struct lws_ss_handle *h;

	/* destroy the old one */

	if (h) {
		lwsl_info("%s: destroying previous stream\n", __func__);
		lws_ss_destroy(&h);
	}

	if ((unsigned int)tests >= LWS_ARRAY_SIZE(tests_seq)) {
		lwsl_notice("Completed all tests\n");
		interrupted = 1;
		return;
	}

	ts = &tests_seq[tests++];

	/* Create the next test stream */

	memset(&ssi, 0, sizeof(ssi));
	ssi.handle_offset = offsetof(myss_t, ss);
	ssi.opaque_user_data_offset = offsetof(myss_t, opaque_data);
	ssi.rx = myss_rx;
	ssi.tx = myss_tx;
	ssi.state = myss_state;
	ssi.user_alloc = sizeof(myss_t);
	ssi.streamtype = ts->streamtype;

	lwsl_user("%s: %d: %s\n", __func__, tests, ts->name);

	if (lws_ss_create(context, 0, &ssi, ts, &h, NULL, NULL)) {
		lwsl_err("%s: failed to create secure stream\n",
			 __func__);
		tests_fail++;
		interrupted = 1;
		return;
	}
}

static int
app_system_state_nf(lws_state_manager_t *mgr, lws_state_notify_link_t *link,
		    int current, int target)
{
	switch (target) {

	case LWS_SYSTATE_OPERATIONAL:
		if (current == LWS_SYSTATE_OPERATIONAL)
			/* we'll start the next test next time around the event loop */
			lws_sul_schedule(context, 0, &sul_next_test, tests_start_next, 1);
		break;
	}

	return 0;
}

static lws_state_notify_link_t * const app_notifier_list[] = {
	&nl, NULL
};

#if defined(LWS_WITH_SYS_METRICS)
static int
my_metric_report(lws_metric_pub_t *mp)
{
	lws_metric_bucket_t *sub = mp->u.hist.head;
	char buf[192];

	do {
		if (lws_metrics_format(mp, &sub, buf, sizeof(buf)))
			lwsl_user("%s: %s\n", __func__, buf);
	} while ((mp->flags & LWSMTFL_REPORT_HIST) && sub);

	/* 0 = leave metric to accumulate, 1 = reset the metric */

	return 1;
}

static const lws_system_ops_t system_ops = {
	.metric_report = my_metric_report,
};

#endif

static void
sigint_handler(int sig)
{
	interrupted = 1;
}

int
main(int argc, const char **argv)
{
	struct lws_context_creation_info info;
	const char *pp;

	signal(SIGINT, sigint_handler);

	memset(&info, 0, sizeof info);
	lws_cmdline_option_handle_builtin(argc, argv, &info);

	if ((pp = lws_cmdline_option(argc, argv, "--amount")))
		amount = (size_t)atoi(pp);

	/* set the expected payload for the bulk-related tests to amount */

	tests_seq[12].eom_pass = tests_seq[13].eom_pass =
					tests_seq[14].eom_pass = amount;
#if !defined(LWS_SS_USE_SSPC)
	// puts(default_ss_policy);
#endif

	lwsl_user("LWS secure streams error path tests [-d<verb>]\n");

	info.fd_limit_per_thread = 1 + 16 + 1;
	info.port = CONTEXT_PORT_NO_LISTEN;
#if defined(LWS_SS_USE_SSPC)
	info.protocols = lws_sspc_protocols;
	{
		const char *p;

		/* connect to ssproxy via UDS by default, else via
		 * tcp connection to this port */
		if ((p = lws_cmdline_option(argc, argv, "-p")))
			info.ss_proxy_port = (uint16_t)atoi(p);

		/* UDS "proxy.ss.lws" in abstract namespace, else this socket
		 * path; when -p given this can specify the network interface
		 * to bind to */
		if ((p = lws_cmdline_option(argc, argv, "-i")))
			info.ss_proxy_bind = p;

		/* if -p given, -a specifies the proxy address to connect to */
		if ((p = lws_cmdline_option(argc, argv, "-a")))
			info.ss_proxy_address = p;
	}
#else
	info.pss_policies_json = default_ss_policy;
	info.options = LWS_SERVER_OPTION_EXPLICIT_VHOSTS |
		       LWS_SERVER_OPTION_H2_JUST_FIX_WINDOW_UPDATE_OVERFLOW |
		       LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;
#endif

	/* integrate us with lws system state management when context created */

	nl.name = "app";
	nl.notify_cb = app_system_state_nf;
	info.register_notifier_list = app_notifier_list;

#if defined(LWS_WITH_SYS_METRICS)
	info.system_ops = &system_ops;
#if defined(LWS_WITH_SECURE_STREAMS_PROXY_API)
	info.metrics_prefix = "ssmex";
#endif
#endif

	/* create the context */

	context = lws_create_context(&info);
	if (!context) {
		lwsl_err("lws init failed\n");
		return 1;
	}

	/* the event loop */

	do { } while(lws_service(context, 0) >= 0 && !interrupted);

	lws_context_destroy(context);

	lwsl_user("Completed: %s (pass %d, fail %d)\n",
		  tests_pass == tests && !tests_fail ? "OK" : "failed",
				  tests_pass, tests_fail);

	return !(tests_pass == tests && !tests_fail);
}
