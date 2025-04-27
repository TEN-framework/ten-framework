/*
 * ssp-h1url plugin
 *
 * Written in 2010-2020 by Andy Green <andy@warmcat.com>
 *
 * This file is made available under the Creative Commons CC0 1.0
 * Universal Public Domain Dedication.
 *
 * CC0 so it can be used as a template for your own secure streams plugins
 * licensed how you like.
 */

#include <libwebsockets.h>

static int
ssp_h1url_create(struct lws_ss_handle *ss, void *info, plugin_auth_status_cb status)
{
	return 0;
}

static int
ssp_h1url_destroy(struct lws_ss_handle *ss)
{
	return 0;
}

static int
ssp_h1url_munge(struct lws_ss_handle *ss, char *path, size_t path_len)
{
	return 0;
}

/* this is the only exported symbol */
const lws_ss_plugin_t ssp_h1url = {
	.name			= "h1url",
	.alloc			= 0,
	.create			= ssp_h1url_create,
	.destroy		= ssp_h1url_destroy,
	.munge			= ssp_h1url_munge
};
