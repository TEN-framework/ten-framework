

struct ops {
	void (*init_and_run)(void);
	void (*stop)(void);
	void (*cleanup)(void);
};

extern struct lws_context *context;
extern int lifetime, reported;

void foreign_timer_service(void *foreign_loop);
void signal_cb(int signum);

extern const struct ops ops_libuv, ops_libevent, ops_glib, ops_libev, ops_sdevent, ops_uloop;
