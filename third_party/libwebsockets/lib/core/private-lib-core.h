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

#if !defined(__LWS_PRIVATE_LIB_CORE_H__)
#define __LWS_PRIVATE_LIB_CORE_H__

#include "lws_config.h"
#include "lws_config_private.h"


#if defined(LWS_WITH_CGI) && defined(LWS_HAVE_VFORK) && \
    !defined(NO_GNU_SOURCE_THIS_TIME) && !defined(_GNU_SOURCE)
 #define  _GNU_SOURCE
#endif

/*
#if !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200112L
#endif
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <limits.h>
#include <stdarg.h>
#include <errno.h>

#ifdef LWS_HAVE_INTTYPES_H
#include <inttypes.h>
#endif

#include <assert.h>

#ifdef LWS_HAVE_SYS_TYPES_H
 #include <sys/types.h>
#endif
#if defined(LWS_HAVE_SYS_STAT_H) && !defined(LWS_PLAT_OPTEE)
 #include <sys/stat.h>
#endif

#if LWS_MAX_SMP > 1 || defined(LWS_WITH_SYS_SMD)
 /* https://stackoverflow.com/questions/33557506/timespec-redefinition-error */
 #define HAVE_STRUCT_TIMESPEC
 #include <pthread.h>
#else
 #if !defined(pid_t) && defined(WIN32)
 #define pid_t int
 #endif
#endif

#ifndef LWS_DEF_HEADER_LEN
#define LWS_DEF_HEADER_LEN 4096
#endif
#ifndef LWS_DEF_HEADER_POOL
#define LWS_DEF_HEADER_POOL 4
#endif
#ifndef LWS_MAX_PROTOCOLS
#define LWS_MAX_PROTOCOLS 5
#endif
#ifndef LWS_MAX_EXTENSIONS_ACTIVE
#define LWS_MAX_EXTENSIONS_ACTIVE 1
#endif
#ifndef LWS_MAX_EXT_OFFERS
#define LWS_MAX_EXT_OFFERS 8
#endif
#ifndef SPEC_LATEST_SUPPORTED
#define SPEC_LATEST_SUPPORTED 13
#endif
#ifndef CIPHERS_LIST_STRING
#define CIPHERS_LIST_STRING "DEFAULT"
#endif
#ifndef LWS_SOMAXCONN
#define LWS_SOMAXCONN SOMAXCONN
#endif

#define MAX_WEBSOCKET_04_KEY_LEN 128

#ifndef SYSTEM_RANDOM_FILEPATH
#define SYSTEM_RANDOM_FILEPATH "/dev/urandom"
#endif

#define LWS_H2_RX_SCRATCH_SIZE 512

#define lws_socket_is_valid(x) (x != LWS_SOCK_INVALID)

#ifndef LWS_HAVE_STRERROR
 #define strerror(x) ""
#endif

 /*
  *
  *  ------ private platform defines ------
  *
  */

#if defined(LWS_PLAT_FREERTOS)
 #include "private-lib-plat-freertos.h"
#else
 #if defined(WIN32) || defined(_WIN32)
  #include "private-lib-plat-windows.h"
 #else
  #if defined(LWS_PLAT_OPTEE)
   #include "private-lib-plat.h"
  #else
   #include "private-lib-plat-unix.h"
  #endif
 #endif
#endif

 /*
  *
  *  ------ public api ------
  *
  */

#include "libwebsockets.h"

/*
 * lws_dsh
*/

typedef struct lws_dsh_obj_head {
	lws_dll2_owner_t		owner;
	size_t				total_size; /* for this kind in dsh */
	int				kind;
} lws_dsh_obj_head_t;

typedef struct lws_dsh_obj {
	lws_dll2_t			list;	/* must be first */
	struct lws_dsh	  		*dsh;	/* invalid when on free list */
	size_t				size;	/* invalid when on free list */
	size_t				asize;
	int				kind; /* so we can account at free */
} lws_dsh_obj_t;

typedef struct lws_dsh {
	lws_dll2_t			list;
	uint8_t				*buf;
	lws_dsh_obj_head_t		*oha;	/* array of object heads/kind */
	size_t				buffer_size;
	size_t				locally_in_use;
	size_t				locally_free;
	int				count_kinds;
	uint8_t				being_destroyed;
	/*
	 * Overallocations at create:
	 *
	 *  - the buffer itself
	 *  - the object heads array
	 */
} lws_dsh_t;

 /*
  *
  *  ------ lifecycle defines ------
  *
  */

typedef struct lws_lifecycle_group {
	lws_dll2_owner_t		owner; /* active count / list */
	uint64_t			ordinal; /* monotonic uid count */
	const char			*tag_prefix; /* eg, "wsi" */
} lws_lifecycle_group_t;

typedef struct lws_lifecycle {
#if defined(LWS_WITH_SECURE_STREAMS_PROXY_API)
	/* we append parent streams on the tag */
	char				gutag[96]; /* object unique tag + relationship info */
#else
	char				gutag[64];
#endif
	lws_dll2_t			list; /* group list membership */
	uint64_t			us_creation; /* creation timestamp */
	lws_log_cx_t			*log_cx;
} lws_lifecycle_t;

void
__lws_lc_tag(struct lws_context *cx, lws_lifecycle_group_t *grp,
	     lws_lifecycle_t *lc, const char *format, ...);

void
__lws_lc_tag_append(lws_lifecycle_t *lc, const char *app);

void
__lws_lc_untag(struct lws_context *cx, lws_lifecycle_t *lc);

const char *
lws_lc_tag(lws_lifecycle_t *lc);

extern lws_log_cx_t log_cx;

/*
 * Generic bidi tx credit management
 */

struct lws_tx_credit {
	int32_t			tx_cr;		/* our credit to write peer */
	int32_t			peer_tx_cr_est; /* peer's credit to write us */

	int32_t			manual_initial_tx_credit;

	uint8_t			skint; /* unable to write anything */
	uint8_t			manual;
};

#ifdef LWS_WITH_IPV6
#if defined(WIN32) || defined(_WIN32)
#include <iphlpapi.h>
#else
#include <net/if.h>
#endif
#endif

#undef X509_NAME

/*
 * All lws_tls...() functions must return this type, converting the
 * native backend result and doing the extra work to determine which one
 * as needed.
 *
 * Native TLS backend return codes are NOT ALLOWED outside the backend.
 *
 * Non-SSL mode also uses these types.
 */
enum lws_ssl_capable_status {
	LWS_SSL_CAPABLE_ERROR			= -1, /* it failed */
	LWS_SSL_CAPABLE_DONE			= 0,  /* it succeeded */
	LWS_SSL_CAPABLE_MORE_SERVICE_READ	= -2, /* retry WANT_READ */
	LWS_SSL_CAPABLE_MORE_SERVICE_WRITE	= -3, /* retry WANT_WRITE */
	LWS_SSL_CAPABLE_MORE_SERVICE		= -4, /* general retry */
};

enum lws_context_destroy {
	LWSCD_NO_DESTROY,		/* running */
	LWSCD_PT_WAS_DEFERRED,		/* destroy from inside service */
	LWSCD_PT_WAIT_ALL_DESTROYED,	/* libuv ends up here later */
	LWSCD_FINALIZATION		/* the final destruction of context */
};

#if defined(LWS_WITH_TLS)
#include "private-lib-tls.h"
#endif

#if defined(WIN32) || defined(_WIN32)
	 // Visual studio older than 2015 and WIN_CE has only _stricmp
	#if (defined(_MSC_VER) && _MSC_VER < 1900) || defined(_WIN32_WCE)
	#define strcasecmp _stricmp
	#define strncasecmp _strnicmp
	#elif !defined(__MINGW32__)
	#define strcasecmp stricmp
	#define strncasecmp strnicmp
	#endif
	#define getdtablesize() 30000
#endif

#ifndef LWS_ARRAY_SIZE
#define LWS_ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define lws_safe_modulo(_a, _b) ((_b) ? ((_a) % (_b)) : 0)

#if defined(__clang__)
#define lws_memory_barrier() __sync_synchronize()
#elif defined(__GNUC__)
#define lws_memory_barrier() __sync_synchronize()
#else
#define lws_memory_barrier()
#endif


struct lws_ring {
	void *buf;
	void (*destroy_element)(void *element);
	uint32_t buflen;
	uint32_t element_len;
	uint32_t head;
	uint32_t oldest_tail;
};

struct lws_protocols;
struct lws;

#if defined(LWS_WITH_NETWORK) /* network */
#include "private-lib-event-libs.h"

#if defined(LWS_WITH_SECURE_STREAMS)
#include "private-lib-secure-streams.h"
#endif

#if defined(LWS_WITH_SYS_SMD)
#include "private-lib-system-smd.h"
#endif

#if defined(LWS_WITH_SYS_FAULT_INJECTION)
#include "private-lib-system-fault-injection.h"
#endif

#include "private-lib-system-metrics.h"


struct lws_foreign_thread_pollfd {
	struct lws_foreign_thread_pollfd *next;
	int fd_index;
	int _and;
	int _or;
};
#endif /* network */

#if defined(LWS_WITH_NETWORK)
#include "private-lib-core-net.h"
#endif

struct lws_system_blob {
	union {
		struct lws_buflist *bl;
		struct {
			const uint8_t *ptr;
			size_t len;
		} direct;
	} u;
	char	is_direct;
};


typedef struct lws_attach_item {
	lws_dll2_t			list;
	lws_attach_cb_t			cb;
	void				*opaque;
	lws_system_states_t		state;
} lws_attach_item_t;

/*
 * These are the context's lifecycle group indexes that exist in this build
 * configuration.  If you add some, make sure to also add the tag_prefix in
 * context.c context creation with matching preprocessor conditionals.
 */

enum {
	LWSLCG_WSI,			/* generic wsi, eg, pipe, listen */
	LWSLCG_VHOST,

	LWSLCG_WSI_SERVER,		/* server wsi */

#if defined(LWS_ROLE_H2) || defined(LWS_ROLE_MQTT)
	LWSLCG_WSI_MUX,			/* a mux child wsi */
#endif

#if defined(LWS_WITH_CLIENT)
	LWSLCG_WSI_CLIENT,		/* client wsi */
#endif

#if defined(LWS_WITH_SECURE_STREAMS)
#if defined(LWS_WITH_CLIENT)
	LWSLCG_SS_CLIENT,		/* secstream client handle */
#endif
#if defined(LWS_WITH_SERVER)
	LWSLCG_SS_SERVER,		/* secstream server handle */
#endif
#if defined(LWS_WITH_CLIENT)
	LWSLCG_WSI_SS_CLIENT,		/* wsi bound to ss client handle */
#endif
#if defined(LWS_WITH_SERVER)
	LWSLCG_WSI_SS_SERVER,		/* wsi bound to ss server handle */
#endif
#endif

#if defined(LWS_WITH_SECURE_STREAMS_PROXY_API)
#if defined(LWS_WITH_CLIENT)
	LWSLCG_SSP_CLIENT,		/* SSPC handle client connection to proxy */
#endif
#if defined(LWS_WITH_SERVER)
	LWSLCG_SSP_ONWARD,		/* SS handle at proxy for onward conn */
#endif
#if defined(LWS_WITH_CLIENT)
	LWSLCG_WSI_SSP_CLIENT,		/* wsi bound to SSPC cli conn to proxy */
#endif
#if defined(LWS_WITH_SERVER)
	LWSLCG_WSI_SSP_ONWARD,		/* wsi bound to Proxy onward connection */
#endif
#endif

	/* always last */
	LWSLCG_COUNT
};

/*
 * the rest is managed per-context, that includes
 *
 *  - processwide single fd -> wsi lookup
 *  - contextwide headers pool
 */

struct lws_context {
 #if defined(LWS_WITH_SERVER)
	char canonical_hostname[96];
 #endif

#if defined(LWS_WITH_FILE_OPS)
	struct lws_plat_file_ops fops_platform;
#endif

#if defined(LWS_WITH_ZIP_FOPS)
	struct lws_plat_file_ops fops_zip;
#endif

	lws_system_blob_t system_blobs[LWS_SYSBLOB_TYPE_COUNT];

#if defined(LWS_WITH_SYS_SMD)
	lws_smd_t				smd;
#endif
#if defined(LWS_WITH_SECURE_STREAMS)
	struct lws_ss_handle			*ss_cpd;
#endif
	lws_sorted_usec_list_t			sul_cpd_defer;

#if defined(LWS_WITH_NETWORK)
	struct lws_context_per_thread		pt[LWS_MAX_SMP];
	lws_retry_bo_t				default_retry;
	lws_sorted_usec_list_t			sul_system_state;

	lws_lifecycle_group_t			lcg[LWSLCG_COUNT];

	const struct lws_protocols		*protocols_copy;

#if defined(LWS_WITH_NETLINK)
	lws_sorted_usec_list_t			sul_nl_coldplug;
	/* process can only have one netlink socket, have to do it in ctx */
	lws_dll2_owner_t			routing_table;
	struct lws				*netlink;
#endif

#if defined(LWS_PLAT_FREERTOS)
	struct sockaddr_in			frt_pipe_si;
#endif

#if defined(LWS_WITH_HTTP2)
	struct http2_settings			set;
#endif

#if LWS_MAX_SMP > 1
	struct lws_mutex_refcount		mr;
#endif

#if defined(LWS_WITH_SYS_METRICS)
	lws_dll2_owner_t			owner_mtr_dynpol;
	/**< owner for lws_metric_policy_dyn_t (dynamic part of metric pols) */
	lws_dll2_owner_t			owner_mtr_no_pol;
	/**< owner for lws_metric_pub_t with no policy to bind to */
#endif

#if defined(LWS_WITH_NETWORK)
/*
 * LWS_WITH_NETWORK =====>
 */

	lws_dll2_owner_t		owner_vh_being_destroyed;

	lws_metric_t			*mt_service; /* doing service */
	const lws_metric_policy_t	*metrics_policies;
	const char			*metrics_prefix;

#if defined(LWS_WITH_SYS_METRICS) && defined(LWS_WITH_CLIENT)
	lws_metric_t			*mt_conn_tcp; /* client tcp conns */
	lws_metric_t			*mt_conn_tls; /* client tcp conns */
	lws_metric_t			*mt_conn_dns; /* client dns external lookups */
	lws_metric_t			*mth_conn_failures; /* histogram of conn failure reasons */
#if defined(LWS_ROLE_H1) || defined(LWS_ROLE_H2)
	lws_metric_t			*mt_http_txn; /* client http transaction */
#endif
#if defined(LWS_WITH_SYS_ASYNC_DNS)
	lws_metric_t			*mt_adns_cache; /* async dns lookup lat */
#endif
#if defined(LWS_WITH_SECURE_STREAMS)
	lws_metric_t			*mth_ss_conn; /* SS connection outcomes */
#endif
#if defined(LWS_WITH_SECURE_STREAMS_PROXY_API)
	lws_metric_t			*mt_ss_cliprox_conn; /* SS cli->prox conn */
	lws_metric_t			*mt_ss_cliprox_paylat; /* cli->prox payload latency */
	lws_metric_t			*mt_ss_proxcli_paylat; /* prox->cli payload latency */
#endif
#endif /* client */

#if defined(LWS_WITH_SERVER)
	lws_metric_t			*mth_srv;
#endif

#if defined(LWS_WITH_EVENT_LIBS)
	struct lws_plugin		*evlib_plugin_list;
	void				*evlib_ctx; /* overallocated */
#endif

#if defined(LWS_WITH_TLS)
	struct lws_context_tls		tls;
#if defined (LWS_WITH_TLS_JIT_TRUST)
	lws_dll2_owner_t		jit_inflight;
	/* ongoing sync or async jit trust lookups */
	struct lws_cache_ttl_lru	*trust_cache;
	/* caches host -> truncated trust SKID mappings */
#endif
#endif
#if defined(LWS_WITH_DRIVERS)
	lws_netdevs_t			netdevs;
#endif

#if defined(LWS_WITH_SYS_ASYNC_DNS)
	lws_async_dns_t			async_dns;
#endif

#if defined(LWS_WITH_SYS_FAULT_INJECTION)
	lws_fi_ctx_t			fic;
	/**< Toplevel Fault Injection ctx */
#endif

#if defined(LWS_WITH_CACHE_NSCOOKIEJAR) && defined(LWS_WITH_CLIENT)
	struct lws_cache_ttl_lru *l1, *nsc;
#endif

#if defined(LWS_WITH_SYS_NTPCLIENT)
	void				*ntpclient_priv;
#endif

#if defined(LWS_WITH_SECURE_STREAMS)
	struct lws_ss_handle		*hss_fetch_policy;
#if defined(LWS_WITH_SECURE_STREAMS_SYS_AUTH_API_AMAZON_COM)
	struct lws_ss_handle		*hss_auth;
	lws_sorted_usec_list_t		sul_api_amazon_com;
	lws_sorted_usec_list_t		sul_api_amazon_com_kick;
#endif
#if !defined(LWS_WITH_SECURE_STREAMS_STATIC_POLICY_ONLY)
	struct lws_ss_x509		*server_der_list;
#endif
#endif

#if defined(LWS_WITH_SYS_STATE)
	lws_state_manager_t		mgr_system;
	lws_state_notify_link_t		protocols_notify;
#endif
#if defined (LWS_WITH_SYS_DHCP_CLIENT)
	lws_dll2_owner_t		dhcpc_owner;
					/**< list of ifaces with dhcpc */
#endif

	/* pointers */

	struct lws_vhost		*vhost_list;
	struct lws_vhost		*no_listener_vhost_list;
	struct lws_vhost		*vhost_pending_destruction_list;
	struct lws_vhost		*vhost_system;

#if defined(LWS_WITH_SERVER)
	const char			*server_string;
#endif

	const struct lws_event_loop_ops	*event_loop_ops;
#endif

#if defined(LWS_WITH_TLS)
	const struct lws_tls_ops	*tls_ops;
#endif

#if defined(LWS_WITH_PLUGINS)
	struct lws_plugin		*plugin_list;
#endif
#ifdef _WIN32
/* different implementation between unix and windows */
	struct lws_fd_hashtable fd_hashtable[FD_HASHTABLE_MODULUS];
#else
	struct lws **lws_lookup;

#endif

/*
 * <====== LWS_WITH_NETWORK end
 */

#endif /* NETWORK */

	lws_log_cx_t			*log_cx;
	const char			*name;

#if defined(LWS_WITH_SECURE_STREAMS_PROXY_API)
	const char	*ss_proxy_bind;
	const char	*ss_proxy_address;
#endif

#if defined(LWS_WITH_FILE_OPS)
	const struct lws_plat_file_ops *fops;
#endif

	struct lws_context **pcontext_finalize;
#if !defined(LWS_PLAT_FREERTOS)
	const char *username, *groupname;
#endif

#if defined(LWS_WITH_MBEDTLS)
	mbedtls_entropy_context mec;
	mbedtls_ctr_drbg_context mcdc;
#endif

#if defined(LWS_WITH_THREADPOOL) && defined(LWS_HAVE_PTHREAD_H)
	struct lws_threadpool *tp_list_head;
#endif

#if defined(LWS_WITH_PEER_LIMITS)
	struct lws_peer			**pl_hash_table;
	struct lws_peer			*peer_wait_list;
	lws_peer_limits_notify_t	pl_notify_cb;
	time_t				next_cull;
#endif

	const lws_system_ops_t		*system_ops;

#if defined(LWS_WITH_SECURE_STREAMS)
#if !defined(LWS_WITH_SECURE_STREAMS_STATIC_POLICY_ONLY)
	const char			*pss_policies_json;
	struct lwsac			*ac_policy;
	void				*pol_args;
#endif
	const lws_ss_policy_t		*pss_policies;
	const lws_ss_auth_t		*pss_auths;
#if defined(LWS_WITH_SSPLUGINS)
	const lws_ss_plugin_t		**pss_plugins;
#endif
#endif

	void *external_baggage_free_on_destroy;
	const struct lws_token_limits *token_limits;
	void *user_space;
#if defined(LWS_WITH_SERVER)
	const struct lws_protocol_vhost_options *reject_service_keywords;
	lws_reload_func deprecation_cb;
#endif
#if !defined(LWS_PLAT_FREERTOS)
	void (*eventlib_signal_cb)(void *event_lib_handle, int signum);
#endif

#if defined(LWS_HAVE_SYS_CAPABILITY_H) && defined(LWS_HAVE_LIBCAP)
	cap_value_t caps[4];
	char count_caps;
#endif

	lws_usec_t time_up; /* monotonic */
#if defined(LWS_WITH_SYS_SMD)
	lws_usec_t smd_ttl_us;
#endif
	uint64_t options;

	time_t last_ws_ping_pong_check_s;
#if defined(LWS_WITH_SECURE_STREAMS)
	time_t					last_policy;
#endif

#if defined(LWS_PLAT_FREERTOS)
	unsigned long time_last_state_dump;
	uint32_t last_free_heap;
#endif

	unsigned int max_fds;
#if !defined(LWS_NO_DAEMONIZE)
	pid_t started_with_parent;
#endif

#if !defined(LWS_PLAT_FREERTOS)
	uid_t uid;
	gid_t gid;
	int fd_random;
	int count_cgi_spawned;
#endif

	unsigned int fd_limit_per_thread;
	unsigned int timeout_secs;
	unsigned int pt_serv_buf_size;
	unsigned int max_http_header_data;
	unsigned int max_http_header_pool;
	int simultaneous_ssl_restriction;
	int simultaneous_ssl;
	int simultaneous_ssl_handshake_restriction;
	int simultaneous_ssl_handshake;
#if defined(LWS_WITH_TLS_JIT_TRUST)
	int		vh_idle_grace_ms;
#endif
#if defined(LWS_WITH_PEER_LIMITS)
	uint32_t pl_hash_elements;	/* protected by context->lock */
	uint32_t count_peers;		/* protected by context->lock */
	unsigned short ip_limit_ah;
	unsigned short ip_limit_wsi;
#endif

#if defined(LWS_WITH_SYS_SMD)
	uint16_t smd_queue_depth;
#endif

#if defined(LWS_WITH_NETLINK)
	lws_route_uidx_t			route_uidx;
#endif

	char		tls_gate_accepts;

	unsigned int deprecated:1;
	unsigned int inside_context_destroy:1;
	unsigned int being_destroyed:1;
	unsigned int service_no_longer_possible:1;
	unsigned int being_destroyed2:1;
	unsigned int requested_stop_internal_loops:1;
	unsigned int protocol_init_done:1;
	unsigned int doing_protocol_init:1;
	unsigned int done_protocol_destroy_cb:1;
	unsigned int evlib_finalize_destroy_after_int_loops_stop:1;
	unsigned int max_fds_unrelated_to_ulimit:1;
	unsigned int policy_updated:1;
#if defined(LWS_WITH_NETLINK)
	unsigned int nl_initial_done:1;
#endif

	unsigned short count_threads;
	unsigned short undestroyed_threads;
	short plugin_protocol_count;
	short plugin_extension_count;
	short server_string_len;
	unsigned short deprecation_pending_listen_close_count;
#if defined(LWS_WITH_SECURE_STREAMS_PROXY_API)
	uint16_t	ss_proxy_port;
#endif
	/* 0 if not known, else us resolution of the poll wait */
	uint16_t us_wait_resolution;

	uint8_t max_fi;
	uint8_t captive_portal_detect;
	uint8_t captive_portal_detect_type;

	uint8_t		destroy_state; /* enum lws_context_destroy */
};

#define lws_get_context_protocol(ctx, x) ctx->vhost_list->protocols[x]
#define lws_get_vh_protocol(vh, x) vh->protocols[x]

int
lws_jws_base64_enc(const char *in, size_t in_len, char *out, size_t out_max);

void
lws_vhost_destroy1(struct lws_vhost *vh);

#if defined(LWS_WITH_CACHE_NSCOOKIEJAR) && defined(LWS_WITH_CLIENT)
int
lws_parse_set_cookie(struct lws *wsi);

int
lws_cookie_send_cookies(struct lws *wsi, char **pp, char *end);
#endif

#if defined(LWS_PLAT_FREERTOS)
int
lws_find_string_in_file(const char *filename, const char *str, int stringlen);
#endif

signed char char_to_hex(const char c);

#if defined(LWS_WITH_NETWORK)
int
lws_system_do_attach(struct lws_context_per_thread *pt);
#endif

struct lws_buflist {
	struct lws_buflist *next;
	size_t len;
	size_t pos;
};

char *
lws_strdup(const char *s);

int
lws_b64_selftest(void);


#ifndef LWS_NO_DAEMONIZE
 pid_t get_daemonize_pid();
#else
 #define get_daemonize_pid() (0)
#endif

void lwsl_emit_stderr(int level, const char *line);

#if !defined(LWS_WITH_TLS)
 #define LWS_SSL_ENABLED(context) (0)
 #define lws_context_init_server_ssl(_a, _b) (0)
 #define lws_ssl_destroy(_a)
 #define lws_context_init_alpn(_a)
 #define lws_ssl_capable_read lws_ssl_capable_read_no_ssl
 #define lws_ssl_capable_write lws_ssl_capable_write_no_ssl
 #define lws_ssl_pending lws_ssl_pending_no_ssl
 #define lws_server_socket_service_ssl(_b, _c, _d) (0)
 #define lws_ssl_close(_a) (0)
 #define lws_ssl_context_destroy(_a)
 #define lws_ssl_SSL_CTX_destroy(_a)
 #define lws_ssl_remove_wsi_from_buffered_list(_a)
 #define __lws_ssl_remove_wsi_from_buffered_list(_a)
 #define lws_context_init_ssl_library(_a, _b)
 #define lws_context_deinit_ssl_library(_a)
 #define lws_tls_check_all_cert_lifetimes(_a)
 #define lws_tls_acme_sni_cert_destroy(_a)
#endif



#if LWS_MAX_SMP > 1
#define lws_context_lock(c, reason) lws_mutex_refcount_lock(&c->mr, reason)
#define lws_context_unlock(c) lws_mutex_refcount_unlock(&c->mr)
#define lws_context_assert_lock_held(c) lws_mutex_refcount_assert_held(&c->mr)
#define lws_vhost_assert_lock_held(v) lws_mutex_refcount_assert_held(&v->mr)
/* enforce context lock held */
#define lws_vhost_lock(v) lws_mutex_refcount_lock(&v->mr, __func__)
#define lws_vhost_unlock(v) lws_mutex_refcount_unlock(&v->mr)


#else
#define lws_pt_mutex_init(_a) (void)(_a)
#define lws_pt_mutex_destroy(_a) (void)(_a)
#define lws_pt_lock(_a, b) (void)(_a)
#define lws_pt_assert_lock_held(_a) (void)(_a)
#define lws_pt_unlock(_a) (void)(_a)
#define lws_context_lock(_a, _b) (void)(_a)
#define lws_context_unlock(_a) (void)(_a)
#define lws_context_assert_lock_held(_a) (void)(_a)
#define lws_vhost_assert_lock_held(_a) (void)(_a)
#define lws_vhost_lock(_a) (void)(_a)
#define lws_vhost_unlock(_a) (void)(_a)
#define lws_pt_stats_lock(_a) (void)(_a)
#define lws_pt_stats_unlock(_a) (void)(_a)
#endif

int LWS_WARN_UNUSED_RESULT
lws_ssl_capable_read_no_ssl(struct lws *wsi, unsigned char *buf, size_t len);

int LWS_WARN_UNUSED_RESULT
lws_ssl_capable_write_no_ssl(struct lws *wsi, unsigned char *buf, size_t len);

int LWS_WARN_UNUSED_RESULT
lws_ssl_pending_no_ssl(struct lws *wsi);

int
lws_tls_check_cert_lifetime(struct lws_vhost *vhost);

int lws_jws_selftest(void);
int lws_jwe_selftest(void);

int
lws_protocol_init(struct lws_context *context);

int
lws_bind_protocol(struct lws *wsi, const struct lws_protocols *p,
		  const char *reason);

const struct lws_protocol_vhost_options *
lws_vhost_protocol_options(struct lws_vhost *vh, const char *name);

const struct lws_http_mount *
lws_find_mount(struct lws *wsi, const char *uri_ptr, int uri_len);

#ifdef LWS_WITH_HTTP2
int lws_wsi_is_h2(struct lws *wsi);
#endif
/*
 * custom allocator
 */
void *
lws_realloc(void *ptr, size_t size, const char *reason);

void * LWS_WARN_UNUSED_RESULT
lws_zalloc(size_t size, const char *reason);

#ifdef LWS_PLAT_OPTEE
void *lws_malloc(size_t size, const char *reason);
void lws_free(void *p);
#define lws_free_set_NULL(P)    do { lws_free(P); (P) = NULL; } while(0)
#else
#define lws_malloc(S, R)	lws_realloc(NULL, S, R)
#define lws_free(P)	lws_realloc(P, 0, "lws_free")
#define lws_free_set_NULL(P)	do { lws_realloc(P, 0, "free"); (P) = NULL; } while(0)
#endif

int
__lws_create_event_pipes(struct lws_context *context);

int
lws_plat_apply_FD_CLOEXEC(int n);

const struct lws_plat_file_ops *
lws_vfs_select_fops(const struct lws_plat_file_ops *fops, const char *vfs_path,
		    const char **vpath);

/* lws_plat_ */

int
lws_plat_context_early_init(void);
void
lws_plat_context_early_destroy(struct lws_context *context);
void
lws_plat_context_late_destroy(struct lws_context *context);

int
lws_plat_init(struct lws_context *context,
	      const struct lws_context_creation_info *info);
int
lws_plat_drop_app_privileges(struct lws_context *context, int actually_drop);

#if defined(LWS_WITH_UNIX_SOCK) && !defined(WIN32)
int
lws_plat_user_colon_group_to_ids(const char *u_colon_g, uid_t *puid, gid_t *pgid);
#endif

int
lws_plat_ntpclient_config(struct lws_context *context);

int
lws_plat_ifname_to_hwaddr(int fd, const char *ifname, uint8_t *hwaddr, int len);

int
lws_plat_vhost_tls_client_ctx_init(struct lws_vhost *vhost);

int
lws_check_byte_utf8(unsigned char state, unsigned char c);
int LWS_WARN_UNUSED_RESULT
lws_check_utf8(unsigned char *state, unsigned char *buf, size_t len);
int alloc_file(struct lws_context *context, const char *filename,
			  uint8_t **buf, lws_filepos_t *amount);

int
lws_lec_scratch(lws_lec_pctx_t *ctx);
void
lws_lec_signed(lws_lec_pctx_t *ctx, int64_t num);

int
lws_cose_key_checks(const lws_cose_key_t *key, int64_t kty, int64_t alg,
		    int key_op, const char *crv);

void lws_msleep(unsigned int);

void
lws_context_destroy2(struct lws_context *context);

#if !defined(PRIu64)
#define PRIu64 "llu"
#endif

#if defined(LWS_WITH_ABSTRACT)
#include "private-lib-abstract.h"
#endif

#ifdef __cplusplus
};
#endif

#endif
