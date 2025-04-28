/*
 * libwebsockets - small server side websockets and web server implementation
 *
 * Copyright (C) 2010 - 2020 Andy Green <andy@warmcat.com>
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

void
lws_state_reg_notifier(lws_state_manager_t *mgr,
		       lws_state_notify_link_t *notify_link)
{
	lws_dll2_add_head(&notify_link->list, &mgr->notify_list);
}

void
lws_state_reg_deregister(lws_state_notify_link_t *nl)
{
	lws_dll2_remove(&nl->list);
}

void
lws_state_reg_notifier_list(lws_state_manager_t *mgr,
			    lws_state_notify_link_t * const *notify_link_array)
{
	if (notify_link_array)
		while (*notify_link_array)
			lws_state_reg_notifier(mgr, *notify_link_array++);
}

#if (_LWS_ENABLED_LOGS & (LLL_INFO | LLL_DEBUG))
static const char *
_systnm(lws_state_manager_t *mgr, int state, char *temp8)
{
	if (!mgr->state_names) {
		lws_snprintf(temp8, 8, "%d", state);
		return temp8;
	}

	return mgr->state_names[state];
}
#endif

static int
_report(lws_state_manager_t *mgr, int a, int b)
{
#if (_LWS_ENABLED_LOGS & LLL_INFO)
	char temp8[8];
#endif

	lws_start_foreach_dll(struct lws_dll2 *, d, mgr->notify_list.head) {
		lws_state_notify_link_t *l =
			lws_container_of(d, lws_state_notify_link_t, list);

		if (l->notify_cb(mgr, l, a, b)) {
			/* a dependency took responsibility for retry */

#if (_LWS_ENABLED_LOGS & LLL_INFO)
			lwsl_cx_info(mgr->context, "%s: %s: rejected '%s' -> '%s'",
				     mgr->name, l->name,
				     _systnm(mgr, a, temp8),
				     _systnm(mgr, b, temp8));
#endif

			return 1;
		}

	} lws_end_foreach_dll(d);

	return 0;
}

static int
_lws_state_transition(lws_state_manager_t *mgr, int target)
{
#if (_LWS_ENABLED_LOGS & LLL_DEBUG)
	char temp8[8];
#endif

	if (_report(mgr, mgr->state, target))
		return 1;

#if (_LWS_ENABLED_LOGS & LLL_DEBUG)
	if (mgr->context)
	lwsl_cx_debug(mgr->context, "%s: changed %d '%s' -> %d '%s'", mgr->name,
		   mgr->state, _systnm(mgr, mgr->state, temp8), target,
		   _systnm(mgr, target, temp8));
#endif

	mgr->state = target;

	/* Indicate success by calling the notifers again with both args same */
	_report(mgr, target, target);

#if defined(LWS_WITH_SYS_SMD)
	if (mgr->smd_class && mgr->context)
		(void)lws_smd_msg_printf(mgr->context,
				   mgr->smd_class, "{\"state\":\"%s\"}",
				   mgr->state_names[target]);
#endif

	return 0;
}

int
lws_state_transition_steps(lws_state_manager_t *mgr, int target)
{
	int n = 0;
#if (_LWS_ENABLED_LOGS & LLL_INFO)
	int i = mgr->state;
	char temp8[8];
#endif

	if (mgr->state > target)
		return 0;

	while (!n && mgr->state != target)
		n = _lws_state_transition(mgr, mgr->state + 1);

#if (_LWS_ENABLED_LOGS & LLL_INFO)
	lwsl_cx_info(mgr->context, "%s -> %s", _systnm(mgr, i, temp8),
			_systnm(mgr, mgr->state, temp8));
#endif

	return 0;
}

int
lws_state_transition(lws_state_manager_t *mgr, int target)
{
	if (mgr->state != target)
		_lws_state_transition(mgr, target);

	return 0;
}
