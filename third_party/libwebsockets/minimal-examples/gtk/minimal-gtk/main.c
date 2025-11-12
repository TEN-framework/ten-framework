#include <gtk/gtk.h>
#include <libwebsockets.h>

static int status = 0;

static void
print_hello(GtkWidget *widget, gpointer data)
{
	g_print("Hello World\n");
}

static void
activate(GtkApplication *app, gpointer user_data)
{
	GtkWidget *window;
	GtkWidget *button, *bbox;

	window = gtk_application_window_new(app);
	gtk_window_set_title(GTK_WINDOW(window), "mywindow");
	gtk_window_set_default_size(GTK_WINDOW(window), 200, 200);

	bbox = gtk_button_box_new(GTK_ORIENTATION_HORIZONTAL);
	gtk_container_add(GTK_CONTAINER(window), bbox);

	button = gtk_button_new_with_label("Hello World");
	g_signal_connect(button, "clicked", G_CALLBACK(print_hello), NULL);
	g_signal_connect_swapped(button, "clicked",
				 G_CALLBACK(gtk_widget_destroy), window);
	gtk_container_add(GTK_CONTAINER(bbox), button);

	gtk_widget_show_all(window);
}

static int
system_notify_cb(lws_state_manager_t *mgr, lws_state_notify_link_t *link,
		   int current, int target)
{
	struct lws_context *context = mgr->parent;
	struct lws_client_connect_info i;

	if (current != LWS_SYSTATE_OPERATIONAL ||
	    target != LWS_SYSTATE_OPERATIONAL)
		return 0;

	lwsl_notice("%s: operational\n", __func__);

	memset(&i, 0, sizeof i); /* otherwise uninitialized garbage */
	i.context = context;
	i.ssl_connection = LCCSCF_USE_SSL | LCCSCF_H2_QUIRK_OVERFLOWS_TXCR |
			   LCCSCF_H2_QUIRK_NGHTTP2_END_STREAM;
	i.port = 443;
	i.address = "warmcat.com";
	i.path = "/";
	i.host = i.address;
	i.origin = i.address;
	i.method = "GET";

	i.protocol = "http";

	return !lws_client_connect_via_info(&i);
}

static int
callback_http(struct lws *wsi, enum lws_callback_reasons reason,
	      void *user, void *in, size_t len)
{
	switch (reason) {

	/* because we are protocols[0] ... */
	case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
		lwsl_err("CLIENT_CONNECTION_ERROR: %s\n",
			 in ? (char *)in : "(null)");
		break;

	case LWS_CALLBACK_ESTABLISHED_CLIENT_HTTP:
		{
			char buf[128];

			lws_get_peer_simple(wsi, buf, sizeof(buf));
			status = lws_http_client_http_response(wsi);

			lwsl_user("Connected to %s, http response: %d\n",
					buf, status);
		}
		break;

	/* chunks of chunked content, with header removed */
	case LWS_CALLBACK_RECEIVE_CLIENT_HTTP_READ:
		lwsl_user("RECEIVE_CLIENT_HTTP_READ: read %d\n", (int)len);
		return 0; /* don't passthru */

	/* uninterpreted http content */
	case LWS_CALLBACK_RECEIVE_CLIENT_HTTP:
		{
			char buffer[1024 + LWS_PRE];
			char *px = buffer + LWS_PRE;
			int lenx = sizeof(buffer) - LWS_PRE;

			if (lws_http_client_read(wsi, &px, &lenx) < 0)
				return -1;
		}
		return 0; /* don't passthru */

	case LWS_CALLBACK_COMPLETED_CLIENT_HTTP:
		lwsl_user("LWS_CALLBACK_COMPLETED_CLIENT_HTTP\n");
		lws_cancel_service(lws_get_context(wsi)); /* abort poll wait */
		break;

	case LWS_CALLBACK_CLOSED_CLIENT_HTTP:
		lws_cancel_service(lws_get_context(wsi)); /* abort poll wait */
		break;

	default:
		break;
	}

	return lws_callback_http_dummy(wsi, reason, user, in, len);
}

static const struct lws_protocols protocols[] = {
	{
		"http",
		callback_http,
		0,
		0,
	},
	{ NULL, NULL, 0, 0 }
};

static gpointer
t1_main (gpointer user_data)
{
	lws_state_notify_link_t notifier = { { NULL, NULL, NULL },
						system_notify_cb, "app" };
	lws_state_notify_link_t *na[] = { &notifier, NULL };
	GMainContext *t1_mc = (GMainContext *)user_data;
	struct lws_context_creation_info info;
	struct lws_context *context;
	void *foreign_loops[1];
	GMainLoop *ml;

	g_print("%s: started\n", __func__);

	g_main_context_push_thread_default(t1_mc);

	ml = g_main_loop_new(t1_mc, FALSE);

	/* attach our lws activities to the main loop of this thread */

	lws_set_log_level(LLL_USER | LLL_ERR | LLL_WARN | LLL_NOTICE, NULL);
	memset(&info, 0, sizeof info);
	info.port = CONTEXT_PORT_NO_LISTEN;
	info.options = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT |
		       LWS_SERVER_OPTION_GLIB;
	info.protocols = protocols;
	foreign_loops[0] = (void *)ml;
	info.foreign_loops = foreign_loops;
	info.register_notifier_list = na;

#if defined(LWS_WITH_MBEDTLS) || defined(USE_WOLFSSL)
	/*
	 * OpenSSL uses the system trust store.  mbedTLS has to be told which
	 * CA to trust explicitly.
	 */
	info.client_ssl_ca_filepath = "./warmcat.com.cer";
#endif

	context = lws_create_context(&info);
	if (!context) {
		lwsl_err("lws init failed\n");
		return NULL;
	}

	/*
	 * We created the lws_context and bound it to this thread's main loop,
	 * let's run the thread's main loop now...
	 */

	g_main_loop_run(ml);
	g_main_loop_unref(ml);

	g_main_context_pop_thread_default(t1_mc);
	g_main_context_unref(t1_mc);

	g_print("%s: ending\n", __func__);

	lws_context_destroy(context);

	return NULL;
}

int
main(int argc, char **argv)
{
	GMainContext *t1_mc = g_main_context_new();
	GtkApplication *app;
	GThread *t1;
	int status;

	t1 = g_thread_new ("t1", t1_main, g_main_context_ref (t1_mc));
	(void)t1;

	app = gtk_application_new("org.gtk.example", G_APPLICATION_FLAGS_NONE);
	g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);

	status = g_application_run(G_APPLICATION(app), argc, argv);
	g_object_unref(app);

	return status;
}

