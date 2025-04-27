/*
 * libwebsockets - small server side websockets and web server implementation
 *
 * Copyright (C) 2010 - 2021 Andy Green <andy@warmcat.com>
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

/** \defgroup log Logging
 *
 * ##Logging
 *
 * Lws provides flexible and filterable logging facilities, which can be
 * used inside lws and in user code.
 *
 * Log categories may be individually filtered bitwise, and directed to built-in
 * sinks for syslog-compatible logging, or a user-defined function.
 *
 * Traditional logs use a single, processwide logging context.  New style log
 * apis (lws_xxx_cx()) can pass the logging context to use in.
 */
///@{

#define LLL_ERR			(1 << 0)
#define	LLL_WARN		(1 << 1)
#define	LLL_NOTICE		(1 << 2)
#define	LLL_INFO		(1 << 3)
#define	LLL_DEBUG		(1 << 4)
#define	LLL_PARSER		(1 << 5)
#define	LLL_HEADER		(1 << 6)
#define	LLL_EXT			(1 << 7)
#define	LLL_CLIENT		(1 << 8)
#define	LLL_LATENCY		(1 << 9)
#define	LLL_USER		(1 << 10)
#define	LLL_THREAD		(1 << 11)

#define	LLL_COUNT		(12) /* set to count of valid flags */

#define	LLLF_SECRECY_PII	(1 << 16)
	/**< contains Personally Identifiable Information */
#define LLLF_SECRECY_BEARER	(1 << 17)
	/**< possession of this data allows impersonation */

#define	LLLF_LOG_TIMESTAMP	(1 << 18)
	/**< set to prepend logs with timestamp */

#define	LLLF_LOG_CONTEXT_AWARE	(1 << 30)
/**< set if the context uses an emit function that takes the logctx, auto-
 * applied when setting emit using lws_set_log_level_cx() api */

struct lws_log_cx;

typedef void (*lws_log_emit_t)(int level, const char *line);
typedef void (*lws_log_emit_cx_t)(struct lws_log_cx *cx, int level,
				  const char *line, size_t len);
typedef void (*lws_log_prepend_cx_t)(struct lws_log_cx *cx, void *obj,
				     char **p, char *e);
typedef void (*lws_log_use_cx_t)(struct lws_log_cx *cx, int _new);

/*
 * This is the logging context
 */

typedef struct lws_log_cx {
	union {
		lws_log_emit_t		emit; /* legacy emit function */
		lws_log_emit_cx_t	emit_cx; /* LLLF_LOG_CONTEXT_AWARE */
	} u;

#if LWS_MAX_SMP > 1
	pthread_mutex_t			refcount_lock;
#endif

	lws_log_use_cx_t		refcount_cb;
	/**< NULL, or a function called after each change to .refcount below,
	 * this enables implementing side-effects like opening and closing
	 * log files when the first and last object binds / unbinds */
	lws_log_prepend_cx_t		prepend;
	/**< NULL, or a cb to optionally prepend a string to logs we are a
	 * parent of */
	struct lws_log_cx		*parent;
	/**< NULL, or points to log ctx we are a child of */
	void				*opaque;
	/**< ignored by lws, used to pass config to emit_cx, eg, filepath */
	void				*stg;
	/**< ignored by lws, may be used a storage by refcount_cb / emit_cx */
	uint32_t			lll_flags;
	/**< mask of log levels we want to emit in this context */
	int32_t				refcount;
	/**< refcount of objects bound to this log context */
#if LWS_MAX_SMP > 1
	char				inited;
#endif
} lws_log_cx_t;

/**
 * lwsl_timestamp: generate logging timestamp string
 *
 * \param level:	logging level
 * \param p:		char * buffer to take timestamp
 * \param len:	length of p
 *
 * returns length written in p
 */
LWS_VISIBLE LWS_EXTERN int
lwsl_timestamp(int level, char *p, size_t len);

#if defined(LWS_PLAT_OPTEE) && !defined(LWS_WITH_NETWORK)
#define _lws_log(aaa, ...) SMSG(__VA_ARGS__)
#else
LWS_VISIBLE LWS_EXTERN void
_lws_log(int filter, const char *format, ...) LWS_FORMAT(2);
LWS_VISIBLE LWS_EXTERN void
_lws_logv(int filter, const char *format, va_list vl);
#endif

struct lws_vhost;
struct lws;

LWS_VISIBLE LWS_EXTERN struct lws_log_cx *
lwsl_context_get_cx(struct lws_context *cx);
LWS_VISIBLE LWS_EXTERN struct lws_log_cx *
lwsl_vhost_get_cx(struct lws_vhost *vh);
LWS_VISIBLE LWS_EXTERN struct lws_log_cx *
lwsl_wsi_get_cx(struct lws *wsi);
#if defined(LWS_WITH_SECURE_STREAMS)
struct lws_ss_handle;
struct lws_sspc_handle;
LWS_VISIBLE LWS_EXTERN struct lws_log_cx *
lwsl_ss_get_cx(struct lws_ss_handle *ss);
LWS_VISIBLE LWS_EXTERN struct lws_log_cx *
lwsl_sspc_get_cx(struct lws_sspc_handle *ss);
#endif

LWS_VISIBLE LWS_EXTERN void
lws_log_emit_cx_file(struct lws_log_cx *cx, int level, const char *line,
			size_t len);

LWS_VISIBLE LWS_EXTERN void
lws_log_use_cx_file(struct lws_log_cx *cx, int _new);

LWS_VISIBLE LWS_EXTERN void
lws_log_prepend_context(struct lws_log_cx *cx, void *obj, char **p, char *e);
LWS_VISIBLE LWS_EXTERN void
lws_log_prepend_vhost(struct lws_log_cx *cx, void *obj, char **p, char *e);
LWS_VISIBLE LWS_EXTERN void
lws_log_prepend_wsi(struct lws_log_cx *cx, void *obj, char **p, char *e);
#if defined(LWS_WITH_SECURE_STREAMS)
LWS_VISIBLE LWS_EXTERN void
lws_log_prepend_ss(struct lws_log_cx *cx, void *obj, char **p, char *e);
LWS_VISIBLE LWS_EXTERN void
lws_log_prepend_sspc(struct lws_log_cx *cx, void *obj, char **p, char *e);
#endif

LWS_VISIBLE LWS_EXTERN void
_lws_log_cx(lws_log_cx_t *cx, lws_log_prepend_cx_t prep, void *obj,
	    int filter, const char *_fun, const char *format, ...) LWS_FORMAT(6);

#define lwsl_cx(_c, _fil, ...) \
		 _lws_log_cx(lwsl_context_get_cx(_c), lws_log_prepend_context, \
					_c, _fil, __func__, __VA_ARGS__)
#define lwsl_vhost(_v, _fil, ...) \
		 _lws_log_cx(lwsl_vhost_get_cx(_v), lws_log_prepend_vhost, _v, \
					_fil, __func__, __VA_ARGS__)
#define lwsl_wsi(_w, _fil, ...) \
		 _lws_log_cx(lwsl_wsi_get_cx(_w), lws_log_prepend_wsi, _w, \
					_fil, __func__, __VA_ARGS__)
#define lwsl_ss(_h, _fil, ...) \
		 _lws_log_cx(lwsl_ss_get_cx(_h), lws_log_prepend_ss, _h, \
					_fil, __func__, __VA_ARGS__)

#define lwsl_hexdump_context(_c, _fil, _buf, _len) \
		lwsl_hexdump_level_cx(lwsl_context_get_cx(_c), \
				      lws_log_prepend_context, \
				      _c, _fil, _buf, _len)
#define lwsl_hexdump_vhost(_v, _fil, _buf, _len) \
		lwsl_hexdump_level_cx(lwsl_vhost_get_cx(_v), \
				      lws_log_prepend_vhost, \
				      _v, _fil, _buf, _len)
#define lwsl_hexdump_wsi(_w, _fil, _buf, _len) \
		lwsl_hexdump_level_cx(lwsl_wsi_get_cx(_w), \
				      lws_log_prepend_wsi, \
				      _w, _fil, _buf, _len)
#define lwsl_hexdump_ss(_h, _fil, _buf, _len) \
		lwsl_hexdump_level_cx(lwsl_ss_get_cx(_h), \
				      lws_log_prepend_ss, \
				      _h, _fil, _buf, _len)

/*
 * Figure out which logs to build in or not
 */

#if defined(_DEBUG)
 /*
  * In DEBUG build, select all logs unless NO_LOGS
  */
 #if defined(LWS_WITH_NO_LOGS)
  #define _LWS_LINIT (LLL_ERR | LLL_USER)
 #else
   #define _LWS_LINIT ((1 << LLL_COUNT) - 1)
 #endif
#else /* not _DEBUG */
#if defined(LWS_WITH_NO_LOGS)
#define _LWS_LINIT (LLL_ERR | LLL_USER)
#else
 #define _LWS_LINIT (LLL_ERR | LLL_USER | LLL_WARN | LLL_NOTICE)
#endif
#endif /* _DEBUG */

/*
 * Create either empty overrides or the ones forced at build-time.
 * These overrides have the final say... any bits set in
 * LWS_LOGGING_BITFIELD_SET force the build of those logs, any bits
 * set in LWS_LOGGING_BITFIELD_CLEAR disable the build of those logs.
 *
 * If not defined lws decides based on CMAKE_BUILD_TYPE=DEBUG or not
 */

#if defined(LWS_LOGGING_BITFIELD_SET)
 #define _LWS_LBS (LWS_LOGGING_BITFIELD_SET)
#else
 #define _LWS_LBS 0
#endif

#if defined(LWS_LOGGING_BITFIELD_CLEAR)
 #define _LWS_LBC (LWS_LOGGING_BITFIELD_CLEAR)
#else
 #define _LWS_LBC 0
#endif

/*
 * Compute the final active logging bitfield for build
 */
#define _LWS_ENABLED_LOGS (((_LWS_LINIT) | (_LWS_LBS)) & ~(_LWS_LBC))

/*
 * Individually enable or disable log levels for build
 * depending on what was computed
 */

/*
 * Process scope logs
 */

#if (_LWS_ENABLED_LOGS & LLL_ERR)
#define lwsl_err(...) _lws_log(LLL_ERR, __VA_ARGS__)
#else
#define lwsl_err(...) do {} while(0)
#endif

#if (_LWS_ENABLED_LOGS & LLL_WARN)
#define lwsl_warn(...) _lws_log(LLL_WARN, __VA_ARGS__)
#else
#define lwsl_warn(...) do {} while(0)
#endif

#if (_LWS_ENABLED_LOGS & LLL_NOTICE)
#define lwsl_notice(...) _lws_log(LLL_NOTICE, __VA_ARGS__)
#else
#define lwsl_notice(...) do {} while(0)
#endif

#if (_LWS_ENABLED_LOGS & LLL_INFO)
#define lwsl_info(...) _lws_log(LLL_INFO, __VA_ARGS__)
#else
#define lwsl_info(...) do {} while(0)
#endif

#if (_LWS_ENABLED_LOGS & LLL_DEBUG)
#define lwsl_debug(...) _lws_log(LLL_DEBUG, __VA_ARGS__)
#else
#define lwsl_debug(...) do {} while(0)
#endif

#if (_LWS_ENABLED_LOGS & LLL_PARSER)
#define lwsl_parser(...) _lws_log(LLL_PARSER, __VA_ARGS__)
#else
#define lwsl_parser(...) do {} while(0)
#endif

#if (_LWS_ENABLED_LOGS & LLL_HEADER)
#define lwsl_header(...) _lws_log(LLL_HEADER, __VA_ARGS__)
#else
#define lwsl_header(...) do {} while(0)
#endif

#if (_LWS_ENABLED_LOGS & LLL_EXT)
#define lwsl_ext(...) _lws_log(LLL_EXT, __VA_ARGS__)
#else
#define lwsl_ext(...) do {} while(0)
#endif

#if (_LWS_ENABLED_LOGS & LLL_CLIENT)
#define lwsl_client(...) _lws_log(LLL_CLIENT, __VA_ARGS__)
#else
#define lwsl_client(...) do {} while(0)
#endif

#if (_LWS_ENABLED_LOGS & LLL_LATENCY)
#define lwsl_latency(...) _lws_log(LLL_LATENCY, __VA_ARGS__)
#else
#define lwsl_latency(...) do {} while(0)
#endif

#if (_LWS_ENABLED_LOGS & LLL_THREAD)
#define lwsl_thread(...) _lws_log(LLL_THREAD, __VA_ARGS__)
#else
#define lwsl_thread(...) do {} while(0)
#endif

#if (_LWS_ENABLED_LOGS & LLL_USER)
#define lwsl_user(...) _lws_log(LLL_USER, __VA_ARGS__)
#else
#define lwsl_user(...) do {} while(0)
#endif

#define lwsl_hexdump_err(...) lwsl_hexdump_level(LLL_ERR, __VA_ARGS__)
#define lwsl_hexdump_warn(...) lwsl_hexdump_level(LLL_WARN, __VA_ARGS__)
#define lwsl_hexdump_notice(...) lwsl_hexdump_level(LLL_NOTICE, __VA_ARGS__)
#define lwsl_hexdump_info(...) lwsl_hexdump_level(LLL_INFO, __VA_ARGS__)
#define lwsl_hexdump_debug(...) lwsl_hexdump_level(LLL_DEBUG, __VA_ARGS__)

/*
 * lws_context scope logs
 */

#if (_LWS_ENABLED_LOGS & LLL_ERR)
#define lwsl_cx_err(_c, ...) lwsl_cx(_c, LLL_ERR, __VA_ARGS__)
#else
#define lwsl_cx_err(_c, ...) do {} while(0)
#endif

#if (_LWS_ENABLED_LOGS & LLL_WARN)
#define lwsl_cx_warn(_c, ...) lwsl_cx(_c, LLL_WARN, __VA_ARGS__)
#else
#define lwsl_cx_warn(_c, ...) do {} while(0)
#endif

#if (_LWS_ENABLED_LOGS & LLL_NOTICE)
#define lwsl_cx_notice(_c, ...) lwsl_cx(_c, LLL_NOTICE, __VA_ARGS__)
#else
#define lwsl_cx_notice(_c, ...) do {} while(0)
#endif

#if (_LWS_ENABLED_LOGS & LLL_INFO)
#define lwsl_cx_info(_c, ...) lwsl_cx(_c, LLL_INFO, __VA_ARGS__)
#else
#define lwsl_cx_info(_c, ...) do {} while(0)
#endif

#if (_LWS_ENABLED_LOGS & LLL_DEBUG)
#define lwsl_cx_debug(_c, ...) lwsl_cx(_c, LLL_DEBUG, __VA_ARGS__)
#else
#define lwsl_cx_debug(_c, ...) do {} while(0)
#endif

#if (_LWS_ENABLED_LOGS & LLL_PARSER)
#define lwsl_cx_parser(_c, ...) lwsl_cx(_c, LLL_PARSER, __VA_ARGS__)
#else
#define lwsl_cx_parser(_c, ...) do {} while(0)
#endif

#if (_LWS_ENABLED_LOGS & LLL_HEADER)
#define lwsl_cx_header(_c, ...) lwsl_cx(_c, LLL_HEADER, __VA_ARGS__)
#else
#define lwsl_cx_header(_c, ...) do {} while(0)
#endif

#if (_LWS_ENABLED_LOGS & LLL_EXT)
#define lwsl_cx_ext(_c, ...) lwsl_cx(_c, LLL_EXT, __VA_ARGS__)
#else
#define lwsl_cx_ext(_c, ...) do {} while(0)
#endif

#if (_LWS_ENABLED_LOGS & LLL_CLIENT)
#define lwsl_cx_client(_c, ...) lwsl_cx(_c, LLL_CLIENT, __VA_ARGS__)
#else
#define lwsl_cx_client(_c, ...) do {} while(0)
#endif

#if (_LWS_ENABLED_LOGS & LLL_LATENCY)
#define lwsl_cx_latency(_c, ...) lwsl_cx(_c, LLL_LATENCY, __VA_ARGS__)
#else
#define lwsl_cx_latency(_c, ...) do {} while(0)
#endif

#if (_LWS_ENABLED_LOGS & LLL_THREAD)
#define lwsl_cx_thread(_c, ...) lwsl_cx(_c, LLL_THREAD, __VA_ARGS__)
#else
#define lwsl_cx_thread(_c, ...) do {} while(0)
#endif

#if (_LWS_ENABLED_LOGS & LLL_USER)
#define lwsl_cx_user(_c, ...) lwsl_cx(_c, LLL_USER, __VA_ARGS__)
#else
#define lwsl_cx_user(_c, ...) do {} while(0)
#endif

#define lwsl_hexdump_cx_err(_c, ...)    lwsl_hexdump_context(_c, LLL_ERR, __VA_ARGS__)
#define lwsl_hexdump_cx_warn(_c, ...)   lwsl_hexdump_context(_c, LLL_WARN, __VA_ARGS__)
#define lwsl_hexdump_cx_notice(_c, ...) lwsl_hexdump_context(_c, LLL_NOTICE, __VA_ARGS__)
#define lwsl_hexdump_cx_info(_c, ...)   lwsl_hexdump_context(_c, LLL_INFO, __VA_ARGS__)
#define lwsl_hexdump_cx_debug(_c, ...)  lwsl_hexdump_context(_c, LLL_DEBUG, __VA_ARGS__)

/*
 * lws_vhost
 */

#if (_LWS_ENABLED_LOGS & LLL_ERR)
#define lwsl_vhost_err(_v, ...) lwsl_vhost(_v, LLL_ERR, __VA_ARGS__)
#else
#define lwsl_vhost_err(_v, ...) do {} while(0)
#endif

#if (_LWS_ENABLED_LOGS & LLL_WARN)
#define lwsl_vhost_warn(_v, ...) lwsl_vhost(_v, LLL_WARN, __VA_ARGS__)
#else
#define lwsl_vhost_warn(_v, ...) do {} while(0)
#endif

#if (_LWS_ENABLED_LOGS & LLL_NOTICE)
#define lwsl_vhost_notice(_v, ...) lwsl_vhost(_v, LLL_NOTICE, __VA_ARGS__)
#else
#define lwsl_vhost_notice(_v, ...) do {} while(0)
#endif

#if (_LWS_ENABLED_LOGS & LLL_INFO)
#define lwsl_vhost_info(_v, ...) lwsl_vhost(_v, LLL_INFO, __VA_ARGS__)
#else
#define lwsl_vhost_info(_v, ...) do {} while(0)
#endif

#if (_LWS_ENABLED_LOGS & LLL_DEBUG)
#define lwsl_vhost_debug(_v, ...) lwsl_vhost(_v, LLL_DEBUG, __VA_ARGS__)
#else
#define lwsl_vhost_debug(_v, ...) do {} while(0)
#endif

#if (_LWS_ENABLED_LOGS & LLL_PARSER)
#define lwsl_vhost_parser(_v, ...) lwsl_vhost(_v, LLL_PARSER, __VA_ARGS__)
#else
#define lwsl_vhost_parser(_v, ...) do {} while(0)
#endif

#if (_LWS_ENABLED_LOGS & LLL_HEADER)
#define lwsl_vhost_header(_v, ...) lwsl_vhost(_v, LLL_HEADER, __VA_ARGS__)
#else
#define lwsl_vhost_header(_v, ...) do {} while(0)
#endif

#if (_LWS_ENABLED_LOGS & LLL_EXT)
#define lwsl_vhost_ext(_v, ...) lwsl_vhost(_v, LLL_EXT, __VA_ARGS__)
#else
#define lwsl_vhost_ext(_v, ...) do {} while(0)
#endif

#if (_LWS_ENABLED_LOGS & LLL_CLIENT)
#define lwsl_vhost_client(_v, ...) lwsl_vhost(_v, LLL_CLIENT, __VA_ARGS__)
#else
#define lwsl_vhost_client(_v, ...) do {} while(0)
#endif

#if (_LWS_ENABLED_LOGS & LLL_LATENCY)
#define lwsl_vhost_latency(_v, ...) lwsl_vhost(_v, LLL_LATENCY, __VA_ARGS__)
#else
#define lwsl_vhost_latency(_v, ...) do {} while(0)
#endif

#if (_LWS_ENABLED_LOGS & LLL_THREAD)
#define lwsl_vhost_thread(_v, ...) lwsl_vhost(_v, LLL_THREAD, __VA_ARGS__)
#else
#define lwsl_vhost_thread(_v, ...) do {} while(0)
#endif

#if (_LWS_ENABLED_LOGS & LLL_USER)
#define lwsl_vhost_user(_v, ...) lwsl_vhost(_v, LLL_USER, __VA_ARGS__)
#else
#define lwsl_vhost_user(_v, ...) do {} while(0)
#endif

#define lwsl_hexdump_vhost_err(_v, ...)    lwsl_hexdump_vhost(_v, LLL_ERR, __VA_ARGS__)
#define lwsl_hexdump_vhost_warn(_v, ...)   lwsl_hexdump_vhost(_v, LLL_WARN, __VA_ARGS__)
#define lwsl_hexdump_vhost_notice(_v, ...) lwsl_hexdump_vhost(_v, LLL_NOTICE, __VA_ARGS__)
#define lwsl_hexdump_vhost_info(_v, ...)   lwsl_hexdump_vhost(_v, LLL_INFO, __VA_ARGS__)
#define lwsl_hexdump_vhost_debug(_v, ...)  lwsl_hexdump_vhost(_v, LLL_DEBUG, __VA_ARGS__)


/*
 * lws_wsi
 */

#if (_LWS_ENABLED_LOGS & LLL_ERR)
#define lwsl_wsi_err(_w, ...) lwsl_wsi(_w, LLL_ERR, __VA_ARGS__)
#else
#define lwsl_wsi_err(_w, ...) do {} while(0)
#endif

#if (_LWS_ENABLED_LOGS & LLL_WARN)
#define lwsl_wsi_warn(_w, ...) lwsl_wsi(_w, LLL_WARN, __VA_ARGS__)
#else
#define lwsl_wsi_warn(_w, ...) do {} while(0)
#endif

#if (_LWS_ENABLED_LOGS & LLL_NOTICE)
#define lwsl_wsi_notice(_w, ...) lwsl_wsi(_w, LLL_NOTICE, __VA_ARGS__)
#else
#define lwsl_wsi_notice(_w, ...) do {} while(0)
#endif

#if (_LWS_ENABLED_LOGS & LLL_INFO)
#define lwsl_wsi_info(_w, ...) lwsl_wsi(_w, LLL_INFO, __VA_ARGS__)
#else
#define lwsl_wsi_info(_w, ...) do {} while(0)
#endif

#if (_LWS_ENABLED_LOGS & LLL_DEBUG)
#define lwsl_wsi_debug(_w, ...) lwsl_wsi(_w, LLL_DEBUG, __VA_ARGS__)
#else
#define lwsl_wsi_debug(_w, ...) do {} while(0)
#endif

#if (_LWS_ENABLED_LOGS & LLL_PARSER)
#define lwsl_wsi_parser(_w, ...) lwsl_wsi(_w, LLL_PARSER, __VA_ARGS__)
#else
#define lwsl_wsi_parser(_w, ...) do {} while(0)
#endif

#if (_LWS_ENABLED_LOGS & LLL_HEADER)
#define lwsl_wsi_header(_w, ...) lwsl_wsi(_w, LLL_HEADER, __VA_ARGS__)
#else
#define lwsl_wsi_header(_w, ...) do {} while(0)
#endif

#if (_LWS_ENABLED_LOGS & LLL_EXT)
#define lwsl_wsi_ext(_w, ...) lwsl_wsi(_w, LLL_EXT, __VA_ARGS__)
#else
#define lwsl_wsi_ext(_w, ...) do {} while(0)
#endif

#if (_LWS_ENABLED_LOGS & LLL_CLIENT)
#define lwsl_wsi_client(_w, ...) lwsl_wsi(_w, LLL_CLIENT, __VA_ARGS__)
#else
#define lwsl_wsi_client(_w, ...) do {} while(0)
#endif

#if (_LWS_ENABLED_LOGS & LLL_LATENCY)
#define lwsl_wsi_latency(_w, ...) lwsl_wsi(_w, LLL_LATENCY, __VA_ARGS__)
#else
#define lwsl_wsi_latency(_w, ...) do {} while(0)
#endif

#if (_LWS_ENABLED_LOGS & LLL_THREAD)
#define lwsl_wsi_thread(_w, ...) lwsl_wsi(_w, LLL_THREAD, __VA_ARGS__)
#else
#define lwsl_wsi_thread(_w, ...) do {} while(0)
#endif

#if (_LWS_ENABLED_LOGS & LLL_USER)
#define lwsl_wsi_user(_w, ...) lwsl_wsi(_w, LLL_USER, __VA_ARGS__)
#else
#define lwsl_wsi_user(_w, ...) do {} while(0)
#endif

#define lwsl_hexdump_wsi_err(_v, ...)    lwsl_hexdump_wsi(_v, LLL_ERR, __VA_ARGS__)
#define lwsl_hexdump_wsi_warn(_v, ...)   lwsl_hexdump_wsi(_v, LLL_WARN, __VA_ARGS__)
#define lwsl_hexdump_wsi_notice(_v, ...) lwsl_hexdump_wsi(_v, LLL_NOTICE, __VA_ARGS__)
#define lwsl_hexdump_wsi_info(_v, ...)   lwsl_hexdump_wsi(_v, LLL_INFO, __VA_ARGS__)
#define lwsl_hexdump_wsi_debug(_v, ...)  lwsl_hexdump_wsi(_v, LLL_DEBUG, __VA_ARGS__)


/*
 * lwsl_ss
 */

#if (_LWS_ENABLED_LOGS & LLL_ERR)
#define lwsl_ss_err(_w, ...) lwsl_ss(_w, LLL_ERR, __VA_ARGS__)
#else
#define lwsl_ss_err(_w, ...) do {} while(0)
#endif

#if (_LWS_ENABLED_LOGS & LLL_WARN)
#define lwsl_ss_warn(_w, ...) lwsl_ss(_w, LLL_WARN, __VA_ARGS__)
#else
#define lwsl_ss_warn(_w, ...) do {} while(0)
#endif

#if (_LWS_ENABLED_LOGS & LLL_NOTICE)
#define lwsl_ss_notice(_w, ...) lwsl_ss(_w, LLL_NOTICE, __VA_ARGS__)
#else
#define lwsl_ss_notice(_w, ...) do {} while(0)
#endif

#if (_LWS_ENABLED_LOGS & LLL_INFO)
#define lwsl_ss_info(_w, ...) lwsl_ss(_w, LLL_INFO, __VA_ARGS__)
#else
#define lwsl_ss_info(_w, ...) do {} while(0)
#endif

#if (_LWS_ENABLED_LOGS & LLL_DEBUG)
#define lwsl_ss_debug(_w, ...) lwsl_ss(_w, LLL_DEBUG, __VA_ARGS__)
#else
#define lwsl_ss_debug(_w, ...) do {} while(0)
#endif

#if (_LWS_ENABLED_LOGS & LLL_PARSER)
#define lwsl_ss_parser(_w, ...) lwsl_ss(_w, LLL_PARSER, __VA_ARGS__)
#else
#define lwsl_ss_parser(_w, ...) do {} while(0)
#endif

#if (_LWS_ENABLED_LOGS & LLL_HEADER)
#define lwsl_ss_header(_w, ...) lwsl_ss(_w, LLL_HEADER, __VA_ARGS__)
#else
#define lwsl_ss_header(_w, ...) do {} while(0)
#endif

#if (_LWS_ENABLED_LOGS & LLL_EXT)
#define lwsl_ss_ext(_w, ...) lwsl_ss(_w, LLL_EXT, __VA_ARGS__)
#else
#define lwsl_ss_ext(_w, ...) do {} while(0)
#endif

#if (_LWS_ENABLED_LOGS & LLL_CLIENT)
#define lwsl_ss_client(_w, ...) lwsl_ss(_w, LLL_CLIENT, __VA_ARGS__)
#else
#define lwsl_ss_client(_w, ...) do {} while(0)
#endif

#if (_LWS_ENABLED_LOGS & LLL_LATENCY)
#define lwsl_ss_latency(_w, ...) lwsl_ss(_w, LLL_LATENCY, __VA_ARGS__)
#else
#define lwsl_ss_latency(_w, ...) do {} while(0)
#endif

#if (_LWS_ENABLED_LOGS & LLL_THREAD)
#define lwsl_ss_thread(_w, ...) lwsl_ss(_w, LLL_THREAD, __VA_ARGS__)
#else
#define lwsl_ss_thread(_w, ...) do {} while(0)
#endif

#if (_LWS_ENABLED_LOGS & LLL_USER)
#define lwsl_ss_user(_w, ...) lwsl_ss(_w, LLL_USER, __VA_ARGS__)
#else
#define lwsl_ss_user(_w, ...) do {} while(0)
#endif

#define lwsl_hexdump_ss_err(_v, ...)    lwsl_hexdump_ss(_v, LLL_ERR, __VA_ARGS__)
#define lwsl_hexdump_ss_warn(_v, ...)   lwsl_hexdump_ss(_v, LLL_WARN, __VA_ARGS__)
#define lwsl_hexdump_ss_notice(_v, ...) lwsl_hexdump_ss(_v, LLL_NOTICE, __VA_ARGS__)
#define lwsl_hexdump_ss_info(_v, ...)   lwsl_hexdump_ss(_v, LLL_INFO, __VA_ARGS__)
#define lwsl_hexdump_ss_debug(_v, ...)  lwsl_hexdump_ss(_v, LLL_DEBUG, __VA_ARGS__)



/**
 * lwsl_hexdump_level() - helper to hexdump a buffer at a selected debug level
 *
 * \param level: one of LLL_ constants
 * \param vbuf: buffer start to dump
 * \param len: length of buffer to dump
 *
 * If \p level is visible, does a nice hexdump -C style dump of \p vbuf for
 * \p len bytes.  This can be extremely convenient while debugging.
 */
LWS_VISIBLE LWS_EXTERN void
lwsl_hexdump_level(int level, const void *vbuf, size_t len);

LWS_VISIBLE LWS_EXTERN void
lwsl_hexdump_level_cx(lws_log_cx_t *cx, lws_log_prepend_cx_t prep, void *obj,
		      int hexdump_level, const void *vbuf, size_t len);

/**
 * lwsl_hexdump() - helper to hexdump a buffer (DEBUG builds only)
 *
 * \param buf: buffer start to dump
 * \param len: length of buffer to dump
 *
 * Calls through to lwsl_hexdump_level(LLL_DEBUG, ... for compatability.
 * It's better to use lwsl_hexdump_level(level, ... directly so you can control
 * the visibility.
 */
LWS_VISIBLE LWS_EXTERN void
lwsl_hexdump(const void *buf, size_t len);

/**
 * lws_is_be() - returns nonzero if the platform is Big Endian
 */
static LWS_INLINE int lws_is_be(void) {
	const int probe = ~0xff;

	return *(const char *)&probe;
}

/**
 * lws_set_log_level() - Set the logging bitfield
 * \param level:	OR together the LLL_ debug contexts you want output from
 * \param log_emit_function:	NULL to leave it as it is, or a user-supplied
 *			function to perform log string emission instead of
 *			the default stderr one.
 *
 * log level defaults to "err", "warn" and "notice" contexts enabled and
 * emission on stderr.  If stderr is a tty (according to isatty()) then
 * the output is coloured according to the log level using ANSI escapes.
 *
 * You can set the default security level for logging using the
 * secrecy_and_log_level() macro to set the \p level parameter, eg
 *
 * lws_set_log_level(secrecy_and_log_level(LWS_SECRECY_PII, LLL_ERR | LLL_WARN),
 *		     my_emit_function);
 *
 * Normally you can just leave it at the default.
 */
LWS_VISIBLE LWS_EXTERN void
lws_set_log_level(int level, lws_log_emit_t log_emit_function);

/**
 * lwsl_emit_syslog() - helper log emit function writes to system log
 *
 * \param level: one of LLL_ log level indexes
 * \param line: log string
 *
 * You use this by passing the function pointer to lws_set_log_level(), to set
 * it as the log emit function, it is not called directly.
 */
LWS_VISIBLE LWS_EXTERN void
lwsl_emit_syslog(int level, const char *line);

/**
 * lwsl_emit_stderr() - helper log emit function writes to stderr
 *
 * \param level: one of LLL_ log level indexes
 * \param line: log string
 *
 * You use this by passing the function pointer to lws_set_log_level(), to set
 * it as the log emit function, it is not called directly.
 *
 * It prepends a system timestamp like [2018/11/13 07:41:57:3989]
 *
 * If stderr is a tty, then ansi colour codes are added.
 */
LWS_VISIBLE LWS_EXTERN void
lwsl_emit_stderr(int level, const char *line);

/**
 * lwsl_emit_stderr_notimestamp() - helper log emit function writes to stderr
 *
 * \param level: one of LLL_ log level indexes
 * \param line: log string
 *
 * You use this by passing the function pointer to lws_set_log_level(), to set
 * it as the log emit function, it is not called directly.
 *
 * If stderr is a tty, then ansi colour codes are added.
 */
LWS_VISIBLE LWS_EXTERN void
lwsl_emit_stderr_notimestamp(int level, const char *line);

/**
 * lwsl_visible() - returns true if the log level should be printed
 *
 * \param level: one of LLL_ log level indexes
 *
 * This is useful if you have to do work to generate the log content, you
 * can skip the work if the log level used to print it is not actually
 * enabled at runtime.
 */
LWS_VISIBLE LWS_EXTERN int
lwsl_visible(int level);

struct lws;

LWS_VISIBLE LWS_EXTERN const char *
lws_wsi_tag(struct lws *wsi);

LWS_VISIBLE LWS_EXTERN void
lwsl_refcount_cx(lws_log_cx_t *cx, int _new);

///@}
