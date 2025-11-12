/*
 * lws-minimal-http-server-sse
 *
 * Written in 2010-2019 by Andy Green <andy@warmcat.com>
 *
 * This file is made available under the Creative Commons CC0 1.0
 * Universal Public Domain Dedication.
 *
 * This demonstrates a minimal http server that can serve both normal static
 * content and server-side event connections.
 *
 * To keep it simple, it serves the static stuff from the subdirectory
 * "./mount-origin" of the directory it was started in.
 *
 * You can change that by changing mount.origin below.
 */

#include <libwebsockets.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#if defined(WIN32)
#define HAVE_STRUCT_TIMESPEC
#if defined(pid_t)
#undef pid_t
#endif
#endif
#include <pthread.h>
#include <time.h>

/* one of these created for each message in the ringbuffer */

struct msg {
	void *payload; /* is malloc'd */
	size_t len;
};

/*
 * Unlike ws, http is a stateless protocol.  This pss only exists for the
 * duration of a single http transaction.  With http/1.1 keep-alive and http/2,
 * that is unrelated to (shorter than) the lifetime of the network connection.
 */
struct pss {
	struct pss *pss_list;
	struct lws *wsi;
	uint32_t tail;
};

/* one of these is created for each vhost our protocol is used with */

struct vhd {
	struct lws_context *context;
	struct lws_vhost *vhost;
	const struct lws_protocols *protocol;

	struct pss *pss_list; /* linked-list of live pss*/
	pthread_t pthread_spam[2];

	pthread_mutex_t lock_ring; /* serialize access to the ring buffer */
	struct lws_ring *ring; /* ringbuffer holding unsent messages */
	char finished;
};

static int interrupted;

#if defined(WIN32)
static void usleep(unsigned long l) { Sleep(l / 1000); }
#endif


/* destroys the message when everyone has had a copy of it */

static void
__minimal_destroy_message(void *_msg)
{
	struct msg *msg = _msg;

	free(msg->payload);
	msg->payload = NULL;
	msg->len = 0;
}

/*
 * This runs under the "spam thread" thread context only.
 *
 * We spawn two threads that generate messages with this.
 *
 */

static void *
thread_spam(void *d)
{
	struct vhd *vhd = (struct vhd *)d;
	struct msg amsg;
	int len = 128, index = 1, n, whoami = 0;

	for (n = 0; n < (int)LWS_ARRAY_SIZE(vhd->pthread_spam); n++)
		if (pthread_equal(pthread_self(), vhd->pthread_spam[n]))
			whoami = n + 1;

	do {
		/* don't generate output if nobody connected */
		if (!vhd->pss_list)
			goto wait;

		pthread_mutex_lock(&vhd->lock_ring); /* --------- ring lock { */

		/* only create if space in ringbuffer */
		n = (int)lws_ring_get_count_free_elements(vhd->ring);
		if (!n) {
			lwsl_user("dropping!\n");
			goto wait_unlock;
		}

		amsg.payload = malloc((unsigned int)len);
		if (!amsg.payload) {
			lwsl_user("OOM: dropping\n");
			goto wait_unlock;
		}
		n = lws_snprintf((char *)amsg.payload, (unsigned int)len,
			         "%s: tid: %d, msg: %d", __func__, whoami, index++);
		amsg.len = (unsigned int)n;
		n = (int)lws_ring_insert(vhd->ring, &amsg, 1);
		if (n != 1) {
			__minimal_destroy_message(&amsg);
			lwsl_user("dropping!\n");
		} else
			/*
			 * This will cause a LWS_CALLBACK_EVENT_WAIT_CANCELLED
			 * in the lws service thread context.
			 */
			lws_cancel_service(vhd->context);

wait_unlock:
		pthread_mutex_unlock(&vhd->lock_ring); /* } ring lock ------- */

wait:
		/* rand() would make more sense but coverity shrieks */
		usleep((useconds_t)(100000 + (time(NULL) & 0xffff)));

	} while (!vhd->finished);

	lwsl_notice("thread_spam %d exiting\n", whoami);

	pthread_exit(NULL);

	return NULL;
}


static int
callback_sse(struct lws *wsi, enum lws_callback_reasons reason, void *user,
	     void *in, size_t len)
{
	struct pss *pss = (struct pss *)user;
	struct vhd *vhd = (struct vhd *)lws_protocol_vh_priv_get(
			lws_get_vhost(wsi), lws_get_protocol(wsi));
	uint8_t buf[LWS_PRE + LWS_RECOMMENDED_MIN_HEADER_SPACE],
		*start = &buf[LWS_PRE], *p = start,
		*end = &buf[sizeof(buf) - 1];
	const struct msg *pmsg;
	void *retval;
	int n;

	switch (reason) {

	/* --- vhost protocol lifecycle --- */

	case LWS_CALLBACK_PROTOCOL_INIT:
		vhd = lws_protocol_vh_priv_zalloc(lws_get_vhost(wsi),
				lws_get_protocol(wsi), sizeof(struct vhd));
		vhd->context = lws_get_context(wsi);
		vhd->protocol = lws_get_protocol(wsi);
		vhd->vhost = lws_get_vhost(wsi);

		vhd->ring = lws_ring_create(sizeof(struct msg), 8,
					    __minimal_destroy_message);
		if (!vhd->ring)
			return 1;

		pthread_mutex_init(&vhd->lock_ring, NULL);

		/* start the content-creating threads */

		for (n = 0; n < (int)LWS_ARRAY_SIZE(vhd->pthread_spam); n++)
			if (pthread_create(&vhd->pthread_spam[n], NULL,
					   thread_spam, vhd)) {
				lwsl_err("thread creation failed\n");
				goto init_fail;
			}

		return 0;

	case LWS_CALLBACK_PROTOCOL_DESTROY:
		init_fail:
		vhd->finished = 1;
		for (n = 0; n < (int)LWS_ARRAY_SIZE(vhd->pthread_spam); n++)
			pthread_join(vhd->pthread_spam[n], &retval);

		if (vhd->ring)
			lws_ring_destroy(vhd->ring);

		pthread_mutex_destroy(&vhd->lock_ring);
		return 0;

	/* --- http connection lifecycle --- */

	case LWS_CALLBACK_HTTP:
		/*
		 * `in` contains the url part after our mountpoint /sse, if any
		 * you can use this to determine what data to return and store
		 * that in the pss
		 */
		lwsl_info("%s: LWS_CALLBACK_HTTP: '%s'\n", __func__,
			  (const char *)in);

		/* SSE requires a http OK response with this content-type */

		if (lws_add_http_common_headers(wsi, HTTP_STATUS_OK,
						"text/event-stream",
						LWS_ILLEGAL_HTTP_CONTENT_LEN,
						&p, end))
			return 1;

		if (lws_finalize_write_http_header(wsi, start, &p, end))
			return 1;

		/* add ourselves to the list of live pss held in the vhd */

		lws_ll_fwd_insert(pss, pss_list, vhd->pss_list);
		pss->tail = lws_ring_get_oldest_tail(vhd->ring);
		pss->wsi = wsi;

		/*
		 * This tells lws we are no longer a normal http stream,
		 * but are an "immortal" (plus or minus whatever timeout you
		 * set on it afterwards) SSE stream.  In http/2 case that also
		 * stops idle timeouts being applied to the network connection
		 * while this wsi is still open.
		 */
		lws_http_mark_sse(wsi);

		/* write the body separately */

		lws_callback_on_writable(wsi);

		return 0;

	case LWS_CALLBACK_CLOSED_HTTP:
		/* remove our closing pss from the list of live pss */

		lws_ll_fwd_remove(struct pss, pss_list, pss, vhd->pss_list);
		return 0;

	/* --- data transfer --- */

	case LWS_CALLBACK_HTTP_WRITEABLE:

		lwsl_info("%s: LWS_CALLBACK_HTTP_WRITEABLE\n", __func__);

		pmsg = lws_ring_get_element(vhd->ring, &pss->tail);
		if (!pmsg)
			break;

		p += lws_snprintf((char *)p, lws_ptr_diff_size_t(end, p),
				  "data: %s\x0d\x0a\x0d\x0a",
				  (const char *)pmsg->payload);

		if (lws_write(wsi, (uint8_t *)start, lws_ptr_diff_size_t(p, start),
			      LWS_WRITE_HTTP) != lws_ptr_diff(p, start))
			return 1;

		lws_ring_consume_and_update_oldest_tail(
			vhd->ring,	/* lws_ring object */
			struct pss,	/* type of objects with tails */
			&pss->tail,	/* tail of guy doing the consuming */
			1,	/* number of payload objects being consumed */
			vhd->pss_list,	/* head of list of objects with tails */
			tail,	/* member name of tail in objects with tails */
			pss_list /* member name of next object in objects with tails */
		);

		if (lws_ring_get_element(vhd->ring, &pss->tail))
			/* come back as soon as we can write more */
			lws_callback_on_writable(pss->wsi);

		return 0;

	case LWS_CALLBACK_EVENT_WAIT_CANCELLED:
		if (!vhd)
			break;
		/*
		 * let everybody know we want to write something on them
		 * as soon as they are ready
		 */
		lws_start_foreach_llp(struct pss **, ppss, vhd->pss_list) {
			lws_callback_on_writable((*ppss)->wsi);
		} lws_end_foreach_llp(ppss, pss_list);
		return 0;

	default:
		break;
	}

	return lws_callback_http_dummy(wsi, reason, user, in, len);
}

static struct lws_protocols protocols[] = {
	{ "http", lws_callback_http_dummy, 0, 0, 0, NULL, 0 },
	{ "sse", callback_sse, sizeof(struct pss), 0, 0, NULL, 0 },
	LWS_PROTOCOL_LIST_TERM
};

/* override the default mount for /sse in the URL space */

static const struct lws_http_mount mount_sse = {
	/* .mount_next */		NULL,		/* linked-list "next" */
	/* .mountpoint */		"/sse",		/* mountpoint URL */
	/* .origin */			NULL,		/* protocol */
	/* .def */			NULL,
	/* .protocol */			"sse",
	/* .cgienv */			NULL,
	/* .extra_mimetypes */		NULL,
	/* .interpret */		NULL,
	/* .cgi_timeout */		0,
	/* .cache_max_age */		0,
	/* .auth_mask */		0,
	/* .cache_reusable */		0,
	/* .cache_revalidate */		0,
	/* .cache_intermediaries */	0,
	/* .origin_protocol */		LWSMPRO_CALLBACK, /* dynamic */
	/* .mountpoint_len */		4,		  /* char count */
	/* .basic_auth_login_file */	NULL,
};

/* default mount serves the URL space from ./mount-origin */

static const struct lws_http_mount mount = {
	/* .mount_next */		&mount_sse,	/* linked-list "next" */
	/* .mountpoint */		"/",		/* mountpoint URL */
	/* .origin */			"./mount-origin", /* serve from dir */
	/* .def */			"index.html",	/* default filename */
	/* .protocol */			NULL,
	/* .cgienv */			NULL,
	/* .extra_mimetypes */		NULL,
	/* .interpret */		NULL,
	/* .cgi_timeout */		0,
	/* .cache_max_age */		0,
	/* .auth_mask */		0,
	/* .cache_reusable */		0,
	/* .cache_revalidate */		0,
	/* .cache_intermediaries */	0,
	/* .origin_protocol */		LWSMPRO_FILE,	/* files in a dir */
	/* .mountpoint_len */		1,		/* char count */
	/* .basic_auth_login_file */	NULL,
};

void sigint_handler(int sig)
{
	interrupted = 1;
}

int main(int argc, const char **argv)
{
	struct lws_context_creation_info info;
	struct lws_context *context;
	const char *p;
	int n = 0, logs = LLL_USER | LLL_ERR | LLL_WARN | LLL_NOTICE
			/* for LLL_ verbosity above NOTICE to be built into lws,
			 * lws must have been configured and built with
			 * -DCMAKE_BUILD_TYPE=DEBUG instead of =RELEASE */
			/* | LLL_INFO */ /* | LLL_PARSER */ /* | LLL_HEADER */
			/* | LLL_EXT */ /* | LLL_CLIENT */ /* | LLL_LATENCY */
			/* | LLL_DEBUG */;

	signal(SIGINT, sigint_handler);

	if ((p = lws_cmdline_option(argc, argv, "-d")))
		logs = atoi(p);

	lws_set_log_level(logs, NULL);
	lwsl_user("LWS minimal http Server-Side Events + ring | visit http://localhost:7681\n");

	memset(&info, 0, sizeof info); /* otherwise uninitialized garbage */
	info.port = 7681;
	info.protocols = protocols;
	info.mounts = &mount;
	info.options =
		LWS_SERVER_OPTION_HTTP_HEADERS_SECURITY_BEST_PRACTICES_ENFORCE;

	context = lws_create_context(&info);
	if (!context) {
		lwsl_err("lws init failed\n");
		return 1;
	}

	while (n >= 0 && !interrupted)
		n = lws_service(context, 0);

	lws_context_destroy(context);

	return 0;
}
