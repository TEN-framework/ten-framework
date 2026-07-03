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

#include "private-lib-core.h"

#if !defined(SOL_TCP) && defined(IPPROTO_TCP)
#define SOL_TCP IPPROTO_TCP
#endif

const char * const method_names[] = {
	"GET", "POST",
#if defined(LWS_WITH_HTTP_UNCOMMON_HEADERS)
	"OPTIONS", "PUT", "PATCH", "DELETE",
#endif
	"CONNECT", "HEAD",
#ifdef LWS_WITH_HTTP2
	":path",
#endif
	};

#if defined(LWS_WITH_FILE_OPS)
static const char * const intermediates[] = { "private", "public" };
#endif

/*
 * return 0: all done
 *        1: nonfatal error
 *       <0: fatal error
 *
 *       REQUIRES CONTEXT LOCK HELD
 */

#if defined(LWS_WITH_SERVER)

struct vh_sock_args {
	const struct lws_context_creation_info	*info;
	struct lws_vhost			*vhost;
	int					af;
};


static int
check_extant(struct lws_dll2 *d, void *user)
{
	struct lws *wsi = lws_container_of(d, struct lws, listen_list);
	struct vh_sock_args *a = (struct vh_sock_args *)user;

	if (!lws_vhost_compare_listen(wsi->a.vhost, a->vhost))
		return 0;

	if (wsi->af != a ->af)
		return 0;

	lwsl_notice(" using listen skt from vhost %s\n", wsi->a.vhost->name);

	return 1;
}

/*
 * Creates a single listen socket of a specific AF
 */

int
_lws_vhost_init_server_af(struct vh_sock_args *a)
{
	struct lws_context *cx = a->vhost->context;
	struct lws_context_per_thread *pt;
	int n, opt = 1, limit = 1;
	lws_sockfd_type sockfd;
	struct lws *wsi;
	int m = 0, is;
#if defined(LWS_WITH_IPV6)
	int value = 1;
#endif

	(void)method_names;
	(void)opt;

	lwsl_info("%s: af %d\n", __func__, (int)a->af);

	if (lws_vhost_foreach_listen_wsi(a->vhost->context, a, check_extant))
		return 0;

deal:

	if (a->vhost->iface) {

		/*
		 * let's check before we do anything else about the disposition
		 * of the interface he wants to bind to...
		 */
		is = lws_socket_bind(a->vhost, NULL, LWS_SOCK_INVALID,
				     a->vhost->listen_port, a->vhost->iface,
				     a->af);
		lwsl_debug("initial if check says %d\n", is);

		if (is == LWS_ITOSA_BUSY)
			/* treat as fatal */
			return -1;

		lws_start_foreach_llp(struct lws_vhost **, pv,
				      cx->no_listener_vhost_list) {
			if (is >= LWS_ITOSA_USABLE && *pv == a->vhost) {
				/* on the list and shouldn't be: remove it */
				lwsl_debug("deferred iface: removing vh %s\n",
						(*pv)->name);
				*pv = a->vhost->no_listener_vhost_list;
				a->vhost->no_listener_vhost_list = NULL;
				goto done_list;
			}
			if (is < LWS_ITOSA_USABLE && *pv == a->vhost)
				goto done_list;
		} lws_end_foreach_llp(pv, no_listener_vhost_list);

		/* not on the list... */

		if (is < LWS_ITOSA_USABLE) {

			/* ... but needs to be: so add it */

			lwsl_debug("deferred iface: adding vh %s\n",
					a->vhost->name);
			a->vhost->no_listener_vhost_list =
					cx->no_listener_vhost_list;
			cx->no_listener_vhost_list = a->vhost;
		}

done_list:

		switch (is) {
		default:
			break;
		case LWS_ITOSA_NOT_EXIST:
			/* can't add it */
			if (!a->info)
				return -1;

			/* first time */
			lwsl_err("%s: VH %s: iface %s port %d DOESN'T EXIST\n",
				 __func__, a->vhost->name, a->vhost->iface,
				 a->vhost->listen_port);

			return (a->info->options &
				LWS_SERVER_OPTION_FAIL_UPON_UNABLE_TO_BIND) ==
				LWS_SERVER_OPTION_FAIL_UPON_UNABLE_TO_BIND ?
				-1 : 1;

		case LWS_ITOSA_NOT_USABLE:
			/* can't add it */
			if (!a->info) /* first time */
				return -1;

			lwsl_err("%s: VH %s: iface %s port %d NOT USABLE\n",
				 __func__, a->vhost->name, a->vhost->iface,
				 a->vhost->listen_port);

			return (a->info->options &
				LWS_SERVER_OPTION_FAIL_UPON_UNABLE_TO_BIND) ==
				LWS_SERVER_OPTION_FAIL_UPON_UNABLE_TO_BIND ?
				-1 : 1;
		}
	}

	(void)n;
#if defined(__linux__)
	/*
	 * A Unix domain sockets cannot be bound multiple times, even if we
	 * set the SO_REUSE* options on.
	 *
	 * However on recent linux, each thread is able to independently listen.
	 *
	 * So we can assume creating just one listening socket for a multi-
	 * threaded environment will typically work.
	 */
	if (a->af != AF_UNIX)
		limit = cx->count_threads;
#endif

	for (m = 0; m < limit; m++) {

		sockfd = lws_fi(&a->vhost->fic, "listenskt") ?
					LWS_SOCK_INVALID :
					socket(a->af, SOCK_STREAM, 0);

		if (sockfd == LWS_SOCK_INVALID) {
			lwsl_err("ERROR opening socket\n");
			return 1;
		}

#if !defined(LWS_PLAT_FREERTOS)
#if (defined(WIN32) || defined(_WIN32)) && defined(SO_EXCLUSIVEADDRUSE)
		/*
		 * only accept that we are the only listener on the port
		 * https://msdn.microsoft.com/zh-tw/library/
		 *    windows/desktop/ms740621(v=vs.85).aspx
		 *
		 * for lws, to match Linux, we default to exclusive listen
		 */
		if (!lws_check_opt(a->vhost->options,
				LWS_SERVER_OPTION_ALLOW_LISTEN_SHARE)) {
			if (setsockopt(sockfd, SOL_SOCKET, SO_EXCLUSIVEADDRUSE,
				       (const void *)&opt, sizeof(opt)) < 0) {
				lwsl_err("reuseaddr failed\n");
				compatible_close(sockfd);
				return -1;
			}
		} else
#endif

		/*
		 * allow us to restart even if old sockets in TIME_WAIT
		 */
		if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR,
			       (const void *)&opt, sizeof(opt)) < 0) {
			lwsl_err("reuseaddr failed\n");
			compatible_close(sockfd);
			return -1;
		}

#if defined(LWS_WITH_IPV6) && defined(IPV6_V6ONLY)
		/*
		 * If we have an ipv6 listen socket, it only accepts ipv6.
		 *
		 * There will be a separate ipv4 listen socket if that's
		 * enabled.
		 */
		if (a->af == AF_INET6 &&
		    setsockopt(sockfd, IPPROTO_IPV6, IPV6_V6ONLY,
			       (const void*)&value, sizeof(value)) < 0) {
			compatible_close(sockfd);
			return -1;
		}
#endif

#if defined(__linux__) && defined(SO_REUSEPORT)
		/* keep coverity happy */
#if LWS_MAX_SMP > 1
		n = 1;
#else
		n = lws_check_opt(a->vhost->options,
				  LWS_SERVER_OPTION_ALLOW_LISTEN_SHARE);
#endif
		if (n || cx->count_threads > 1) /* ... also implied by threads > 1 */
			if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT,
					(const void *)&opt, sizeof(opt)) < 0) {
				compatible_close(sockfd);
				return -1;
			}
#endif
#endif
		lws_plat_set_socket_options(a->vhost, sockfd, 0);

		is = lws_socket_bind(a->vhost, NULL, sockfd,
				     a->vhost->listen_port,
				     a->vhost->iface, a->af);

		if (is == LWS_ITOSA_BUSY) {
			/* treat as fatal */
			compatible_close(sockfd);

			return -1;
		}

		/*
		 * There is a race where the network device may come up and then
		 * go away and fail here.  So correctly handle unexpected failure
		 * here despite we earlier confirmed it.
		 */
		if (is < 0) {
			lwsl_info("%s: lws_socket_bind says %d\n", __func__, is);
			compatible_close(sockfd);
			if (a->vhost->iface)
				goto deal;
			return -1;
		}

		/*
		 * Create the listen wsi and customize it
		 */

		lws_context_lock(cx, __func__);
		wsi = __lws_wsi_create_with_role(cx, m, &role_ops_listen, NULL);
		lws_context_unlock(cx);
		if (wsi == NULL) {
			lwsl_err("Out of mem\n");
			goto bail;
		}

		wsi->af = (uint8_t)a->af;

#ifdef LWS_WITH_UNIX_SOCK
		if (!LWS_UNIX_SOCK_ENABLED(a->vhost))
#endif
		{
			wsi->unix_skt = 1;
			a->vhost->listen_port = is;

			lwsl_debug("%s: lws_socket_bind says %d\n", __func__, is);
		}

		wsi->desc.sockfd = sockfd;
		wsi->a.protocol = a->vhost->protocols;
		lws_vhost_bind_wsi(a->vhost, wsi);
		wsi->listener = 1;

		if (wsi->a.context->event_loop_ops->init_vhost_listen_wsi)
			wsi->a.context->event_loop_ops->init_vhost_listen_wsi(wsi);

		pt = &cx->pt[m];
		lws_pt_lock(pt, __func__);

		if (__insert_wsi_socket_into_fds(cx, wsi)) {
			lwsl_notice("inserting wsi socket into fds failed\n");
			lws_pt_unlock(pt);
			goto bail;
		}

		lws_dll2_add_tail(&wsi->listen_list, &a->vhost->listen_wsi);
		lws_pt_unlock(pt);

#if defined(WIN32) && defined(TCP_FASTOPEN)
		if (a->vhost->fo_listen_queue) {
			int optval = 1;
			if (setsockopt(wsi->desc.sockfd, IPPROTO_TCP,
				       TCP_FASTOPEN,
				       (const char*)&optval, sizeof(optval)) < 0) {
				int error = LWS_ERRNO;
				lwsl_warn("%s: TCP_NODELAY failed with error %d\n",
						__func__, error);
			}
		}
#else
#if defined(TCP_FASTOPEN)
		if (a->vhost->fo_listen_queue) {
			int qlen = a->vhost->fo_listen_queue;

			if (setsockopt(wsi->desc.sockfd, SOL_TCP, TCP_FASTOPEN,
				       &qlen, sizeof(qlen)))
				lwsl_warn("%s: TCP_FASTOPEN failed\n", __func__);
		}
#endif
#endif

		n = listen(wsi->desc.sockfd, LWS_SOMAXCONN);
		if (n < 0) {
			lwsl_err("listen failed with error %d\n", LWS_ERRNO);
			lws_dll2_remove(&wsi->listen_list);
			__remove_wsi_socket_from_fds(wsi);
			goto bail;
		}

		if (wsi)
			__lws_lc_tag(a->vhost->context,
				     &a->vhost->context->lcg[LWSLCG_WSI],
				     &wsi->lc, "listen|%s|%s|%d",
				     a->vhost->name,
				     a->vhost->iface ? a->vhost->iface : "",
				     (int)a->vhost->listen_port);

	} /* for each thread able to independently listen */

	if (!lws_check_opt(cx->options, LWS_SERVER_OPTION_EXPLICIT_VHOSTS)) {
#ifdef LWS_WITH_UNIX_SOCK
		if (a->af == AF_UNIX)
			lwsl_info(" Listening on \"%s\"\n", a->vhost->iface);
		else
#endif
			lwsl_info(" Listening on %s:%d\n",
					a->vhost->iface,
					a->vhost->listen_port);
        }

	// info->port = vhost->listen_port;

	return 0;

bail:
	compatible_close(sockfd);

	return -1;
}


int
_lws_vhost_init_server(const struct lws_context_creation_info *info,
		       struct lws_vhost *vhost)
{
	struct vh_sock_args a;

	a.info = info;
	a.vhost = vhost;

	if (info) {
		vhost->iface = info->iface;
		vhost->listen_port = info->port;
	}

	/* set up our external listening socket we serve on */

	if (vhost->listen_port == CONTEXT_PORT_NO_LISTEN ||
	    vhost->listen_port == CONTEXT_PORT_NO_LISTEN_SERVER)
		return 0;

	/*
	 * Let's figure out what AF(s) we want this vhost to listen on.
	 *
	 * We want AF_UNIX alone if that's what's told
	 */

#if defined(LWS_WITH_UNIX_SOCK)
	/*
	 * If unix socket, ask for that and we are done
	 */
	if (LWS_UNIX_SOCK_ENABLED(vhost)) {
		a.af = AF_UNIX;
		goto single;
	}
#endif

	/*
	 * We may support both ipv4 and ipv6, but get a numeric vhost listen
	 * iface that is unambiguously ipv4 or ipv6, meaning we can only listen
	 * for the related AF then.
	 */

	if (vhost->iface) {
		uint8_t buf[16];
		int q;

		q = lws_parse_numeric_address(vhost->iface, buf, sizeof(buf));

		if (q == 4) {
			a.af = AF_INET;
			goto single;
		}

		if (q == 16) {
#if defined(LWS_WITH_IPV6)
			if (LWS_IPV6_ENABLED(vhost)) {
				a.af = AF_INET6;
				goto single;
			}
#endif
			lwsl_err("%s: ipv6 not supported on %s\n", __func__,
					vhost->name);
			return 1;
		}
	}

	/*
	 * ... if we make it here, we would want to listen on AF_INET and
	 * AF_INET6 unless one or the other is forbidden
	 */

    int init_af = 0;
#if defined(LWS_WITH_IPV6)
	if (!(LWS_IPV6_ENABLED(vhost) &&
	      (vhost->options & LWS_SERVER_OPTION_IPV6_V6ONLY_MODIFY) &&
	      (vhost->options & LWS_SERVER_OPTION_IPV6_V6ONLY_VALUE))) {
#endif
		a.af = AF_INET;
		init_af = _lws_vhost_init_server_af(&a);
		if (init_af > 0)
			return 1;

#if defined(LWS_WITH_IPV6)
	}
	if (LWS_IPV6_ENABLED(vhost)) {
		a.af = AF_INET6;
		goto single;
	}
#endif

	return init_af;

single:
	return _lws_vhost_init_server_af(&a);
}

#endif

struct lws_vhost *
lws_select_vhost(struct lws_context *context, int port, const char *servername)
{
	struct lws_vhost *vhost = context->vhost_list;
	const char *p;
	int n, colon;

	n = (int)strlen(servername);
	colon = n;
	p = strchr(servername, ':');
	if (p)
		colon = lws_ptr_diff(p, servername);

	/* Priotity 1: first try exact matches */

	while (vhost) {
		if (port == vhost->listen_port &&
		    !strncmp(vhost->name, servername, (unsigned int)colon)) {
			lwsl_info("SNI: Found: %s\n", servername);
			return vhost;
		}
		vhost = vhost->vhost_next;
	}

	/*
	 * Priority 2: if no exact matches, try matching *.vhost-name
	 * unintentional matches are possible but resolve to x.com for *.x.com
	 * which is reasonable.  If exact match exists we already chose it and
	 * never reach here.  SSL will still fail it if the cert doesn't allow
	 * *.x.com.
	 */
	vhost = context->vhost_list;
	while (vhost) {
		int m = (int)strlen(vhost->name);
		if (port && port == vhost->listen_port &&
		    m <= (colon - 2) &&
		    servername[colon - m - 1] == '.' &&
		    !strncmp(vhost->name, servername + colon - m, (unsigned int)m)) {
			lwsl_info("SNI: Found %s on wildcard: %s\n",
				    servername, vhost->name);
			return vhost;
		}
		vhost = vhost->vhost_next;
	}

	/* Priority 3: match the first vhost on our port */

	vhost = context->vhost_list;
	while (vhost) {
		if (port && port == vhost->listen_port) {
			lwsl_info("%s: vhost match to %s based on port %d\n",
					__func__, vhost->name, port);
			return vhost;
		}
		vhost = vhost->vhost_next;
	}

	/* no match */

	return NULL;
}

static const struct lws_mimetype {
	const char *extension;
	const char *mimetype;
} server_mimetypes[] = {
	{ ".html", "text/html" },
	{ ".htm", "text/html" },
	{ ".js", "text/javascript" },
	{ ".css", "text/css" },
	{ ".png", "image/png" },
	{ ".jpg", "image/jpeg" },
	{ ".jpeg", "image/jpeg" },
	{ ".ico", "image/x-icon" },
	{ ".gif", "image/gif" },
	{ ".svg", "image/svg+xml" },
	{ ".ttf", "application/x-font-ttf" },
	{ ".otf", "application/font-woff" },
	{ ".woff", "application/font-woff" },
	{ ".woff2", "application/font-woff2" },
	{ ".gz", "application/gzip" },
	{ ".txt", "text/plain" },
	{ ".xml", "application/xml" },
	{ ".json", "application/json" },
	{ ".mjs", "text/javascript" },
};

const char *
lws_get_mimetype(const char *file, const struct lws_http_mount *m)
{
	const struct lws_protocol_vhost_options *pvo;
	size_t n = strlen(file), len, i;
	const char *fallback_mimetype = NULL;
	const struct lws_mimetype *mt;

	/* prioritize user-defined mimetypes */
	for (pvo = m ? m->extra_mimetypes : NULL; pvo; pvo = pvo->next) {
		/* ie, match anything */
		if (!fallback_mimetype && pvo->name[0] == '*') {
			fallback_mimetype = pvo->value;
			continue;
		}

		len = strlen(pvo->name);
		if (n > len && !strcasecmp(&file[n - len], pvo->name)) {
			lwsl_info("%s: match to user mimetype: %s\n", __func__,
				  pvo->value);
			return pvo->value;
		}
	}

	/* fallback to server-defined mimetypes */
	for (i = 0; i < LWS_ARRAY_SIZE(server_mimetypes); ++i) {
		mt = &server_mimetypes[i];

		len = strlen(mt->extension);
		if (n > len && !strcasecmp(&file[n - len], mt->extension)) {
			lwsl_info("%s: match to server mimetype: %s\n", __func__,
				  mt->mimetype);
			return mt->mimetype;
		}
	}

	/* fallback to '*' if defined */
	if (fallback_mimetype) {
		lwsl_info("%s: match to any mimetype: %s\n", __func__,
			  fallback_mimetype);
		return fallback_mimetype;
	}

	return NULL;
}

#if defined(LWS_WITH_FILE_OPS)
static lws_fop_flags_t
lws_vfs_prepare_flags(struct lws *wsi)
{
	lws_fop_flags_t f = 0;

	if (!lws_hdr_total_length(wsi, WSI_TOKEN_HTTP_ACCEPT_ENCODING))
		return f;

	if (strstr(lws_hdr_simple_ptr(wsi, WSI_TOKEN_HTTP_ACCEPT_ENCODING),
		   "gzip")) {
		lwsl_info("client indicates GZIP is acceptable\n");
		f |= LWS_FOP_FLAG_COMPR_ACCEPTABLE_GZIP;
	}

	return f;
}

static int
lws_http_serve(struct lws *wsi, char *uri, const char *origin,
	       const struct lws_http_mount *m)
{
	const struct lws_protocol_vhost_options *pvo = m->interpret;
	struct lws_process_html_args args;
	const char *mimetype;
#if !defined(_WIN32_WCE)
	const struct lws_plat_file_ops *fops;
	const char *vpath;
	lws_fop_flags_t fflags = LWS_O_RDONLY;
#if defined(WIN32) && defined(LWS_HAVE__STAT32I64)
	struct _stat32i64 st;
#else
	struct stat st;
#endif
	int spin = 0;
#endif
	char path[256], sym[2048];
	unsigned char *p = (unsigned char *)sym + 32 + LWS_PRE, *start = p;
	unsigned char *end = p + sizeof(sym) - 32 - LWS_PRE;
#if !defined(WIN32) && !defined(LWS_PLAT_FREERTOS)
	size_t len;
#endif
	int n;

	wsi->handling_404 = 0;
	if (!wsi->a.vhost)
		return -1;

#if defined(LWS_ROLE_H1) || defined(LWS_ROLE_H2)
	if (wsi->a.vhost->http.error_document_404 &&
	    !strcmp(uri, wsi->a.vhost->http.error_document_404))
		wsi->handling_404 = 1;
#endif

	lws_snprintf(path, sizeof(path) - 1, "%s/%s", origin, uri);

#if !defined(_WIN32_WCE)

	fflags |= lws_vfs_prepare_flags(wsi);

	do {
		spin++;
		fops = lws_vfs_select_fops(wsi->a.context->fops, path, &vpath);

		if (wsi->http.fop_fd)
			lws_vfs_file_close(&wsi->http.fop_fd);

		wsi->http.fop_fd = fops->LWS_FOP_OPEN(wsi->a.context->fops,
							path, vpath, &fflags);
		if (!wsi->http.fop_fd) {
			lwsl_info("%s: Unable to open '%s': errno %d\n",
				  __func__, path, errno);

			return 1;
		}

		/* if it can't be statted, don't try */
		if (fflags & LWS_FOP_FLAG_VIRTUAL)
			break;
#if defined(LWS_PLAT_FREERTOS)
		break;
#endif
#if !defined(WIN32)
		if (fstat(wsi->http.fop_fd->fd, &st)) {
			lwsl_info("unable to stat %s\n", path);
			goto notfound;
		}
#else
#if defined(LWS_HAVE__STAT32I64)
		{
			WCHAR buf[MAX_PATH];
			MultiByteToWideChar(CP_UTF8, 0, path, -1, buf, LWS_ARRAY_SIZE(buf));
			if (_wstat32i64(buf, &st)) {
				lwsl_info("unable to stat %s\n", path);
				goto notfound;
			}
		}
#else
		if (stat(path, &st)) {
			lwsl_info("unable to stat %s\n", path);
			goto notfound;
		}
#endif
#endif

		wsi->http.fop_fd->mod_time = (uint32_t)st.st_mtime;
		fflags |= LWS_FOP_FLAG_MOD_TIME_VALID;

#if !defined(WIN32) && !defined(LWS_PLAT_FREERTOS)
		if ((S_IFMT & st.st_mode) == S_IFLNK) {
			len = (size_t)readlink(path, sym, sizeof(sym) - 1);
			if (len) {
				lwsl_err("Failed to read link %s\n", path);
				goto notfound;
			}
			sym[len] = '\0';
			lwsl_debug("symlink %s -> %s\n", path, sym);
			lws_snprintf(path, sizeof(path) - 1, "%s", sym);
		}
#endif
		if ((S_IFMT & st.st_mode) == S_IFDIR) {
			lwsl_debug("default filename append to dir\n");
			lws_snprintf(path, sizeof(path) - 1, "%s/%s/%s",
				 origin, uri, m->def ? m->def : "index.html");
		}

	} while ((S_IFMT & st.st_mode) != S_IFREG && spin < 5);

	if (spin == 5)
		lwsl_err("symlink loop %s \n", path);

	n = sprintf(sym, "%08llX%08lX",
		    (unsigned long long)lws_vfs_get_length(wsi->http.fop_fd),
		    (unsigned long)lws_vfs_get_mod_time(wsi->http.fop_fd));

	/* disable ranges if IF_RANGE token invalid */

	if (lws_hdr_total_length(wsi, WSI_TOKEN_HTTP_IF_RANGE))
		if (strcmp(sym, lws_hdr_simple_ptr(wsi, WSI_TOKEN_HTTP_IF_RANGE)))
			/* differs - defeat Range: */
			wsi->http.ah->frag_index[WSI_TOKEN_HTTP_RANGE] = 0;

	if (lws_hdr_total_length(wsi, WSI_TOKEN_HTTP_IF_NONE_MATCH)) {
		/*
		 * he thinks he has some version of it already,
		 * check if the tag matches
		 */
		if (!strcmp(sym, lws_hdr_simple_ptr(wsi,
					WSI_TOKEN_HTTP_IF_NONE_MATCH))) {

			char cache_control[50], *cc = "no-store";
			int cclen = 8;

			lwsl_debug("%s: ETAG match %s %s\n", __func__,
				   uri, origin);

			/* we don't need to send the payload */
			if (lws_add_http_header_status(wsi,
					HTTP_STATUS_NOT_MODIFIED, &p, end)) {
				lwsl_err("%s: failed adding not modified\n",
						__func__);
				return -1;
			}

			if (lws_add_http_header_by_token(wsi,
					WSI_TOKEN_HTTP_ETAG,
					(unsigned char *)sym, n, &p, end))
				return -1;

			/* but we still need to send cache control... */

			if (m->cache_max_age && m->cache_reusable) {
				if (!m->cache_revalidate) {
					cc = cache_control;
					cclen = sprintf(cache_control,
						"%s, max-age=%u",
						intermediates[wsi->cache_intermediaries],
						m->cache_max_age);
				} else {
					cc = cache_control;
                                        cclen = sprintf(cache_control,
                                        	"must-revalidate, %s, max-age=%u",
                                                intermediates[wsi->cache_intermediaries],
                                                m->cache_max_age);
				}
			}

			if (lws_add_http_header_by_token(wsi,
					WSI_TOKEN_HTTP_CACHE_CONTROL,
					(unsigned char *)cc, cclen, &p, end))
				return -1;

			if (lws_finalize_http_header(wsi, &p, end))
				return -1;

			n = lws_write(wsi, start, lws_ptr_diff_size_t(p, start),
				      LWS_WRITE_HTTP_HEADERS |
				      LWS_WRITE_H2_STREAM_END);
			if (n != lws_ptr_diff(p, start)) {
				lwsl_err("_write returned %d from %ld\n", n,
					 (long)(p - start));
				return -1;
			}

			lws_vfs_file_close(&wsi->http.fop_fd);

			if (lws_http_transaction_completed(wsi))
				return -1;

			return 0;
		}
	}

	if (lws_add_http_header_by_token(wsi, WSI_TOKEN_HTTP_ETAG,
			(unsigned char *)sym, n, &p, end))
		return -1;
#endif

	mimetype = lws_get_mimetype(path, m);
	if (!mimetype) {
		lwsl_info("unknown mimetype for %s\n", path);
		if (lws_return_http_status(wsi,
				HTTP_STATUS_UNSUPPORTED_MEDIA_TYPE, NULL) ||
		    lws_http_transaction_completed(wsi))
			return -1;

		return 0;
	}
	if (!mimetype[0])
		lwsl_debug("sending no mimetype for %s\n", path);

	wsi->sending_chunked = 0;
	wsi->interpreting = 0;

	/*
	 * check if this is in the list of file suffixes to be interpreted by
	 * a protocol
	 */
	while (pvo) {
		n = (int)strlen(path);
		if (n > (int)strlen(pvo->name) &&
		    !strcmp(&path[(unsigned int)n - strlen(pvo->name)], pvo->name)) {
			wsi->interpreting = 1;
			if (!wsi->mux_substream)
				wsi->sending_chunked = 1;

			wsi->protocol_interpret_idx = (char)(
				lws_vhost_name_to_protocol(wsi->a.vhost,
							   pvo->value) -
				&lws_get_vhost(wsi)->protocols[0]);

			lwsl_debug("want %s interpreted by %s (pcol is %s)\n", path,
				    wsi->a.vhost->protocols[
				             (int)wsi->protocol_interpret_idx].name,
				             wsi->a.protocol->name);
			if (lws_bind_protocol(wsi, &wsi->a.vhost->protocols[
			          (int)wsi->protocol_interpret_idx], __func__))
				return -1;

			if (lws_ensure_user_space(wsi))
				return -1;
			break;
		}
		pvo = pvo->next;
	}

	if (wsi->sending_chunked) {
		if (lws_add_http_header_by_token(wsi,
				WSI_TOKEN_HTTP_TRANSFER_ENCODING,
				(unsigned char *)"chunked", 7,
				&p, end))
			return -1;
	}

	if (m->protocol) {
		const struct lws_protocols *pp = lws_vhost_name_to_protocol(
						       wsi->a.vhost, m->protocol);

		if (lws_bind_protocol(wsi, pp, __func__))
			return -1;
		args.p = (char *)p;
		args.max_len = lws_ptr_diff(end, p);
		if (pp->callback(wsi, LWS_CALLBACK_ADD_HEADERS,
					  wsi->user_space, &args, 0))
			return -1;
		p = (unsigned char *)args.p;
	}

	*p = '\0';
	n = lws_serve_http_file(wsi, path, mimetype, (char *)start,
				lws_ptr_diff(p, start));

	if (n < 0 || ((n > 0) && lws_http_transaction_completed(wsi)))
		return -1; /* error or can't reuse connection: close the socket */

	return 0;

notfound:

	return 1;
}
#endif

#if defined(LWS_ROLE_H1) || defined(LWS_ROLE_H2)
const struct lws_http_mount *
lws_find_mount(struct lws *wsi, const char *uri_ptr, int uri_len)
{
	const struct lws_http_mount *hm, *hit = NULL;
	int best = 0;

	hm = wsi->a.vhost->http.mount_list;
	while (hm) {
		if (uri_len >= hm->mountpoint_len &&
		    !strncmp(uri_ptr, hm->mountpoint, hm->mountpoint_len) &&
		    (uri_ptr[hm->mountpoint_len] == '\0' ||
		     uri_ptr[hm->mountpoint_len] == '/' ||
		     hm->mountpoint_len == 1)
		    ) {
#if defined(LWS_WITH_SYS_METRICS)
			lws_metrics_tag_wsi_add(wsi, "mnt", hm->mountpoint);
#endif

			if (hm->origin_protocol == LWSMPRO_CALLBACK ||
			    ((hm->origin_protocol == LWSMPRO_CGI ||
			     lws_hdr_total_length(wsi, WSI_TOKEN_GET_URI) ||
			     lws_hdr_total_length(wsi, WSI_TOKEN_POST_URI) ||
#if defined(LWS_WITH_HTTP_UNCOMMON_HEADERS)
			     lws_hdr_total_length(wsi, WSI_TOKEN_PUT_URI) ||
			     lws_hdr_total_length(wsi, WSI_TOKEN_PATCH_URI) ||
			     lws_hdr_total_length(wsi, WSI_TOKEN_DELETE_URI) ||
#endif
			     lws_hdr_total_length(wsi, WSI_TOKEN_HEAD_URI) ||
#if defined(LWS_ROLE_H2)
			     (wsi->mux_substream &&
				lws_hdr_total_length(wsi,
						WSI_TOKEN_HTTP_COLON_PATH)) ||
#endif
			     hm->protocol) &&
			    hm->mountpoint_len > best)) {
				best = hm->mountpoint_len;
				hit = hm;
			}
		}
		hm = hm->mount_next;
	}

	return hit;
}
#endif

#if defined(LWS_WITH_HTTP_BASIC_AUTH) && !defined(LWS_PLAT_FREERTOS) && defined(LWS_WITH_FILE_OPS)
static int
lws_find_string_in_file(const char *filename, const char *string, int stringlen)
{
	char buf[128];
	int fd, match = 0, pos = 0, n = 0, hit = 0;

	fd = lws_open(filename, O_RDONLY);
	if (fd < 0) {
		lwsl_err("can't open auth file: %s\n", filename);
		return 0;
	}

	while (1) {
		if (pos == n) {
			n = (int)read(fd, buf, sizeof(buf));
			if (n <= 0) {
				if (match == stringlen)
					hit = 1;
				break;
			}
			pos = 0;
		}

		if (match == stringlen) {
			if (buf[pos] == '\r' || buf[pos] == '\n') {
				hit = 1;
				break;
			}
			match = 0;
		}

		if (buf[pos] == string[match])
			match++;
		else
			match = 0;

		pos++;
	}

	close(fd);

	return hit;
}
#endif

#if defined(LWS_WITH_HTTP_BASIC_AUTH)

int
lws_unauthorised_basic_auth(struct lws *wsi)
{
	struct lws_context_per_thread *pt = &wsi->a.context->pt[(int)wsi->tsi];
	unsigned char *start = pt->serv_buf + LWS_PRE,
		      *p = start, *end = p + 2048;
	char buf[64];
	int n;

	/* no auth... tell him it is required */

	if (lws_add_http_header_status(wsi, HTTP_STATUS_UNAUTHORIZED, &p, end))
		return -1;

	n = lws_snprintf(buf, sizeof(buf), "Basic realm=\"lwsws\"");
	if (lws_add_http_header_by_token(wsi,
			WSI_TOKEN_HTTP_WWW_AUTHENTICATE,
			(unsigned char *)buf, n, &p, end))
		return -1;

	if (lws_add_http_header_content_length(wsi, 0, &p, end))
		return -1;

	if (lws_finalize_http_header(wsi, &p, end))
		return -1;

	n = lws_write(wsi, start, lws_ptr_diff_size_t(p, start), LWS_WRITE_HTTP_HEADERS |
					     LWS_WRITE_H2_STREAM_END);
	if (n < 0)
		return -1;

	return lws_http_transaction_completed(wsi);

}

#endif

int lws_clean_url(char *p)
{
	if (p[0] == 'h' && p[1] == 't' && p[2] == 't' && p[3] == 'p') {
		p += 4;
		if (*p == 's')
		p++;
		if (*p == ':') {
			p++;
			if (*p == '/')
			p++;
		}
	}

	while (*p) {
		if (p[0] == '/' && p[1] == '/') {
			char *p1 = p;
			while (*p1) {
				*p1 = p1[1];
				p1++;
			}
			continue;
		}
		p++;
	}

	return 0;
}

static const unsigned char methods[] = {
	WSI_TOKEN_GET_URI,
	WSI_TOKEN_POST_URI,
#if defined(LWS_WITH_HTTP_UNCOMMON_HEADERS)
	WSI_TOKEN_OPTIONS_URI,
	WSI_TOKEN_PUT_URI,
	WSI_TOKEN_PATCH_URI,
	WSI_TOKEN_DELETE_URI,
#endif
	WSI_TOKEN_CONNECT,
	WSI_TOKEN_HEAD_URI,
#ifdef LWS_WITH_HTTP2
	WSI_TOKEN_HTTP_COLON_PATH,
#endif
};

int
lws_http_get_uri_and_method(struct lws *wsi, char **puri_ptr, int *puri_len)
{
	int n, count = 0;

	for (n = 0; n < (int)LWS_ARRAY_SIZE(methods); n++)
		if (lws_hdr_total_length(wsi, methods[n]))
			count++;
	if (!count) {
		lwsl_warn("Missing URI in HTTP request\n");
		return -1;
	}

	if (count != 1 &&
	    !((wsi->mux_substream || wsi->h2_stream_carries_ws)
#if defined(LWS_ROLE_H2)
			    &&
	      lws_hdr_total_length(wsi, WSI_TOKEN_HTTP_COLON_PATH)
#endif
	      )) {
		lwsl_warn("multiple methods?\n");
		return -1;
	}

	for (n = 0; n < (int)LWS_ARRAY_SIZE(methods); n++)
		if (lws_hdr_total_length(wsi, methods[n])) {
			*puri_ptr = lws_hdr_simple_ptr(wsi, methods[n]);
			*puri_len = lws_hdr_total_length(wsi, methods[n]);
			return n;
		}

	return -1;
}

#if defined(LWS_WITH_HTTP_BASIC_AUTH)

enum lws_check_basic_auth_results
lws_check_basic_auth(struct lws *wsi, const char *basic_auth_login_file,
		     unsigned int auth_mode)
{
#if defined(LWS_WITH_FILE_OPS)
	char b64[160], plain[(sizeof(b64) * 3) / 4], *pcolon;
	int m, ml, fi, bar;

	if (!basic_auth_login_file && auth_mode == LWSAUTHM_DEFAULT)
		return LCBA_CONTINUE;

	/* Did he send auth? */
	ml = lws_hdr_total_length(wsi, WSI_TOKEN_HTTP_AUTHORIZATION);
	if (!ml)
		return LCBA_FAILED_AUTH;

	/* Disallow fragmentation monkey business */

	fi = wsi->http.ah->frag_index[WSI_TOKEN_HTTP_AUTHORIZATION];
	if (wsi->http.ah->frags[fi].nfrag) {
		lwsl_err("fragmented basic auth header not allowed\n");
		return LCBA_FAILED_AUTH;
	}

	m = lws_hdr_copy(wsi, b64, sizeof(b64),
			 WSI_TOKEN_HTTP_AUTHORIZATION);
	if (m < 7) {
		lwsl_err("b64 auth too long\n");
		return LCBA_END_TRANSACTION;
	}

	b64[5] = '\0';
	if (strcasecmp(b64, "Basic")) {
		lwsl_err("auth missing basic: %s\n", b64);
		return LCBA_END_TRANSACTION;
	}

	/* It'll be like Authorization: Basic QWxhZGRpbjpPcGVuU2VzYW1l */

	m = lws_b64_decode_string(b64 + 6, plain, sizeof(plain) - 1);
	if (m < 0) {
		lwsl_err("plain auth too long\n");
		return LCBA_END_TRANSACTION;
	}

	plain[m] = '\0';
	pcolon = strchr(plain, ':');
	if (!pcolon) {
		lwsl_err("basic auth format broken\n");
		return LCBA_END_TRANSACTION;
	}

	switch (auth_mode) {
	case LWSAUTHM_DEFAULT:
		if (lws_find_string_in_file(basic_auth_login_file, plain, m))
			break;
		lwsl_err("%s: basic auth lookup failed\n", __func__);
		return LCBA_FAILED_AUTH;

	case LWSAUTHM_BASIC_AUTH_CALLBACK:
		bar = wsi->a.protocol->callback(wsi,
				LWS_CALLBACK_VERIFY_BASIC_AUTHORIZATION,
				wsi->user_space, plain, (unsigned int)m);
		if (!bar)
			return LCBA_FAILED_AUTH;
		break;
	default:
		/* Invalid auth mode so lets fail all authentication attempts */
		return LCBA_FAILED_AUTH;
	}

	/*
	 * Rewrite WSI_TOKEN_HTTP_AUTHORIZATION so it is just the
	 * authorized username
	 */

	*pcolon = '\0';
	wsi->http.ah->frags[fi].len = (uint16_t)lws_ptr_diff_size_t(pcolon, &plain[0]);
	pcolon = lws_hdr_simple_ptr(wsi, WSI_TOKEN_HTTP_AUTHORIZATION);
	strncpy(pcolon, plain, (unsigned int)(ml - 1));
	pcolon[ml - 1] = '\0';
	lwsl_info("%s: basic auth accepted for %s\n", __func__,
		 lws_hdr_simple_ptr(wsi, WSI_TOKEN_HTTP_AUTHORIZATION));

	return LCBA_CONTINUE;
#else
	return LCBA_FAILED_AUTH;
#endif
}

#endif

#if defined(LWS_WITH_HTTP_PROXY)
/*
 * Set up an onward http proxy connection according to the mount this
 * uri falls under.  Notice this can also be starting the proxying of what was
 * originally an incoming h1 upgrade, or an h2 ws "upgrade".
 */
int
lws_http_proxy_start(struct lws *wsi, const struct lws_http_mount *hit,
		     char *uri_ptr, char ws)
{
	char ads[96], host[96], *pcolon, *pslash, unix_skt = 0;
	struct lws_client_connect_info i;
	struct lws *cwsi;
	int n, na;
	unsigned int max_http_header_data = wsi->a.context->max_http_header_data > 256 ?
					    wsi->a.context->max_http_header_data : 256;
	char *rpath = NULL;

#if defined(LWS_ROLE_WS)
	if (ws)
		/*
		 * Neither our inbound ws upgrade request side, nor our onward
		 * ws client connection on our side can bind to the actual
		 * protocol that only the remote inbound side and the remote
		 * onward side understand.
		 *
		 * Instead these are both bound to our built-in "lws-ws-proxy"
		 * protocol, which understands how to proxy between the two
		 * sides.
		 *
		 * We bind the parent, inbound part here and our side of the
		 * onward client connection is bound to the same handler using
		 * the .local_protocol_name.
		 */
		lws_bind_protocol(wsi, &lws_ws_proxy, __func__);
#endif
	memset(&i, 0, sizeof(i));
	i.context = lws_get_context(wsi);

	if (hit->origin[0] == '+')
		unix_skt = 1;

	pcolon = strchr(hit->origin, ':');
	pslash = strchr(hit->origin, '/');
	if (!pslash) {
		lwsl_err("Proxy mount origin '%s' must have /\n", hit->origin);
		return -1;
	}

	if (unix_skt) {
		if (!pcolon) {
			lwsl_err("Proxy mount origin for unix skt must "
				 "have address delimited by :\n");

			return -1;
		}
		n = lws_ptr_diff(pcolon, hit->origin);
		pslash = pcolon;
	} else {
		if (pcolon > pslash)
			pcolon = NULL;

		if (pcolon)
			n = (int)(pcolon - hit->origin);
		else
			n = (int)(pslash - hit->origin);

		if (n >= (int)sizeof(ads) - 2)
			n = sizeof(ads) - 2;
	}

	memcpy(ads, hit->origin, (unsigned int)n);
	ads[n] = '\0';

	i.address = ads;
	i.port = 80;
	if (hit->origin_protocol == LWSMPRO_HTTPS) {
		i.port = 443;
		i.ssl_connection = 1;
	}
	if (pcolon)
		i.port = atoi(pcolon + 1);

	rpath = lws_malloc(max_http_header_data, __func__);
	if (!rpath)
		return -1;

	/* rpath needs cleaning after this... ---> */

	n = lws_snprintf(rpath, max_http_header_data - 1, "/%s/%s",
			 pslash + 1, uri_ptr + hit->mountpoint_len) - 1;
	lws_clean_url(rpath);
	n = (int)strlen(rpath);
	if (n && rpath[n - 1] == '/')
		n--;

	na = lws_hdr_total_length(wsi, WSI_TOKEN_HTTP_URI_ARGS);
	if (na) {
		char *p;
		int budg;

		if (!n) /* don't start with the ?... use the first / if so */
			n++;

		p = rpath + n;

		if (na >= (int)max_http_header_data - n - 2) {
			lwsl_info("%s: query string %d longer "
				  "than we can handle\n", __func__,
				  na);
			lws_free(rpath);
			return -1;
		}

		*p++ = '?';
		budg = lws_hdr_copy(wsi, p,
			     (int)(&rpath[max_http_header_data - 1] - p),
			     WSI_TOKEN_HTTP_URI_ARGS);
	       if (budg > 0)
		       p += budg;

		*p = '\0';
	}

	i.path = rpath;
	lwsl_notice("%s: proxied path '%s'\n", __func__, i.path);

	/* incoming may be h1 or h2... if he sends h1 HOST, use that
	 * directly, otherwise we must convert h2 :authority to h1
	 * host */

	i.host = NULL;
#if defined(LWS_ROLE_H2)
	n = lws_hdr_total_length(wsi, WSI_TOKEN_HTTP_COLON_AUTHORITY);
	if (n > 0)
		i.host = lws_hdr_simple_ptr(wsi, WSI_TOKEN_HTTP_COLON_AUTHORITY);
	else
#endif
	{
		n = lws_hdr_total_length(wsi, WSI_TOKEN_HOST);
		if (n > 0)
			i.host = lws_hdr_simple_ptr(wsi, WSI_TOKEN_HOST);
	}

#if 0
	if (i.address[0] != '+' ||
	    !lws_hdr_simple_ptr(wsi, WSI_TOKEN_HOST))
		i.host = i.address;
	else
		i.host = lws_hdr_simple_ptr(wsi, WSI_TOKEN_HOST);
#endif
	i.origin = NULL;
	if (!ws) {
		if (lws_hdr_simple_ptr(wsi, WSI_TOKEN_POST_URI)
#if defined(LWS_WITH_HTTP2)
								|| (
			lws_hdr_simple_ptr(wsi, WSI_TOKEN_HTTP_COLON_METHOD) &&
			!strcmp(lws_hdr_simple_ptr(wsi,
					WSI_TOKEN_HTTP_COLON_METHOD), "post")
			)
#endif
		)
			i.method = "POST";
		else if (lws_hdr_simple_ptr(wsi, WSI_TOKEN_PUT_URI)
#if defined(LWS_WITH_HTTP2)
								|| (
			lws_hdr_simple_ptr(wsi, WSI_TOKEN_HTTP_COLON_METHOD) &&
			!strcmp(lws_hdr_simple_ptr(wsi,
					WSI_TOKEN_HTTP_COLON_METHOD), "put")
			)
#endif
		)
			i.method = "PUT";
		else if (lws_hdr_simple_ptr(wsi, WSI_TOKEN_PATCH_URI)
#if defined(LWS_WITH_HTTP2)
								|| (
			lws_hdr_simple_ptr(wsi, WSI_TOKEN_HTTP_COLON_METHOD) &&
			!strcmp(lws_hdr_simple_ptr(wsi,
					WSI_TOKEN_HTTP_COLON_METHOD), "patch")
			)
#endif
		)
			i.method = "PATCH";
		else if (lws_hdr_simple_ptr(wsi, WSI_TOKEN_DELETE_URI)
#if defined(LWS_WITH_HTTP2)
								|| (
			lws_hdr_simple_ptr(wsi, WSI_TOKEN_HTTP_COLON_METHOD) &&
			!strcmp(lws_hdr_simple_ptr(wsi,
					WSI_TOKEN_HTTP_COLON_METHOD), "delete")
			)
#endif
		)
			i.method = "DELETE";
		else
			i.method = "GET";
	}

	if (i.host)
		lws_snprintf(host, sizeof(host), "%s:%u", i.host,
					wsi->a.vhost->listen_port);
	else
		lws_snprintf(host, sizeof(host), "%s:%d", i.address, i.port);

	i.host = host;

	i.alpn = "http/1.1";
	i.parent_wsi = wsi;
	i.pwsi = &cwsi;
#if defined(LWS_ROLE_WS)
	i.protocol = lws_hdr_simple_ptr(wsi, WSI_TOKEN_PROTOCOL);
	if (ws)
		i.local_protocol_name = "lws-ws-proxy";
#endif

//	i.uri_replace_from = hit->origin;
//	i.uri_replace_to = hit->mountpoint;

	lwsl_info("proxying to %s port %d url %s, ssl %d, from %s, to %s\n",
		   i.address, i.port, i.path, i.ssl_connection,
		   i.uri_replace_from, i.uri_replace_to);

	if (!lws_client_connect_via_info(&i)) {
		lwsl_err("proxy connect fail\n");

		/*
		 * ... we can't do the proxy action, but we can
		 * cleanly return him a 503 and a description
		 */

		lws_return_http_status(wsi,
			HTTP_STATUS_SERVICE_UNAVAILABLE,
			"<h1>Service Temporarily Unavailable</h1>"
			"The server is temporarily unable to service "
			"your request due to maintenance downtime or "
			"capacity problems. Please try again later.");
		lws_free(rpath);
		return 1;
	}
	lws_free(rpath);

	lwsl_info("%s: setting proxy clientside on %s (parent %s)\n",
		  __func__, lws_wsi_tag(cwsi), lws_wsi_tag(lws_get_parent(cwsi)));

	cwsi->http.proxy_clientside = 1;
	if (ws) {
		wsi->proxied_ws_parent = 1;
		cwsi->h1_ws_proxied = 1;
		if (i.protocol) {
			lwsl_debug("%s: (requesting '%s')\n",
					__func__, i.protocol);
		}
	}

	return 0;
}
#endif


static const char * const oprot[] = {
	"http://", "https://"
};


static int
lws_http_redirect_hit(struct lws_context_per_thread *pt, struct lws *wsi,
		      const struct lws_http_mount *hit, char *uri_ptr,
		      int uri_len, int *h)
{
	char *s;
	int n;

	*h = 0;
	s = uri_ptr + hit->mountpoint_len;

	/*
	 * if we have a mountpoint like https://xxx.com/yyy
	 * there is an implied / at the end for our purposes since
	 * we can only mount on a "directory".
	 *
	 * But if we just go with that, the browser cannot understand
	 * that he is actually looking down one "directory level", so
	 * even though we give him /yyy/abc.html he acts like the
	 * current directory level is /.  So relative urls like "x.png"
	 * wrongly look outside the mountpoint.
	 *
	 * Therefore if we didn't come in on a url with an explicit
	 * / at the end, we must redirect to add it so the browser
	 * understands he is one "directory level" down.
	 */
	if ((hit->mountpoint_len > 1 ||
	     (hit->origin_protocol == LWSMPRO_REDIR_HTTP ||
	      hit->origin_protocol == LWSMPRO_REDIR_HTTPS)) &&
	    (*s != '/' ||
	     (hit->origin_protocol == LWSMPRO_REDIR_HTTP ||
	      hit->origin_protocol == LWSMPRO_REDIR_HTTPS)) &&
	    (hit->origin_protocol != LWSMPRO_CGI &&
	     hit->origin_protocol != LWSMPRO_CALLBACK)) {
		unsigned char *start = pt->serv_buf + LWS_PRE, *p = start,
			      *end = p + wsi->a.context->pt_serv_buf_size -
					LWS_PRE - 512;

		*h = 1;

		lwsl_info("Doing 301 '%s' org %s\n", s, hit->origin);

		/* > at start indicates deal with by redirect */
		if (hit->origin_protocol == LWSMPRO_REDIR_HTTP ||
		    hit->origin_protocol == LWSMPRO_REDIR_HTTPS)
			n = lws_snprintf((char *)end, 256, "%s%s",
				    oprot[hit->origin_protocol & 1],
				    hit->origin);
		else {
			if (!lws_hdr_total_length(wsi, WSI_TOKEN_HOST)) {
#if defined(LWS_ROLE_H2)
				if (!lws_hdr_total_length(wsi,
						WSI_TOKEN_HTTP_COLON_AUTHORITY))
#endif
					goto bail_nuke_ah;
#if defined(LWS_ROLE_H2)
				n = lws_snprintf((char *)end, 256,
				    "%s%s%s/", oprot[!!lws_is_ssl(wsi)],
				    lws_hdr_simple_ptr(wsi,
						WSI_TOKEN_HTTP_COLON_AUTHORITY),
				    uri_ptr);
#else
				;
#endif
			} else
				n = lws_snprintf((char *)end, 256,
				    "%s%s%s/", oprot[!!lws_is_ssl(wsi)],
				    lws_hdr_simple_ptr(wsi, WSI_TOKEN_HOST),
				    uri_ptr);
		}

		lws_clean_url((char *)end);
		n = lws_http_redirect(wsi, HTTP_STATUS_MOVED_PERMANENTLY,
				      end, n, &p, end);
		if ((int)n < 0)
			goto bail_nuke_ah;

		return lws_http_transaction_completed(wsi);
	}

	return 0;

bail_nuke_ah:
	lws_header_table_detach(wsi, 1);

	return 1;
}

int
lws_http_action(struct lws *wsi)
{
	struct lws_context_per_thread *pt = &wsi->a.context->pt[(int)wsi->tsi];
	int uri_len = 0, meth, m, http_version_len, ha;
	const struct lws_http_mount *hit = NULL;
	enum http_version request_version;
	struct lws_process_html_args args;
	enum http_conn_type conn_type;
	char content_length_str[32];
	char http_version_str[12];
	char http_conn_str[25];
	char *uri_ptr = NULL;
#if defined(LWS_WITH_FILE_OPS)
	char *s;
#endif
	unsigned int n;

	meth = lws_http_get_uri_and_method(wsi, &uri_ptr, &uri_len);
	if (meth < 0 || meth >= (int)LWS_ARRAY_SIZE(method_names))
		goto bail_nuke_ah;

	lws_metrics_tag_wsi_add(wsi, "vh", wsi->a.vhost->name);
	lws_metrics_tag_wsi_add(wsi, "meth", method_names[meth]);

	/* we insist on absolute paths */

	if (!uri_ptr || uri_ptr[0] != '/') {
		lws_return_http_status(wsi, HTTP_STATUS_FORBIDDEN, NULL);

		goto bail_nuke_ah;
	}

	lwsl_info("Method: '%s' (%d), request for '%s'\n", method_names[meth],
		  meth, uri_ptr);

	if (wsi->role_ops &&
	    lws_rops_fidx(wsi->role_ops, LWS_ROPS_check_upgrades))
		switch (lws_rops_func_fidx(wsi->role_ops,
					   LWS_ROPS_check_upgrades).
							check_upgrades(wsi)) {
		case LWS_UPG_RET_DONE:
			return 0;
		case LWS_UPG_RET_CONTINUE:
			break;
		case LWS_UPG_RET_BAIL:
			goto bail_nuke_ah;
		}

	if (lws_ensure_user_space(wsi))
		goto bail_nuke_ah;

	/* HTTP header had a content length? */

	wsi->http.rx_content_length = 0;
	wsi->http.content_length_explicitly_zero = 0;
	if (lws_hdr_total_length(wsi, WSI_TOKEN_POST_URI)
#if defined(LWS_WITH_HTTP_UNCOMMON_HEADERS)
			||
	    lws_hdr_total_length(wsi, WSI_TOKEN_PATCH_URI) ||
	    lws_hdr_total_length(wsi, WSI_TOKEN_PUT_URI)
#endif
	    )
		wsi->http.rx_content_length = 100 * 1024 * 1024;

	if (lws_hdr_total_length(wsi, WSI_TOKEN_HTTP_CONTENT_LENGTH) &&
	    lws_hdr_copy(wsi, content_length_str,
			 sizeof(content_length_str) - 1,
			 WSI_TOKEN_HTTP_CONTENT_LENGTH) > 0) {
		wsi->http.rx_content_remain = wsi->http.rx_content_length =
				(lws_filepos_t)atoll(content_length_str);
		if (!wsi->http.rx_content_length) {
			wsi->http.content_length_explicitly_zero = 1;
			lwsl_debug("%s: explicit 0 content-length\n", __func__);
		}
	}

	if (wsi->mux_substream) {
		wsi->http.request_version = HTTP_VERSION_2;
	} else {
		/* http_version? Default to 1.0, override with token: */
		request_version = HTTP_VERSION_1_0;

		/* Works for single digit HTTP versions. : */
		http_version_len = lws_hdr_total_length(wsi, WSI_TOKEN_HTTP);
		if (http_version_len > 7 &&
		    lws_hdr_copy(wsi, http_version_str,
				 sizeof(http_version_str) - 1,
				 WSI_TOKEN_HTTP) > 0 &&
		    http_version_str[5] == '1' && http_version_str[7] == '1')
			request_version = HTTP_VERSION_1_1;

		wsi->http.request_version = request_version;

		/* HTTP/1.1 defaults to "keep-alive", 1.0 to "close" */
		if (request_version == HTTP_VERSION_1_1)
			conn_type = HTTP_CONNECTION_KEEP_ALIVE;
		else
			conn_type = HTTP_CONNECTION_CLOSE;

		/* Override default if http "Connection:" header: */
		if (lws_hdr_total_length(wsi, WSI_TOKEN_CONNECTION) &&
		    lws_hdr_copy(wsi, http_conn_str, sizeof(http_conn_str) - 1,
				 WSI_TOKEN_CONNECTION) > 0) {
			http_conn_str[sizeof(http_conn_str) - 1] = '\0';
			if (!strcasecmp(http_conn_str, "keep-alive"))
				conn_type = HTTP_CONNECTION_KEEP_ALIVE;
			else
				if (!strcasecmp(http_conn_str, "close"))
					conn_type = HTTP_CONNECTION_CLOSE;
		}
		wsi->http.conn_type = conn_type;
	}

	n = (unsigned int)wsi->a.protocol->callback(wsi, LWS_CALLBACK_FILTER_HTTP_CONNECTION,
				    wsi->user_space, uri_ptr, (unsigned int)uri_len);
	if (n) {
		lwsl_info("LWS_CALLBACK_HTTP closing\n");

		return 1;
	}
	/*
	 * if there is content supposed to be coming,
	 * put a timeout on it having arrived
	 */
	if (!wsi->mux_stream_immortal)
		lws_set_timeout(wsi, PENDING_TIMEOUT_HTTP_CONTENT,
				(int)wsi->a.context->timeout_secs);
#if defined(LWS_WITH_TLS)
	if (wsi->tls.redirect_to_https) {
		/*
		 * We accepted http:// only so we could redirect to
		 * https://, so issue the redirect.  Create the redirection
		 * URI from the host: header, and regenerate the path part from
		 * the parsed pieces
		 */
		unsigned char *start = pt->serv_buf + LWS_PRE, *p = start,
			      *end = p + wsi->a.context->pt_serv_buf_size -
				     LWS_PRE;

		n = (unsigned int)lws_hdr_total_length(wsi, WSI_TOKEN_HOST);
		if (!n || n > 128)
			goto bail_nuke_ah;

		if (!lws_hdr_simple_ptr(wsi, WSI_TOKEN_HOST))
			goto bail_nuke_ah;

		p += lws_snprintf((char *)p, lws_ptr_diff_size_t(end, p), "https://");
		memcpy(p, lws_hdr_simple_ptr(wsi, WSI_TOKEN_HOST), n);
		p += n;
		*p++ = '/';
		if (uri_len >= lws_ptr_diff(end, p))
			goto bail_nuke_ah;

		if (uri_ptr[0])
			p--;
		memcpy(p, uri_ptr, (unsigned int)uri_len);
		p += uri_len;

		n = 0;
		while (lws_hdr_copy_fragment(wsi, (char *)p + 1,
					     lws_ptr_diff(end, p) - 2,
					     WSI_TOKEN_HTTP_URI_ARGS, (int)n) > 0) {
			*p = n ? '&' : '?';
			p += strlen((char *)p);
			if (p >= end - 2)
				goto bail_nuke_ah;
			n++;
		}

		n = (unsigned int)lws_ptr_diff(p, start);

		p += LWS_PRE;
		n = (unsigned int)lws_http_redirect(wsi, HTTP_STATUS_MOVED_PERMANENTLY,
				      start, (int)n, &p, end);
		if ((int)n < 0)
			goto bail_nuke_ah;

		return lws_http_transaction_completed(wsi);
	}
#endif

#ifdef LWS_WITH_ACCESS_LOG
	lws_prepare_access_log_info(wsi, uri_ptr, uri_len, meth);
#endif

	/* can we serve it from the mount list? */

	hit = lws_find_mount(wsi, uri_ptr, uri_len);
	if (!hit) {
		/* deferred cleanup and reset to protocols[0] */

		lwsl_info("no hit\n");

		if (lws_bind_protocol(wsi, &wsi->a.vhost->protocols[0],
				      "no mount hit"))
			return 1;

		lwsi_set_state(wsi, LRS_DOING_TRANSACTION);

		m = wsi->a.protocol->callback(wsi, LWS_CALLBACK_HTTP,
				    wsi->user_space, uri_ptr, (unsigned int)uri_len);

		goto after;
	}

#if defined(LWS_WITH_FILE_OPS)
	s = uri_ptr + hit->mountpoint_len;
#endif
	n = (unsigned int)lws_http_redirect_hit(pt, wsi, hit, uri_ptr, uri_len, &ha);
	if (ha)
		return (int)n;

#if defined(LWS_WITH_HTTP_BASIC_AUTH)

	/* basic auth? */

	switch (lws_check_basic_auth(wsi, hit->basic_auth_login_file,
				     hit->auth_mask & AUTH_MODE_MASK)) {
	case LCBA_CONTINUE:
		break;
	case LCBA_FAILED_AUTH:
		return lws_unauthorised_basic_auth(wsi);
	case LCBA_END_TRANSACTION:
		lws_return_http_status(wsi, HTTP_STATUS_FORBIDDEN, NULL);
		return lws_http_transaction_completed(wsi);
	}
#endif

#if defined(LWS_WITH_HTTP_PROXY)
	/*
	 * The mount is a reverse proxy?
	 */

	// if (hit)
	// lwsl_notice("%s: origin_protocol: %d\n", __func__, hit->origin_protocol);
	//else
	//	lwsl_notice("%s: no hit\n", __func__);

	if (hit->origin_protocol == LWSMPRO_HTTPS ||
	    hit->origin_protocol == LWSMPRO_HTTP) {
		n = (unsigned int)lws_http_proxy_start(wsi, hit, uri_ptr, 0);
		// lwsl_notice("proxy start says %d\n", n);
		if (n)
			return (int)n;

		goto deal_body;
	}
#endif

	/*
	 * A particular protocol callback is mounted here?
	 *
	 * For the duration of this http transaction, bind us to the
	 * associated protocol
	 */
	if (hit->origin_protocol == LWSMPRO_CALLBACK || hit->protocol) {
		const struct lws_protocols *pp;
		const char *name = hit->origin;
		if (hit->protocol)
			name = hit->protocol;

		pp = lws_vhost_name_to_protocol(wsi->a.vhost, name);
		if (!pp) {
			lwsl_err("Unable to find plugin '%s'\n",
				 hit->origin);
			return 1;
		}

		if (lws_bind_protocol(wsi, pp, "http action CALLBACK bind"))
			return 1;

		lwsl_debug("%s: %s, checking access rights for mask 0x%x\n",
				__func__, hit->origin, hit->auth_mask);

		args.p = uri_ptr;
		args.len = uri_len;
		args.max_len = hit->auth_mask & ~AUTH_MODE_MASK;
		args.final = 0; /* used to signal callback dealt with it */
		args.chunked = 0;

		n = (unsigned int)wsi->a.protocol->callback(wsi,
					    LWS_CALLBACK_CHECK_ACCESS_RIGHTS,
					    wsi->user_space, &args, 0);
		if (n) {
			lws_return_http_status(wsi, HTTP_STATUS_UNAUTHORIZED,
					       NULL);
			goto bail_nuke_ah;
		}
		if (args.final) /* callback completely handled it well */
			return 0;

		if (hit->cgienv && wsi->a.protocol->callback(wsi,
				LWS_CALLBACK_HTTP_PMO,
				wsi->user_space, (void *)hit->cgienv, 0))
			return 1;

		if (lws_hdr_total_length(wsi, WSI_TOKEN_POST_URI)) {
			m = wsi->a.protocol->callback(wsi, LWS_CALLBACK_HTTP,
					    wsi->user_space,
					    uri_ptr + hit->mountpoint_len,
					    (unsigned int)uri_len - hit->mountpoint_len);
			goto after;
		}
	}

#ifdef LWS_WITH_CGI
	/* did we hit something with a cgi:// origin? */
	if (hit->origin_protocol == LWSMPRO_CGI) {
		const char *cmd[] = {
			NULL, /* replace with cgi path */
			NULL
		};

		lwsl_debug("%s: cgi\n", __func__);
		cmd[0] = hit->origin;

		n = 5;
		if (hit->cgi_timeout)
			n = (unsigned int)hit->cgi_timeout;

		n = (unsigned int)lws_cgi(wsi, cmd, hit->mountpoint_len, (int)n,
			    hit->cgienv);
		if (n) {
			lwsl_err("%s: cgi failed\n", __func__);
			return -1;
		}

		goto deal_body;
	}
#endif

#if defined(LWS_WITH_FILE_OPS)
	n = (unsigned int)(uri_len - lws_ptr_diff(s, uri_ptr));
	if (s[0] == '\0' || (n == 1 && s[n - 1] == '/'))
		s = (char *)hit->def;
	if (!s)
		s = "index.html";
#endif

	wsi->cache_secs = (unsigned int)hit->cache_max_age;
	wsi->cache_reuse = hit->cache_reusable;
	wsi->cache_revalidate = hit->cache_revalidate;
	wsi->cache_intermediaries = hit->cache_intermediaries;

#if defined(LWS_WITH_FILE_OPS)
	m = 1;
	if (hit->origin_protocol == LWSMPRO_FILE)
		m = lws_http_serve(wsi, s, hit->origin, hit);

	if (m > 0)
#endif
	{
		/*
		 * lws_return_http_status(wsi, HTTP_STATUS_NOT_FOUND, NULL);
		 */
		if (hit->protocol) {
			const struct lws_protocols *pp =
					lws_vhost_name_to_protocol(
						wsi->a.vhost, hit->protocol);

			/* coverity */
			if (!pp)
				return 1;

			lwsi_set_state(wsi, LRS_DOING_TRANSACTION);

			if (lws_bind_protocol(wsi, pp, "http_action HTTP"))
				return 1;

			m = pp->callback(wsi, LWS_CALLBACK_HTTP,
					 wsi->user_space,
					 uri_ptr + hit->mountpoint_len,
					 (size_t)(uri_len - hit->mountpoint_len));
		} else
			m = wsi->a.protocol->callback(wsi, LWS_CALLBACK_HTTP,
				    wsi->user_space, uri_ptr, (size_t)uri_len);
	}

after:
	if (m) {
		lwsl_info("LWS_CALLBACK_HTTP closing\n");

		return 1;
	}

#if defined(LWS_WITH_CGI) || defined(LWS_WITH_HTTP_PROXY)
deal_body:
#endif
	/*
	 * If we're not issuing a file, check for content_length or
	 * HTTP keep-alive. No keep-alive header allocation for
	 * ISSUING_FILE, as this uses HTTP/1.0.
	 *
	 * In any case, return 0 and let lws_read decide how to
	 * proceed based on state
	 */
	if (lwsi_state(wsi) == LRS_ISSUING_FILE)
		return 0;

	/* Prepare to read body if we have a content length: */
	lwsl_debug("wsi->http.rx_content_length %lld %d %d\n",
		   (long long)wsi->http.rx_content_length,
		   wsi->upgraded_to_http2, wsi->mux_substream);

	if (wsi->http.content_length_explicitly_zero &&
	    lws_hdr_total_length(wsi, WSI_TOKEN_POST_URI)) {

		/*
		 * POST with an explicit content-length of zero
		 *
		 * If we don't give the user code the empty HTTP_BODY callback,
		 * he may become confused to hear the HTTP_BODY_COMPLETION (due
		 * to, eg, instantiation of lws_spa never happened).
		 *
		 * HTTP_BODY_COMPLETION is responsible for sending the result
		 * status code and result body if any, and to do the transaction
		 * complete processing.
		 */
		if (wsi->a.protocol->callback(wsi, LWS_CALLBACK_HTTP_BODY,
					    wsi->user_space, NULL, 0))
			return 1;
		if (wsi->a.protocol->callback(wsi, LWS_CALLBACK_HTTP_BODY_COMPLETION,
					    wsi->user_space, NULL, 0))
			return 1;

		return 0;
	}

	if (wsi->http.rx_content_length <= 0)
		return 0;

	if (lwsi_state(wsi) != LRS_DISCARD_BODY) {
		lwsi_set_state(wsi, LRS_BODY);
		lwsl_info("%s: %s: LRS_BODY state set (0x%x)\n", __func__,
			  lws_wsi_tag(wsi), (int)wsi->wsistate);
	}
	wsi->http.rx_content_remain = wsi->http.rx_content_length;

	/*
	 * At this point we have transitioned from deferred
	 * action to expecting BODY on the stream wsi, if it's
	 * in a bundle like h2.  So if the stream wsi has its
	 * own buflist, we need to deal with that first.
	 */

	while (1) {
		struct lws_tokens ebuf;
		int m;

		ebuf.len = (int)lws_buflist_next_segment_len(&wsi->buflist,
							     &ebuf.token);
		if (!ebuf.len)
			break;

		lwsl_debug("%s: consuming %d\n", __func__, (int)ebuf.len);
		m = lws_read_h1(wsi, ebuf.token, (lws_filepos_t)ebuf.len);
		if (m < 0)
			return -1;

		if (lws_buflist_aware_finished_consuming(wsi, &ebuf, m, 1,
							 __func__))
			return -1;
	}

	return 0;

bail_nuke_ah:
	lws_header_table_detach(wsi, 1);

	return 1;
}

int
lws_confirm_host_header(struct lws *wsi)
{
	struct lws_tokenize ts;
	lws_tokenize_elem e;
	int port = 80, n;
	char buf[128];

	/*
	 * this vhost wants us to validate what the
	 * client sent against our vhost name
	 */

	if (!lws_hdr_total_length(wsi, WSI_TOKEN_HOST)) {
		lwsl_info("%s: missing host on upgrade\n", __func__);

		return 1;
	}

#if defined(LWS_WITH_TLS)
	if (wsi->tls.ssl)
		port = 443;
#endif

	n = lws_hdr_copy(wsi, buf, sizeof(buf) - 1, WSI_TOKEN_HOST);
	if (n <= 0) {
		lwsl_info("%s: missing or oversize host header\n", __func__);
		return 1;
	}
	ts.len = (size_t)n;
	lws_tokenize_init(&ts, buf, LWS_TOKENIZE_F_DOT_NONTERM /* server.com */|
				    LWS_TOKENIZE_F_NO_FLOATS /* 1.server.com */|
				    LWS_TOKENIZE_F_MINUS_NONTERM /* a-b.com */);

	if (lws_tokenize(&ts) != LWS_TOKZE_TOKEN)
		goto bad_format;

	if (strncmp(ts.token, wsi->a.vhost->name, ts.token_len)) {
		buf[(size_t)(ts.token - buf) + ts.token_len] = '\0';
		lwsl_info("%s: '%s' in host hdr but vhost name %s\n",
			  __func__, ts.token, wsi->a.vhost->name);
		return 1;
	}

	e = lws_tokenize(&ts);
	if (e == LWS_TOKZE_DELIMITER && ts.token[0] == ':') {
		if (lws_tokenize(&ts) != LWS_TOKZE_INTEGER)
			goto bad_format;
		else
			port = atoi(ts.token);
	} else
		if (e != LWS_TOKZE_ENDED)
			goto bad_format;

	if (wsi->a.vhost->listen_port != port) {
		lwsl_info("%s: host port %d mismatches vhost port %d\n",
			  __func__, port, wsi->a.vhost->listen_port);
		return 1;
	}

	lwsl_debug("%s: host header OK\n", __func__);

	return 0;

bad_format:
	lwsl_info("%s: bad host header format\n", __func__);

	return 1;
}

#if defined(LWS_WITH_SERVER)
int
lws_http_to_fallback(struct lws *wsi, unsigned char *obuf, size_t olen)
{
	const struct lws_role_ops *role = &role_ops_raw_skt;
	const struct lws_protocols *p1, *protocol =
			 &wsi->a.vhost->protocols[wsi->a.vhost->raw_protocol_index];
	char ipbuf[64];
	int n;

	if (wsi->a.vhost->listen_accept_role &&
	    lws_role_by_name(wsi->a.vhost->listen_accept_role))
		role = lws_role_by_name(wsi->a.vhost->listen_accept_role);

	if (wsi->a.vhost->listen_accept_protocol) {
		p1 = lws_vhost_name_to_protocol(wsi->a.vhost,
			    wsi->a.vhost->listen_accept_protocol);
		if (p1)
			protocol = p1;
	}

	lws_bind_protocol(wsi, protocol, __func__);

	lws_role_transition(wsi, LWSIFR_SERVER, LRS_ESTABLISHED, role);

	lws_header_table_detach(wsi, 0);
	lws_set_timeout(wsi, NO_PENDING_TIMEOUT, 0);

	n = LWS_CALLBACK_SERVER_NEW_CLIENT_INSTANTIATED;
	if (wsi->role_ops->adoption_cb[1])
		n = wsi->role_ops->adoption_cb[1];

	ipbuf[0] = '\0';
#if !defined(LWS_PLAT_OPTEE)
	lws_get_peer_simple(wsi, ipbuf, sizeof(ipbuf));
#endif

	lwsl_notice("%s: vh %s, peer: %s, role %s, "
		    "protocol %s, cb %d, ah %p\n", __func__, wsi->a.vhost->name,
		    ipbuf, role ? role->name : "null", protocol->name, n,
		    wsi->http.ah);

	if ((wsi->a.protocol->callback)(wsi, (enum lws_callback_reasons)n, wsi->user_space, NULL, 0))
		return 1;

	n = LWS_CALLBACK_RAW_RX;
	if (wsi->role_ops->rx_cb[lwsi_role_server(wsi)])
		n = wsi->role_ops->rx_cb[lwsi_role_server(wsi)];
	if (wsi->a.protocol->callback(wsi, (enum lws_callback_reasons)n, wsi->user_space, obuf, olen))
		return 1;

	return 0;
}

int
lws_handshake_server(struct lws *wsi, unsigned char **buf, size_t len)
{
	struct lws_context *context = lws_get_context(wsi);
	struct lws_context_per_thread *pt = &context->pt[(int)wsi->tsi];
#if defined(LWS_WITH_HTTP2)
	struct allocated_headers *ah;
#endif
	unsigned char *obuf = *buf;
#if defined(LWS_WITH_HTTP2)
	char tbuf[128], *p;
#endif
	size_t olen = len;
	int n = 0, m, i;

	if (len >= 10000000) {
		lwsl_err("%s: assert: len %ld\n", __func__, (long)len);
		assert(0);
	}

	if (!wsi->http.ah) {
		lwsl_err("%s: assert: NULL ah\n", __func__);
		assert(0);
	}

	while (len) {
		if (!lwsi_role_server(wsi) || !lwsi_role_http(wsi)) {
			lwsl_err("%s: bad wsi role 0x%x\n", __func__,
					(int)lwsi_role(wsi));
			goto bail_nuke_ah;
		}

		i = (int)len;
		m = lws_parse(wsi, *buf, &i);
		lwsl_info("%s: parsed count %d\n", __func__, (int)len - i);
		(*buf) += (int)len - i;
		len = (unsigned int)i;

		if (m == LPR_DO_FALLBACK) {

			/*
			 * http parser went off the rails and
			 * LWS_SERVER_OPTION_FALLBACK_TO_APPLY_LISTEN_
			 * ACCEPT_CONFIG is set on this vhost.
			 *
			 * We are transitioning from http with an AH, to
			 * a backup role (raw-skt, by default).  Drop
			 * the ah, bind to the role with mode as
			 * ESTABLISHED.
			 */
raw_transition:

			if (lws_http_to_fallback(wsi, obuf, olen)) {
				lwsl_info("%s: fallback -> close\n", __func__);
				goto bail_nuke_ah;
			}

			(*buf) = obuf + olen;

			return 0;
		}
		if (m) {
			lwsl_info("lws_parse failed\n");
			goto bail_nuke_ah;
		}

		/* coverity... */
		if (!wsi->http.ah)
			goto bail_nuke_ah;

		if (wsi->http.ah->parser_state != WSI_PARSING_COMPLETE)
			continue;

		lwsl_parser("%s: lws_parse sees parsing complete\n", __func__);

		/* select vhost */

		if (wsi->a.vhost->listen_port &&
		    lws_hdr_total_length(wsi, WSI_TOKEN_HOST)) {
			struct lws_vhost *vhost = lws_select_vhost(
				context, wsi->a.vhost->listen_port,
				lws_hdr_simple_ptr(wsi, WSI_TOKEN_HOST));

			if (vhost)
				lws_vhost_bind_wsi(vhost, wsi);
		} else
			lwsl_info("no host\n");

		if ((!lwsi_role_h2(wsi) || !lwsi_role_server(wsi)) &&
		    (!wsi->conn_stat_done))
			wsi->conn_stat_done = 1;

		/* check for unwelcome guests */
#if defined(LWS_WITH_HTTP_UNCOMMON_HEADERS)
		if (wsi->a.context->reject_service_keywords) {
			const struct lws_protocol_vhost_options *rej =
					wsi->a.context->reject_service_keywords;
			char ua[384], *msg = NULL;

			if (lws_hdr_copy(wsi, ua, sizeof(ua) - 1,
					 WSI_TOKEN_HTTP_USER_AGENT) > 0) {
#ifdef LWS_WITH_ACCESS_LOG
				char *uri_ptr = NULL;
				int meth, uri_len;
#endif
				ua[sizeof(ua) - 1] = '\0';
				while (rej) {
					if (!strstr(ua, rej->name)) {
						rej = rej->next;
						continue;
					}

					msg = strchr(rej->value, ' ');
					if (msg)
						msg++;
					lws_return_http_status(wsi,
						(unsigned int)atoi(rej->value), msg);
#ifdef LWS_WITH_ACCESS_LOG
					meth = lws_http_get_uri_and_method(wsi,
							&uri_ptr, &uri_len);
					if (meth >= 0)
						lws_prepare_access_log_info(wsi,
							uri_ptr, uri_len, meth);

					/* wsi close will do the log */
#endif
					/*
					 * We don't want anything from
					 * this rejected guy.  Follow
					 * the close flow, not the
					 * transaction complete flow.
					 */
					goto bail_nuke_ah;
				}
			}
		}
#endif
		/*
		 * So he may have come to us requesting one or another kind
		 * of upgrade from http... but we may want to redirect him at
		 * http level.  In that case, we need to check the redirect
		 * situation even though he's not actually wanting http and
		 * prioritize returning that if there is one.
		 */

		{
			const struct lws_http_mount *hit = NULL;
			int uri_len = 0, ha, n;
			char *uri_ptr = NULL;

			n = lws_http_get_uri_and_method(wsi, &uri_ptr, &uri_len);
			if (n >= 0) {
				hit = lws_find_mount(wsi, uri_ptr, uri_len);
				if (hit) {
					n = lws_http_redirect_hit(pt, wsi, hit, uri_ptr,
								  uri_len, &ha);
					if (ha)
						return n;
				}
			}
		}



		if (lws_hdr_total_length(wsi, WSI_TOKEN_CONNECT)) {
			lwsl_info("Changing to RAW mode\n");
			goto raw_transition;
		}

		lwsi_set_state(wsi, LRS_PRE_WS_SERVING_ACCEPT);
		lws_set_timeout(wsi, NO_PENDING_TIMEOUT, 0);

		if (lws_hdr_total_length(wsi, WSI_TOKEN_UPGRADE)) {

			const char *up = lws_hdr_simple_ptr(wsi,
							    WSI_TOKEN_UPGRADE);

			if (strcasecmp(up, "websocket") &&
			    strcasecmp(up, "h2c")) {
				lwsl_info("Unknown upgrade '%s'\n", up);

				if (lws_return_http_status(wsi,
						HTTP_STATUS_FORBIDDEN, NULL) ||
				    lws_http_transaction_completed(wsi))
					goto bail_nuke_ah;
			}

			n = user_callback_handle_rxflow(wsi->a.protocol->callback,
					wsi, LWS_CALLBACK_HTTP_CONFIRM_UPGRADE,
					wsi->user_space, (char *)up, 0);

			/* just hang up? */

			if (n < 0)
				goto bail_nuke_ah;

			/* callback returned headers already, do t_c? */

			if (n > 0) {
				if (lws_http_transaction_completed(wsi))
					goto bail_nuke_ah;

				/* continue on */

				return 0;
			}

			/* callback said 0, it was allowed */

			if (wsi->a.vhost->options &
			    LWS_SERVER_OPTION_VHOST_UPG_STRICT_HOST_CHECK &&
			    lws_confirm_host_header(wsi))
				goto bail_nuke_ah;

			if (!strcasecmp(up, "websocket")) {
#if defined(LWS_ROLE_WS)
				lws_metrics_tag_wsi_add(wsi, "upg", "ws");
				lwsl_info("Upgrade to ws\n");
				goto upgrade_ws;
#endif
			}
#if defined(LWS_WITH_HTTP2)
			if (!strcasecmp(up, "h2c")) {
				lws_metrics_tag_wsi_add(wsi, "upg", "h2c");
				lwsl_info("Upgrade to h2c\n");
				goto upgrade_h2c;
			}
#endif
		}

		/* no upgrade ack... he remained as HTTP */

		lwsl_info("%s: %s: No upgrade\n", __func__, lws_wsi_tag(wsi));

		lwsi_set_state(wsi, LRS_ESTABLISHED);
#if defined(LWS_WITH_FILE_OPS)
		wsi->http.fop_fd = NULL;
#endif

#if defined(LWS_WITH_HTTP_STREAM_COMPRESSION)
		lws_http_compression_validate(wsi);
#endif

		lwsl_debug("%s: %s: ah %p\n", __func__, lws_wsi_tag(wsi),
			   (void *)wsi->http.ah);

		n = lws_http_action(wsi);

		return n;

#if defined(LWS_WITH_HTTP2)
upgrade_h2c:
		if (!lws_hdr_total_length(wsi, WSI_TOKEN_HTTP2_SETTINGS)) {
			lwsl_info("missing http2_settings\n");
			goto bail_nuke_ah;
		}

		lwsl_info("h2c upgrade...\n");

		p = lws_hdr_simple_ptr(wsi, WSI_TOKEN_HTTP2_SETTINGS);
		/* convert the peer's HTTP-Settings */
		n = lws_b64_decode_string(p, tbuf, sizeof(tbuf));
		if (n < 0) {
			lwsl_parser("HTTP2_SETTINGS too long\n");
			return 1;
		}

		wsi->upgraded_to_http2 = 1;

		/* adopt the header info */

		ah = wsi->http.ah;
		lws_role_transition(wsi, LWSIFR_SERVER, LRS_H2_AWAIT_PREFACE,
				    &role_ops_h2);

		/* http2 union member has http union struct at start */
		wsi->http.ah = ah;

		if (!wsi->h2.h2n) {
			wsi->h2.h2n = lws_zalloc(sizeof(*wsi->h2.h2n), "h2n");
			if (!wsi->h2.h2n)
				return 1;
		}

		lws_h2_init(wsi);

		/* HTTP2 union */

		lws_h2_settings(wsi, &wsi->h2.h2n->peer_set, (uint8_t *)tbuf, n);

		if (lws_hpack_dynamic_size(wsi, (int)wsi->h2.h2n->peer_set.s[
		                                      H2SET_HEADER_TABLE_SIZE]))
			return 1;

		strcpy(tbuf, "HTTP/1.1 101 Switching Protocols\x0d\x0a"
			      "Connection: Upgrade\x0d\x0a"
			      "Upgrade: h2c\x0d\x0a\x0d\x0a");
		m = (int)strlen(tbuf);
		n = lws_issue_raw(wsi, (unsigned char *)tbuf, (unsigned int)m);
		if (n != m) {
			lwsl_debug("http2 switch: ERROR writing to socket\n");
			return 1;
		}

		return 0;
#endif
#if defined(LWS_ROLE_WS)
upgrade_ws:
		if (lws_process_ws_upgrade(wsi))
			goto bail_nuke_ah;

		return 0;
#endif
	} /* while all chars are handled */

	return 0;

bail_nuke_ah:
	/* drop the header info */
	lws_header_table_detach(wsi, 1);

	return 1;
}
#endif

int LWS_WARN_UNUSED_RESULT
lws_http_transaction_completed(struct lws *wsi)
{
	int n;

	if (wsi->http.cgi_transaction_complete)
		return 0;

	if (lws_has_buffered_out(wsi)
#if defined(LWS_WITH_HTTP_STREAM_COMPRESSION)
			|| wsi->http.comp_ctx.buflist_comp ||
	    wsi->http.comp_ctx.may_have_more
#endif
	) {
		/*
		 * ...so he tried to send something large as the http reply,
		 * it went as a partial, but he immediately said the
		 * transaction was completed.
		 *
		 * Defer the transaction completed until the last part of the
		 * partial is sent.
		 */
		lwsl_debug("%s: %s: deferring due to partial\n", __func__,
				lws_wsi_tag(wsi));
		wsi->http.deferred_transaction_completed = 1;
		lws_callback_on_writable(wsi);

		return 0;
	}
	/*
	 * Are we finishing the transaction before we have consumed any body?
	 *
	 * For h1 this would kill keepalive pipelining, and for h2, considering
	 * it can extend over multiple DATA frames, it would kill the network
	 * connection.
	 */
	if (wsi->http.rx_content_length && wsi->http.rx_content_remain) {
		/*
		 * are we already in LRS_DISCARD_BODY and didn't clear the
		 * remaining before trying to complete the transaction again?
		 */
		if (lwsi_state(wsi) == LRS_DISCARD_BODY)
			return -1;
		/*
		 * let's defer transaction completed processing until we
		 * discarded the remaining body
		 */
		lwsi_set_state(wsi, LRS_DISCARD_BODY);

		return 0;
	}

#if defined(LWS_WITH_SYS_METRICS)
	{
		char tmp[10];

		lws_snprintf(tmp, sizeof(tmp), "%u", wsi->http.response_code);
		lws_metrics_tag_wsi_add(wsi, "status", tmp);
	}
#endif

	lwsl_info("%s: %s\n", __func__, lws_wsi_tag(wsi));

#if defined(LWS_WITH_HTTP_STREAM_COMPRESSION)
	lws_http_compression_destroy(wsi);
#endif
	lws_access_log(wsi);

	if (!wsi->hdr_parsing_completed
#if defined(LWS_WITH_CGI)
			&& !wsi->http.cgi
#endif
	) {
		char peer[64];

#if !defined(LWS_PLAT_OPTEE)
		lws_get_peer_simple(wsi, peer, sizeof(peer) - 1);
#else
		peer[0] = '\0';
#endif
		peer[sizeof(peer) - 1] = '\0';
		lwsl_info("%s: (from %s) ignoring, ah parsing incomplete\n",
				__func__, peer);
		return 0;
	}

#if defined(LWS_WITH_CGI)
	if (wsi->http.cgi) {
		lwsl_debug("%s: cleaning cgi\n", __func__);
		wsi->http.cgi_transaction_complete = 1;
		lws_cgi_remove_and_kill(wsi);
		lws_spawn_piped_destroy(&wsi->http.cgi->lsp);
		lws_sul_cancel(&wsi->http.cgi->sul_grace);

		lws_free_set_NULL(wsi->http.cgi);
		wsi->http.cgi_transaction_complete = 0;
	}
#endif

	/* if we can't go back to accept new headers, drop the connection */
	if (wsi->mux_substream)
		return 1;

	if (wsi->seen_zero_length_recv)
		return 1;

	if (wsi->http.conn_type != HTTP_CONNECTION_KEEP_ALIVE) {
		lwsl_info("%s: %s: close connection\n", __func__, lws_wsi_tag(wsi));
		return 1;
	}

	if (lws_bind_protocol(wsi, &wsi->a.vhost->protocols[0], __func__))
		return 1;

	/*
	 * otherwise set ourselves up ready to go again, but because we have no
	 * idea about the wsi writability, we make put it in a holding state
	 * until we can verify POLLOUT.  The part of this that confirms POLLOUT
	 * with no partials is in lws_server_socket_service() below.
	 */
	lwsl_debug("%s: %s: setting DEF_ACT from 0x%x: %p\n", __func__,
		   lws_wsi_tag(wsi), (int)wsi->wsistate, wsi->buflist);
	lwsi_set_state(wsi, LRS_DEFERRING_ACTION);
	wsi->http.tx_content_length = 0;
	wsi->http.tx_content_remain = 0;
	wsi->hdr_parsing_completed = 0;
	wsi->sending_chunked = 0;
#ifdef LWS_WITH_ACCESS_LOG
	wsi->http.access_log.sent = 0;
#endif
#if defined(LWS_WITH_FILE_OPS) && (defined(LWS_ROLE_H1) || defined(LWS_ROLE_H2))
	if (lwsi_role_http(wsi) && lwsi_role_server(wsi) &&
	    wsi->http.fop_fd != NULL)
		lws_vfs_file_close(&wsi->http.fop_fd);
#endif

	n = NO_PENDING_TIMEOUT;
	if (wsi->a.vhost->keepalive_timeout)
		n = PENDING_TIMEOUT_HTTP_KEEPALIVE_IDLE;
	lws_set_timeout(wsi, (enum pending_timeout)n, wsi->a.vhost->keepalive_timeout);

	/*
	 * We already know we are on http1.1 / keepalive and the next thing
	 * coming will be another header set.
	 *
	 * If there is no pending rx and we still have the ah, drop it and
	 * reacquire a new ah when the new headers start to arrive.  (Otherwise
	 * we needlessly hog an ah indefinitely.)
	 *
	 * However if there is pending rx and we know from the keepalive state
	 * that is already at least the start of another header set, simply
	 * reset the existing header table and keep it.
	 */
	if (wsi->http.ah) {
		// lws_buflist_describe(&wsi->buflist, wsi, __func__);
		if (!lws_buflist_next_segment_len(&wsi->buflist, NULL)) {
			lwsl_debug("%s: %s: nothing in buflist, detaching ah\n",
				  __func__, lws_wsi_tag(wsi));
			lws_header_table_detach(wsi, 1);
#ifdef LWS_WITH_TLS
			/*
			 * additionally... if we are hogging an SSL instance
			 * with no pending pipelined headers (or ah now), and
			 * SSL is scarce, drop this connection without waiting
			 */

			if (wsi->a.vhost->tls.use_ssl &&
			    wsi->a.context->simultaneous_ssl_restriction &&
			    wsi->a.context->simultaneous_ssl ==
				   wsi->a.context->simultaneous_ssl_restriction) {
				lwsl_info("%s: simultaneous_ssl_restriction\n",
					  __func__);
				return 1;
			}
#endif
		} else {
			lwsl_info("%s: %s: resetting/keeping ah as pipeline\n",
				  __func__, lws_wsi_tag(wsi));
			lws_header_table_reset(wsi, 0);
			/*
			 * If we kept the ah, we should restrict the amount
			 * of time we are willing to keep it.  Otherwise it
			 * will be bound the whole time the connection remains
			 * open.
			 */
			lws_set_timeout(wsi, PENDING_TIMEOUT_HOLDING_AH,
					wsi->a.vhost->keepalive_timeout);
		}
		/* If we're (re)starting on headers, need other implied init */
		if (wsi->http.ah)
			wsi->http.ah->ues = URIES_IDLE;

		//lwsi_set_state(wsi, LRS_ESTABLISHED); // !!!
	} else
		if (lws_buflist_next_segment_len(&wsi->buflist, NULL))
			if (lws_header_table_attach(wsi, 0))
				lwsl_debug("acquired ah\n");

	lwsl_debug("%s: %s: keep-alive await new transaction (state 0x%x)\n",
		   __func__, lws_wsi_tag(wsi), (int)wsi->wsistate);
	lws_callback_on_writable(wsi);

	return 0;
}

#if defined(LWS_WITH_FILE_OPS)
int
lws_serve_http_file(struct lws *wsi, const char *file, const char *content_type,
		    const char *other_headers, int other_headers_len)
{
	struct lws_context *context = lws_get_context(wsi);
	struct lws_context_per_thread *pt = &context->pt[(int)wsi->tsi];
	unsigned char *response = pt->serv_buf + LWS_PRE;
#if defined(LWS_WITH_RANGES)
	struct lws_range_parsing *rp = &wsi->http.range;
#endif
	int ret = 0, cclen = 8, n = HTTP_STATUS_OK;
	char cache_control[50], *cc = "no-store";
	lws_fop_flags_t fflags = LWS_O_RDONLY;
	const struct lws_plat_file_ops *fops;
	lws_filepos_t total_content_length;
	unsigned char *p = response;
	unsigned char *end = p + context->pt_serv_buf_size - LWS_PRE;
	const char *vpath;
#if defined(LWS_WITH_RANGES)
	int ranges;
#endif

	if (wsi->handling_404)
		n = HTTP_STATUS_NOT_FOUND;

	/*
	 * We either call the platform fops .open with first arg platform fops,
	 * or we call fops_zip .open with first arg platform fops, and fops_zip
	 * open will decide whether to switch to fops_zip or stay with fops_def.
	 *
	 * If wsi->http.fop_fd is already set, the caller already opened it
	 */
	if (!wsi->http.fop_fd) {
		fops = lws_vfs_select_fops(wsi->a.context->fops, file, &vpath);
		fflags |= lws_vfs_prepare_flags(wsi);
		wsi->http.fop_fd = fops->LWS_FOP_OPEN(wsi->a.context->fops,
							file, vpath, &fflags);
		if (!wsi->http.fop_fd) {
			lwsl_info("%s: Unable to open: '%s': errno %d\n",
				  __func__, file, errno);
			if (lws_return_http_status(wsi, HTTP_STATUS_NOT_FOUND,
						   NULL))
						return -1;
			return !wsi->mux_substream;
		}
	}

	/*
	 * Caution... wsi->http.fop_fd is live from here
	 */

	wsi->http.filelen = lws_vfs_get_length(wsi->http.fop_fd);
	total_content_length = wsi->http.filelen;

#if defined(LWS_WITH_RANGES)
	ranges = lws_ranges_init(wsi, rp, wsi->http.filelen);

	lwsl_debug("Range count %d\n", ranges);
	/*
	 * no ranges -> 200;
	 *  1 range  -> 206 + Content-Type: normal; Content-Range;
	 *  more     -> 206 + Content-Type: multipart/byteranges
	 *  		Repeat the true Content-Type in each multipart header
	 *  		along with Content-Range
	 */
	if (ranges < 0) {
		/* it means he expressed a range in Range:, but it was illegal */
		lws_return_http_status(wsi,
				HTTP_STATUS_REQ_RANGE_NOT_SATISFIABLE, NULL);
		if (lws_http_transaction_completed(wsi))
			goto bail; /* <0 means just hang up */

		lws_vfs_file_close(&wsi->http.fop_fd);

		return 0; /* == 0 means we did the transaction complete */
	}
	if (ranges)
		n = HTTP_STATUS_PARTIAL_CONTENT;
#endif

	if (lws_add_http_header_status(wsi, (unsigned int)n, &p, end))
		goto bail;

	if ((wsi->http.fop_fd->flags & (LWS_FOP_FLAG_COMPR_ACCEPTABLE_GZIP |
		       LWS_FOP_FLAG_COMPR_IS_GZIP)) ==
	    (LWS_FOP_FLAG_COMPR_ACCEPTABLE_GZIP | LWS_FOP_FLAG_COMPR_IS_GZIP)) {
		if (lws_add_http_header_by_token(wsi,
			WSI_TOKEN_HTTP_CONTENT_ENCODING,
			(unsigned char *)"gzip", 4, &p, end))
			goto bail;
		lwsl_info("file is being provided in gzip\n");
	}
#if defined(LWS_WITH_HTTP_STREAM_COMPRESSION)
	else {
		/*
		 * if we know its very compressible, and we can use
		 * compression, then use the most preferred compression
		 * method that the client said he will accept
		 */

		if (!wsi->interpreting && (
		     !strncmp(content_type, "text/", 5) ||
		     !strcmp(content_type, "application/javascript") ||
		     !strcmp(content_type, "image/svg+xml")))
			lws_http_compression_apply(wsi, NULL, &p, end, 0);
	}
#endif

	if (
#if defined(LWS_WITH_RANGES)
	    ranges < 2 &&
#endif
	    content_type && content_type[0])
		if (lws_add_http_header_by_token(wsi,
						 WSI_TOKEN_HTTP_CONTENT_TYPE,
						 (unsigned char *)content_type,
						 (int)strlen(content_type),
						 &p, end))
			goto bail;

#if defined(LWS_WITH_RANGES)
	if (ranges >= 2) { /* multipart byteranges */
		lws_strncpy(wsi->http.multipart_content_type, content_type,
			sizeof(wsi->http.multipart_content_type));

		if (lws_add_http_header_by_token(wsi,
						 WSI_TOKEN_HTTP_CONTENT_TYPE,
						 (unsigned char *)
						 "multipart/byteranges; "
						 "boundary=_lws",
			 	 	 	 20, &p, end))
			goto bail;

		/*
		 *  our overall content length has to include
		 *
		 *  - (n + 1) x "_lws\r\n"
		 *  - n x Content-Type: xxx/xxx\r\n
		 *  - n x Content-Range: bytes xxx-yyy/zzz\r\n
		 *  - n x /r/n
		 *  - the actual payloads (aggregated in rp->agg)
		 *
		 *  Precompute it for the main response header
		 */

		total_content_length = (lws_filepos_t)rp->agg +
				       6 /* final _lws\r\n */;

		lws_ranges_reset(rp);
		while (lws_ranges_next(rp)) {
			n = lws_snprintf(cache_control, sizeof(cache_control),
					"bytes %llu-%llu/%llu",
					rp->start, rp->end, rp->extent);

			total_content_length = total_content_length +
					(lws_filepos_t)(
				6 /* header _lws\r\n */ +
				/* Content-Type: xxx/xxx\r\n */
				14 + (int)strlen(content_type) + 2 +
				/* Content-Range: xxxx\r\n */
				15 + n + 2 +
				2); /* /r/n */
		}

		lws_ranges_reset(rp);
		lws_ranges_next(rp);
	}

	if (ranges == 1) {
		total_content_length = (lws_filepos_t)rp->agg;
		n = lws_snprintf(cache_control, sizeof(cache_control),
				 "bytes %llu-%llu/%llu",
				 rp->start, rp->end, rp->extent);

		if (lws_add_http_header_by_token(wsi,
						 WSI_TOKEN_HTTP_CONTENT_RANGE,
						 (unsigned char *)cache_control,
						 n, &p, end))
			goto bail;
	}

	wsi->http.range.inside = 0;

	if (lws_add_http_header_by_token(wsi, WSI_TOKEN_HTTP_ACCEPT_RANGES,
					 (unsigned char *)"bytes", 5, &p, end))
		goto bail;
#endif

	if (!wsi->mux_substream) {
		/* for http/1.1 ... */
		if (!wsi->sending_chunked
#if defined(LWS_WITH_HTTP_STREAM_COMPRESSION)
				&& !wsi->http.lcs
#endif
		) {
			/* ... if not already using chunked and not using an
			 * http compression translation, then send the naive
			 * content length
			 */
			if (lws_add_http_header_content_length(wsi,
						total_content_length, &p, end))
				goto bail;
		} else {

#if defined(LWS_WITH_HTTP_STREAM_COMPRESSION)
			if (wsi->http.lcs) {

				/* ...otherwise, for http 1 it must go chunked.
				 * For the compression case, the reason is we
				 * compress on the fly and do not know the
				 * compressed content-length until it has all
				 * been sent.  Http/1.1 pipelining must be able
				 * to know where the transaction boundaries are
				 * ... so chunking...
				 */
				if (lws_add_http_header_by_token(wsi,
						WSI_TOKEN_HTTP_TRANSFER_ENCODING,
						(unsigned char *)"chunked", 7,
						&p, end))
					goto bail;

				/*
				 * ...this is fun, isn't it :-)  For h1 that is
				 * using an http compression translation, the
				 * compressor must chunk its output privately.
				 *
				 * h2 doesn't need (or support) any of this
				 * crap.
				 */
				lwsl_debug("setting chunking\n");
				wsi->http.comp_ctx.chunking = 1;
			}
#endif
		}
	}

	if (wsi->cache_secs && wsi->cache_reuse) {
		if (!wsi->cache_revalidate) {
			cc = cache_control;
			cclen = sprintf(cache_control, "%s, max-age=%u",
				    intermediates[wsi->cache_intermediaries],
				    wsi->cache_secs);
		} else {
			cc = cache_control;
			cclen = sprintf(cache_control,
					"must-revalidate, %s, max-age=%u",
                                intermediates[wsi->cache_intermediaries],
                                                    wsi->cache_secs);

		}
	}

	/* Only add cache control if its not specified by any other_headers. */
	if (!other_headers ||
	    (!strstr(other_headers, "cache-control") &&
	     !strstr(other_headers, "Cache-Control"))) {
		if (lws_add_http_header_by_token(wsi,
				WSI_TOKEN_HTTP_CACHE_CONTROL,
				(unsigned char *)cc, cclen, &p, end))
			goto bail;
	}

	if (other_headers) {
		if ((end - p) < other_headers_len)
			goto bail;
		memcpy(p, other_headers, (unsigned int)other_headers_len);
		p += other_headers_len;
	}

	if (lws_finalize_http_header(wsi, &p, end))
		goto bail;

	ret = lws_write(wsi, response, lws_ptr_diff_size_t(p, response), LWS_WRITE_HTTP_HEADERS);
	if (ret != (p - response)) {
		lwsl_err("_write returned %d from %ld\n", ret,
			 (long)(p - response));
		goto bail;
	}

	wsi->http.filepos = 0;
	lwsi_set_state(wsi, LRS_ISSUING_FILE);

	if (lws_hdr_total_length(wsi, WSI_TOKEN_HEAD_URI)) {
		/* we do not emit the body */
		lws_vfs_file_close(&wsi->http.fop_fd);
		if (lws_http_transaction_completed(wsi))
			goto bail;

		return 0;
	}

	lws_callback_on_writable(wsi);

	return 0;

bail:
	lws_vfs_file_close(&wsi->http.fop_fd);

	return -1;
}
#endif

#if defined(LWS_WITH_FILE_OPS)

int lws_serve_http_file_fragment(struct lws *wsi)
{
	struct lws_context *context = wsi->a.context;
	struct lws_context_per_thread *pt = &context->pt[(int)wsi->tsi];
	struct lws_process_html_args args;
	lws_filepos_t amount, poss;
	unsigned char *p, *pstart;
#if defined(LWS_WITH_RANGES)
	unsigned char finished = 0;
#endif
#if defined(LWS_ROLE_H2)
	struct lws *nwsi;
#endif
	int n, m;

	lwsl_debug("wsi->mux_substream %d\n", wsi->mux_substream);

	do {

		/* priority 1: buffered output */

		if (lws_has_buffered_out(wsi)) {
			if (lws_issue_raw(wsi, NULL, 0) < 0) {
				lwsl_info("%s: closing\n", __func__);
				goto file_had_it;
			}
			break;
		}

		/* priority 2: buffered pre-compression-transform */

#if defined(LWS_WITH_HTTP_STREAM_COMPRESSION)
	if (wsi->http.comp_ctx.buflist_comp ||
	    wsi->http.comp_ctx.may_have_more) {
		enum lws_write_protocol wp = LWS_WRITE_HTTP;

		lwsl_info("%s: completing comp partial (buflist %p, may %d)\n",
			   __func__, wsi->http.comp_ctx.buflist_comp,
			   wsi->http.comp_ctx.may_have_more);

		if (lws_rops_fidx(wsi->role_ops, LWS_ROPS_write_role_protocol) &&
		    lws_rops_func_fidx(wsi->role_ops, LWS_ROPS_write_role_protocol).
					write_role_protocol(wsi, NULL, 0, &wp) < 0) {
			lwsl_info("%s signalling to close\n", __func__);
			goto file_had_it;
		}
		lws_callback_on_writable(wsi);

		break;
	}
#endif

		if (wsi->http.filepos == wsi->http.filelen)
			goto all_sent;

		n = 0;
		p = pstart = pt->serv_buf + LWS_H2_FRAME_HEADER_LENGTH;

#if defined(LWS_WITH_RANGES)
		if (wsi->http.range.count_ranges && !wsi->http.range.inside) {

			lwsl_notice("%s: doing range start %llu\n", __func__,
				    wsi->http.range.start);

			if ((long long)lws_vfs_file_seek_cur(wsi->http.fop_fd,
						   (lws_fileofs_t)wsi->http.range.start -
						   (lws_fileofs_t)wsi->http.filepos) < 0)
				goto file_had_it;

			wsi->http.filepos = wsi->http.range.start;

			if (wsi->http.range.count_ranges > 1) {
				n =  lws_snprintf((char *)p,
						context->pt_serv_buf_size -
						LWS_H2_FRAME_HEADER_LENGTH,
					"_lws\x0d\x0a"
					"Content-Type: %s\x0d\x0a"
					"Content-Range: bytes "
						"%llu-%llu/%llu\x0d\x0a"
					"\x0d\x0a",
					wsi->http.multipart_content_type,
					wsi->http.range.start,
					wsi->http.range.end,
					wsi->http.range.extent);
				p += n;
			}

			wsi->http.range.budget = wsi->http.range.end -
						   wsi->http.range.start + 1;
			wsi->http.range.inside = 1;
		}
#endif

		poss = context->pt_serv_buf_size;

#if defined(LWS_ROLE_H2)
		/*
		 * If it's h2, restrict any lump that we are sending to the
		 * max h2 frame size the peer indicated he could handle in
		 * his SETTINGS
		 */
		nwsi = lws_get_network_wsi(wsi);
		if (nwsi->h2.h2n &&
		    poss > (lws_filepos_t)nwsi->h2.h2n->peer_set.s[H2SET_MAX_FRAME_SIZE])
			poss = (lws_filepos_t)nwsi->h2.h2n->peer_set.s[H2SET_MAX_FRAME_SIZE];
#endif
		poss = poss - (lws_filepos_t)(n + LWS_H2_FRAME_HEADER_LENGTH);

		if (wsi->http.tx_content_length)
			if (poss > wsi->http.tx_content_remain)
				poss = wsi->http.tx_content_remain;

		/*
		 * If there is a hint about how much we will do well to send at
		 * one time, restrict ourselves to only trying to send that.
		 */
		if (wsi->a.protocol->tx_packet_size &&
		    poss > wsi->a.protocol->tx_packet_size)
			poss = wsi->a.protocol->tx_packet_size;

		if (lws_rops_fidx(wsi->role_ops, LWS_ROPS_tx_credit)) {
			lws_filepos_t txc = (unsigned int)lws_rops_func_fidx(wsi->role_ops,
							       LWS_ROPS_tx_credit).
					tx_credit(wsi, LWSTXCR_US_TO_PEER, 0);

			if (!txc) {
				/*
				 * We shouldn't've been able to get the
				 * WRITEABLE if we are skint
				 */
				lwsl_notice("%s: %s: no tx credit\n", __func__,
						lws_wsi_tag(wsi));

				return 0;
			}
			if (txc < poss)
				poss = txc;

			/*
			 * Tracking consumption of the actual payload amount
			 * will be handled when the role data frame is sent...
			 */
		}

#if defined(LWS_WITH_RANGES)
		if (wsi->http.range.count_ranges) {
			if (wsi->http.range.count_ranges > 1)
				poss -= 7; /* allow for final boundary */
			if (poss > wsi->http.range.budget)
				poss = wsi->http.range.budget;
		}
#endif
		if (wsi->sending_chunked) {
			/* we need to drop the chunk size in here */
			p += 10;
			/* allow for the chunk to grow by 128 in translation */
			poss -= 10 + 128;
		}

		amount = 0;
		if (lws_vfs_file_read(wsi->http.fop_fd, &amount, p, poss) < 0)
			goto file_had_it; /* caller will close */

		if (wsi->sending_chunked)
			n = (int)amount;
		else
			n = lws_ptr_diff(p, pstart) + (int)amount;

		lwsl_debug("%s: sending %d\n", __func__, n);

		if (n) {
			lws_set_timeout(wsi, PENDING_TIMEOUT_HTTP_CONTENT,
					(int)context->timeout_secs);

			if (wsi->interpreting) {
				args.p = (char *)p;
				args.len = n;
				args.max_len = (int)(unsigned int)poss + 128;
				args.final = wsi->http.filepos + (unsigned int)n ==
							wsi->http.filelen;
				args.chunked = wsi->sending_chunked;
				if (user_callback_handle_rxflow(
				     wsi->a.vhost->protocols[
				     (int)wsi->protocol_interpret_idx].callback,
				     wsi, LWS_CALLBACK_PROCESS_HTML,
				     wsi->user_space, &args, 0) < 0)
					goto file_had_it;
				n = args.len;
				p = (unsigned char *)args.p;
			} else
				p = pstart;

#if defined(LWS_WITH_RANGES)
			if (wsi->http.range.send_ctr + 1 ==
				wsi->http.range.count_ranges && // last range
			    wsi->http.range.count_ranges > 1 && // was 2+ ranges (ie, multipart)
			    wsi->http.range.budget - amount == 0) {// final part
				n += lws_snprintf((char *)pstart + n, 6,
					"_lws\x0d\x0a"); // append trailing boundary
				lwsl_debug("added trailing boundary\n");
			}
#endif
			m = lws_write(wsi, p, (unsigned int)n, wsi->http.filepos + amount ==
					wsi->http.filelen ?
					 LWS_WRITE_HTTP_FINAL : LWS_WRITE_HTTP);
			if (m < 0)
				goto file_had_it;

			wsi->http.filepos += amount;

#if defined(LWS_WITH_RANGES)
			if (wsi->http.range.count_ranges >= 1) {
				wsi->http.range.budget -= amount;
				if (wsi->http.range.budget == 0) {
					lwsl_notice("range budget exhausted\n");
					wsi->http.range.inside = 0;
					wsi->http.range.send_ctr++;

					if (lws_ranges_next(&wsi->http.range) < 1) {
						finished = 1;
						goto all_sent;
					}
				}
			}
#endif

			if (m != n) {
				/* adjust for what was not sent */
				if (lws_vfs_file_seek_cur(wsi->http.fop_fd,
							   m - n) ==
							     (lws_fileofs_t)-1)
					goto file_had_it;
			}
		}

all_sent:
		if ((!lws_has_buffered_out(wsi)
#if defined(LWS_WITH_HTTP_STREAM_COMPRESSION)
				&& !wsi->http.comp_ctx.buflist_comp &&
		    !wsi->http.comp_ctx.may_have_more
#endif
		    ) && (wsi->http.filepos >= wsi->http.filelen
#if defined(LWS_WITH_RANGES)
		    || finished)
#else
		)
#endif
		) {
			lwsi_set_state(wsi, LRS_ESTABLISHED);
			/* we might be in keepalive, so close it off here */
			lws_vfs_file_close(&wsi->http.fop_fd);

			lwsl_debug("file completed\n");

			if (wsi->a.protocol->callback &&
			    user_callback_handle_rxflow(wsi->a.protocol->callback,
					wsi, LWS_CALLBACK_HTTP_FILE_COMPLETION,
					wsi->user_space, NULL, 0) < 0) {
					/*
					 * For http/1.x, the choices from
					 * transaction_completed are either
					 * 0 to use the connection for pipelined
					 * or nonzero to hang it up.
					 *
					 * However for http/2. while we are
					 * still interested in hanging up the
					 * nwsi if there was a network-level
					 * fatal error, simply completing the
					 * transaction is a matter of the stream
					 * state, not the root connection at the
					 * network level
					 */
					if (wsi->mux_substream)
						return 1;
					else
						return -1;
				}

			return 1;  /* >0 indicates completed */
		}
		/*
		 * while(1) here causes us to spam the whole file contents into
		 * a hugely bloated output buffer if it ever can't send the
		 * whole chunk...
		 */
	} while (!lws_send_pipe_choked(wsi));

	lws_callback_on_writable(wsi);

	return 0; /* indicates further processing must be done */

file_had_it:
	lws_vfs_file_close(&wsi->http.fop_fd);

	return -1;
}

#endif

#if defined(LWS_WITH_SERVER)
void
lws_server_get_canonical_hostname(struct lws_context *context,
				  const struct lws_context_creation_info *info)
{
	if (lws_check_opt(info->options,
			LWS_SERVER_OPTION_SKIP_SERVER_CANONICAL_NAME))
		return;
#if !defined(LWS_PLAT_FREERTOS)
	/* find canonical hostname */
	if (gethostname((char *)context->canonical_hostname,
		        sizeof(context->canonical_hostname) - 1))
		lws_strncpy((char *)context->canonical_hostname, "unknown",
			    sizeof(context->canonical_hostname));

	lwsl_cx_info(context, " canonical_hostname = %s\n",
					context->canonical_hostname);
#else
	(void)context;
#endif
}
#endif

int
lws_chunked_html_process(struct lws_process_html_args *args,
			 struct lws_process_html_state *s)
{
	char *sp, buffer[32];
	const char *pc;
	int old_len, n;

	/* do replacements */
	sp = args->p;
	old_len = args->len;
	args->len = 0;
	s->start = sp;
	while (sp < args->p + old_len) {

		if (args->len + 7 >= args->max_len) {
			lwsl_err("Used up interpret padding\n");
			return -1;
		}

		if ((!s->pos && *sp == '$') || s->pos) {
			int hits = 0, hit = 0;

			if (!s->pos)
				s->start = sp;
			s->swallow[s->pos++] = *sp;
			if (s->pos == sizeof(s->swallow) - 1)
				goto skip;
			for (n = 0; n < s->count_vars; n++)
				if (!strncmp(s->swallow, s->vars[n], (unsigned int)s->pos)) {
					hits++;
					hit = n;
				}
			if (!hits) {
skip:
				s->swallow[s->pos] = '\0';
				memcpy(s->start, s->swallow, (unsigned int)s->pos);
				args->len++;
				s->pos = 0;
				sp = s->start + 1;
				continue;
			}
			if (hits == 1 && s->pos == (int)strlen(s->vars[hit])) {
				pc = s->replace(s->data, hit);
				if (!pc)
					pc = "NULL";
				n = (int)strlen(pc);
				s->swallow[s->pos] = '\0';
				if (n != s->pos) {
					memmove(s->start + n, s->start + s->pos,
						(unsigned int)(old_len - (sp - args->p) - 1));
					old_len += (n - s->pos) + 1;
				}
				memcpy(s->start, pc, (unsigned int)n);
				args->len++;
				sp = s->start + 1;

				s->pos = 0;
			}
			sp++;
			continue;
		}

		args->len++;
		sp++;
	}

	if (args->chunked) {
		/* no space left for final chunk trailer */
		if (args->final && args->len + 7 >= args->max_len)
			return -1;

		n = sprintf(buffer, "%X\x0d\x0a", args->len);

		args->p -= n;
		memcpy(args->p, buffer, (unsigned int)n);
		args->len += n;

		if (args->final) {
			sp = args->p + args->len;
			*sp++ = '\x0d';
			*sp++ = '\x0a';
			*sp++ = '0';
			*sp++ = '\x0d';
			*sp++ = '\x0a';
			*sp++ = '\x0d';
			*sp++ = '\x0a';
			args->len += 7;
		} else {
			sp = args->p + args->len;
			*sp++ = '\x0d';
			*sp++ = '\x0a';
			args->len += 2;
		}
	}

	return 0;
}
