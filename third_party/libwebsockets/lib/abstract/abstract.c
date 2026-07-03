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

#include <private-lib-core.h>
#include <private-lib-abstract.h>

extern const lws_abs_transport_t lws_abs_transport_cli_raw_skt,
				 lws_abs_transport_cli_unit_test;
#if defined(LWS_WITH_SMTP)
extern const lws_abs_protocol_t lws_abs_protocol_smtp;
#endif
#if defined(LWS_WITH_MQTT)
extern const lws_abs_protocol_t lws_abs_protocol_mqttc;
#endif

static const lws_abs_transport_t * const available_abs_transports[] = {
	&lws_abs_transport_cli_raw_skt,
	&lws_abs_transport_cli_unit_test,
};

#if defined(LWS_WITH_ABSTRACT)
static const lws_abs_protocol_t * const available_abs_protocols[] = {
#if defined(LWS_WITH_SMTP)
	&lws_abs_protocol_smtp,
#endif
#if defined(LWS_WITH_MQTT)
	&lws_abs_protocol_mqttc,
#endif
};
#endif

const lws_abs_transport_t *
lws_abs_transport_get_by_name(const char *name)
{
	int n;

	for (n = 0; n < (int)LWS_ARRAY_SIZE(available_abs_transports); n++)
		if (!strcmp(name, available_abs_transports[n]->name))
			return available_abs_transports[n];

	lwsl_err("%s: cannot find '%s'\n", __func__, name);

	return NULL;
}

const lws_abs_protocol_t *
lws_abs_protocol_get_by_name(const char *name)
{
#if defined(LWS_WITH_ABSTRACT)
	int n;

	for (n = 0; n < (int)LWS_ARRAY_SIZE(available_abs_protocols); n++)
		if (!strcmp(name, available_abs_protocols[n]->name))
			return available_abs_protocols[n];
#endif
	lwsl_err("%s: cannot find '%s'\n", __func__, name);

	return NULL;
}

const lws_token_map_t *
lws_abs_get_token(const lws_token_map_t *token_map, short name_index)
{
	if (!token_map)
		return NULL;

	do {
		if (token_map->name_index == name_index)
			return token_map;
		token_map++;
	} while (token_map->name_index);

	return NULL;
}

static int
lws_abstract_compare_connection(lws_abs_t *abs1, lws_abs_t *abs2)
{
	/* it has to be using the same protocol */
	if (abs1->ap != abs2->ap)
		return 1;

	/* protocol has to allow some kind of binding */
	if (!abs1->ap->flags)
		return 1;

	/* it has to be using the same transport */
	if (abs1->at != abs2->at)
		return 1;

	/*
	 * The transport must feel the endpoint and conditions in use match the
	 * requested endpoint and conditions... and the transport type must be
	 * willing to allow it
	 */
	if (abs1->at->compare(abs1, abs2))
		return 1;

	/*
	 * The protocol must feel they are in compatible modes if any
	 * (and the protocol type must be willing to allow it)
	 */
	if (abs1->ap->compare(abs1, abs2))
		return 1;

	/*
	 * If no objection by now, we can say there's already a comparable
	 * connection and both the protocol and transport feel we can make
	 * use of it.
	 */

	return 0;
}

static int
find_compatible(struct lws_dll2 *d, void *user)
{
	lws_abs_t *ai1 = (lws_abs_t *)user,
		  *ai2 = lws_container_of(d, lws_abs_t, abstract_instances);

	if (!lws_abstract_compare_connection(ai1, ai2)) {
		/* we can bind to it */
		lws_dll2_add_tail(&ai1->bound, &ai2->children_owner);

		return 1;
	}

	return 0;
}

lws_abs_t *
lws_abs_bind_and_create_instance(const lws_abs_t *abs)
{
	size_t size = sizeof(lws_abs_t) + abs->ap->alloc + abs->at->alloc;
	lws_abs_t *ai;
	int n;

	/*
	 * since we know we will allocate the lws_abs_t, the protocol's
	 * instance allocation, and the transport's instance allocation,
	 * we merge it into a single heap allocation
	 */
	ai = lws_malloc(size, "abs inst");
	if (!ai)
		return NULL;

	*ai = *abs;
	ai->ati = NULL;

	ai->api = (char *)ai + sizeof(lws_abs_t);

	if (!ai->ap->flags) /* protocol only understands single connections */
		goto fresh;

	lws_vhost_lock(ai->vh); /* ----------------------------------- vh { */

	/*
	 * Let's have a look for any already-connected transport we can use
	 */

	n = lws_dll2_foreach_safe(&ai->vh->abstract_instances_owner, ai,
				  find_compatible);

	lws_vhost_unlock(ai->vh); /* } vh --------------------------------- */

	if (n)
		goto vh_list_add;

	/* there's no existing connection doing what we want */

fresh:

	ai->ati = (char *)ai->api + abs->ap->alloc;
	if (ai->at->create(ai)) {
		ai->ati = NULL;
		goto bail;
	}

vh_list_add:
	/* add us to the vhost's dll2 of instances */

	lws_dll2_clear(&ai->abstract_instances);
	lws_dll2_add_head(&ai->abstract_instances,
			  &ai->vh->abstract_instances_owner);

	if (ai->ap->create(ai)) {
		ai->api = NULL;
		goto bail;
	}

	if (ai->bound.owner) { /* we are a piggybacker */
		lws_abs_t *ai2 = lws_container_of(ai->bound.owner, lws_abs_t,
						  children_owner);
		/*
		 * Provide an 'event' in the parent context to start handling
		 * the bind if it's otherwise idle.  We give the parent abs
		 * because we don't know if we're "next" or whatever.  Just that
		 * a child joined him and he should look into his child
		 * situation in case he was waiting for one to appear.
		 */
		if (ai2->ap->child_bind(ai2)) {
			lwsl_info("%s: anticpated child bind fail\n", __func__);
			lws_dll2_remove(&ai->bound);

			goto bail;
		}
	}

	return ai;

bail:
	lws_abs_destroy_instance(&ai);

	return NULL;
}

/*
 * We get called to clean up each child that was still bound to a parent
 * at the time the parent is getting destroyed.
 */

static void
__lws_abs_destroy_instance2(lws_abs_t **ai)
{
	lws_abs_t *a = *ai;

	if (a->api)
		a->ap->destroy(&a->api);
	if (a->ati)
		a->at->destroy(&a->ati);

	lws_dll2_remove(&a->abstract_instances);

	*ai = NULL;
	free(a);
}

static int
__reap_children(struct lws_dll2 *d, void *user)
{
	lws_abs_t *ac = lws_container_of(d, lws_abs_t, bound);

	lws_dll2_foreach_safe(&ac->children_owner, NULL, __reap_children);

	/* then destroy ourselves */

	__lws_abs_destroy_instance2(&ac);

	return 0;
}

void
lws_abs_destroy_instance(lws_abs_t **ai)
{
	lws_abs_t *a = *ai;

	/* destroy child instances that are bound to us first... */

	lws_vhost_lock(a->vh); /* ----------------------------------- vh { */

	lws_dll2_foreach_safe(&a->children_owner, NULL, __reap_children);

	/* ...then destroy ourselves */

	__lws_abs_destroy_instance2(ai);

	lws_vhost_unlock(a->vh); /* } vh --------------------------------- */
}

lws_abs_t *
lws_abstract_alloc(struct lws_vhost *vhost, void *user,
		   const char *abstract_path, const lws_token_map_t *ap_tokens,
		   const lws_token_map_t *at_tokens, struct lws_sequencer *seq,
		   void *opaque_user_data)
{
	lws_abs_t *abs = lws_zalloc(sizeof(*abs), __func__);
	struct lws_tokenize ts;
	lws_tokenize_elem e;
	char tmp[30];

	if (!abs)
		return NULL;

	lws_tokenize_init(&ts, abstract_path, LWS_TOKENIZE_F_MINUS_NONTERM);

	e = lws_tokenize(&ts);
	if (e != LWS_TOKZE_TOKEN)
		goto abs_path_problem;

	if (lws_tokenize_cstr(&ts, tmp, sizeof(tmp)))
		goto abs_path_problem;

	abs->ap = lws_abs_protocol_get_by_name(tmp);
	if (!abs->ap)
		goto abs_path_problem;

	e = lws_tokenize(&ts);
	if (e != LWS_TOKZE_DELIMITER)
		goto abs_path_problem;

	e = lws_tokenize(&ts);
	if (e != LWS_TOKZE_TOKEN)
		goto abs_path_problem;

	if (lws_tokenize_cstr(&ts, tmp, sizeof(tmp)))
		goto abs_path_problem;

	abs->at = lws_abs_transport_get_by_name(tmp);
	if (!abs->at)
		goto abs_path_problem;

	abs->vh = vhost;
	abs->ap_tokens = ap_tokens;
	abs->at_tokens = at_tokens;
	abs->seq = seq;
	abs->opaque_user_data = opaque_user_data;

	lwsl_info("%s: allocated %s\n", __func__, abstract_path);

	return abs;

abs_path_problem:
	lwsl_err("%s: bad abs path '%s'\n", __func__, abstract_path);
	lws_free_set_NULL(abs);

	return NULL;
}

void
lws_abstract_free(lws_abs_t **pabs)
{
	if (*pabs)
		lws_free_set_NULL(*pabs);
}
