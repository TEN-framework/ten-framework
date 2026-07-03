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

#include <tchar.h>
#include <stdio.h>
#include <strsafe.h>

void
lws_spawn_timeout(struct lws_sorted_usec_list *sul)
{
	struct lws_spawn_piped *lsp = lws_container_of(sul,
					struct lws_spawn_piped, sul);

	lwsl_warn("%s: spawn exceeded timeout, killing\n", __func__);

	lws_spawn_piped_kill_child_process(lsp);
}

void
lws_spawn_sul_reap(struct lws_sorted_usec_list *sul)
{
	struct lws_spawn_piped *lsp = lws_container_of(sul,
					struct lws_spawn_piped, sul_reap);

	lwsl_notice("%s: reaping spawn after last stdpipe, tries left %d\n",
		    __func__, lsp->reap_retry_budget);
	if (!lws_spawn_reap(lsp) && !lsp->pipes_alive) {
		if (--lsp->reap_retry_budget) {
			lws_sul_schedule(lsp->info.vh->context, lsp->info.tsi,
					 &lsp->sul_reap, lws_spawn_sul_reap,
					 250 * LWS_US_PER_MS);
		} else {
			lwsl_err("%s: Unable to reap lsp %p, killing\n",
				 __func__, lsp);
			lsp->reap_retry_budget = 20;
			lws_spawn_piped_kill_child_process(lsp);
		}
	}
}

static struct lws *
lws_create_basic_wsi(struct lws_context *context, int tsi,
		     const struct lws_role_ops *ops)
{
	struct lws_context_per_thread *pt = &context->pt[tsi];
	struct lws *new_wsi;

	if (!context->vhost_list)
		return NULL;

	if ((unsigned int)context->pt[tsi].fds_count ==
	    context->fd_limit_per_thread - 1) {
		lwsl_err("no space for new conn\n");
		return NULL;
	}

	lws_context_lock(context, __func__);
	new_wsi = __lws_wsi_create_with_role(context, tsi, ops, NULL);
	lws_context_unlock(context);
	if (new_wsi == NULL) {
		lwsl_err("Out of memory for new connection\n");
		return NULL;
	}

	new_wsi->rxflow_change_to = LWS_RXFLOW_ALLOW;

	/* initialize the instance struct */

	lws_role_transition(new_wsi, 0, LRS_ESTABLISHED, ops);

	new_wsi->hdr_parsing_completed = 0;
	new_wsi->position_in_fds_table = LWS_NO_FDS_POS;

	/*
	 * these can only be set once the protocol is known
	 * we set an unestablished connection's protocol pointer
	 * to the start of the defauly vhost supported list, so it can look
	 * for matching ones during the handshake
	 */

	new_wsi->user_space = NULL;
	new_wsi->desc.sockfd = LWS_SOCK_INVALID;

	return new_wsi;
}

void
lws_spawn_piped_destroy(struct lws_spawn_piped **_lsp)
{
	struct lws_spawn_piped *lsp = *_lsp;
	struct lws *wsi;
	int n;

	if (!lsp)
		return;

	for (n = 0; n < 3; n++) {
		if (lsp->pipe_fds[n][!!(n == 0)]) {
			CloseHandle(lsp->pipe_fds[n][n == 0]);
			lsp->pipe_fds[n][n == 0] = NULL;
		}

		for (n = 0; n < 3; n++) {
			if (lsp->stdwsi[n]) {
				lwsl_notice("%s: closing stdwsi %d\n", __func__, n);
				wsi = lsp->stdwsi[n];
				lsp->stdwsi[n]->desc.filefd = NULL;
				lsp->stdwsi[n] = NULL;
				lws_set_timeout(wsi, 1, LWS_TO_KILL_SYNC);
			}
		}
	}

	lws_dll2_remove(&lsp->dll);

	lws_sul_cancel(&lsp->sul);
	lws_sul_cancel(&lsp->sul_reap);
	lws_sul_cancel(&lsp->sul_poll);

	lwsl_warn("%s: deleting lsp\n", __func__);

	lws_free_set_NULL((*_lsp));
}

int
lws_spawn_reap(struct lws_spawn_piped *lsp)
{

	void *opaque = lsp->info.opaque;
	lsp_cb_t cb = lsp->info.reap_cb;
	struct _lws_siginfo_t lsi;
	lws_usec_t acct[4];
	DWORD ex;

	if (!lsp->child_pid)
		return 0;

	if (!GetExitCodeProcess(lsp->child_pid, &ex)) {
		lwsl_notice("%s: GetExitCodeProcess failed\n", __func__);
		return 0;
	}

	/* nonzero = success */

	if (ex == STILL_ACTIVE) {
		lwsl_notice("%s: still active\n", __func__);
		return 0;
	}

	/* mark the earliest time we knew he had gone */
	if (!lsp->reaped) {
		lsp->reaped = lws_now_usecs();

		/*
		 * Switch the timeout to restrict the amount of grace time
		 * to drain stdwsi
		 */

		lws_sul_schedule(lsp->info.vh->context, lsp->info.tsi,
				 &lsp->sul, lws_spawn_timeout,
				 5 * LWS_US_PER_SEC);
	}

	/*
	 * Stage finalizing our reaction to the process going down until the
	 * stdwsi flushed whatever is in flight and all noticed they were
	 * closed.  For that reason, each stdwsi close must call lws_spawn_reap
	 * to check if that was the last one and we can proceed with the reap.
	 */

	if (!lsp->ungraceful && lsp->pipes_alive) {
		lwsl_notice("%s: stdwsi alive, not reaping\n", __func__);
		return 0;
	}

	/* we reached the reap point, no need for timeout wait */

	lws_sul_cancel(&lsp->sul);

	/*
	 * All the stdwsi went down, nothing more is coming... it's over
	 * Collect the final information and then reap the dead process
	 */

	lsi.retcode = 0x10000 | (int)ex;
	lwsl_notice("%s: process exit 0x%x\n", __func__, lsi.retcode);
	lsp->child_pid = NULL;

	/* destroy the lsp itself first (it's freed and plsp set NULL */

	if (lsp->info.plsp)
		lws_spawn_piped_destroy(lsp->info.plsp);

	/* then do the parent callback informing it's destroyed */

	memset(acct, 0, sizeof(acct));
	if (cb)
		cb(opaque, acct, &lsi, 0);

	lwsl_notice("%s: completed reap\n", __func__);

	return 1; /* was reaped */
}

int
lws_spawn_piped_kill_child_process(struct lws_spawn_piped *lsp)
{
	if (!lsp->child_pid)
		return 1;

	lsp->ungraceful = 1; /* don't wait for flushing, just kill it */

	if (lws_spawn_reap(lsp))
		/* that may have invalidated lsp */
		return 0;

	lwsl_warn("%s: calling TerminateProcess on child pid\n", __func__);
	TerminateProcess(lsp->child_pid, 252);
	lws_spawn_reap(lsp);

	/* that may have invalidated lsp */

	return 0;
}

static void
windows_pipe_poll_hack(lws_sorted_usec_list_t *sul)
{
	struct lws_spawn_piped *lsp = lws_container_of(sul,
					struct lws_spawn_piped, sul_poll);
	struct lws *wsi, *wsi1;
	DWORD br;
	char c;

	/*
	 * Do it first, we know lsp exists and if it's destroyed inbetweentimes,
	 * it will already have cancelled this
	 */

	lws_sul_schedule(lsp->context, 0, &lsp->sul_poll,
			 windows_pipe_poll_hack, 50 * LWS_US_PER_MS);

	wsi = lsp->stdwsi[LWS_STDOUT];
	wsi1 = lsp->stdwsi[LWS_STDERR];
	if (wsi && lsp->pipe_fds[LWS_STDOUT][0] != NULL) {
		if (!PeekNamedPipe(lsp->pipe_fds[LWS_STDOUT][0], &c, 1, &br,
				   NULL, NULL)) {

			lwsl_notice("%s: stdout pipe errored\n", __func__);
			CloseHandle(lsp->stdwsi[LWS_STDOUT]->desc.filefd);
			lsp->pipe_fds[LWS_STDOUT][0] = NULL;
			lsp->stdwsi[LWS_STDOUT]->desc.filefd = NULL;
			lsp->stdwsi[LWS_STDOUT] = NULL;
			lws_set_timeout(wsi, 1, LWS_TO_KILL_SYNC);

			if (lsp->stdwsi[LWS_STDIN]) {
				lwsl_notice("%s: closing stdin from stdout close\n",
						__func__);
				CloseHandle(lsp->stdwsi[LWS_STDIN]->desc.filefd);
				wsi = lsp->stdwsi[LWS_STDIN];
				lsp->stdwsi[LWS_STDIN]->desc.filefd = NULL;
				lsp->stdwsi[LWS_STDIN] = NULL;
				lsp->pipe_fds[LWS_STDIN][1] = NULL;
				lws_set_timeout(wsi, 1, LWS_TO_KILL_SYNC);
			}

			/*
			 * lsp may be destroyed by here... if we wanted to
			 * handle a still-extant stderr we'll get it next time
			 */

			return;
		} else
			if (br)
				wsi->a.protocol->callback(wsi,
							LWS_CALLBACK_RAW_RX_FILE,
							NULL, NULL, 0);
	}

	/*
	 * lsp may have been destroyed above
	 */

	if (wsi1 && lsp->pipe_fds[LWS_STDERR][0]) {
		if (!PeekNamedPipe(lsp->pipe_fds[LWS_STDERR][0], &c, 1, &br,
				   NULL, NULL)) {

			lwsl_notice("%s: stderr pipe errored\n", __func__);
			CloseHandle(wsi1->desc.filefd);
			/*
			 * Assume is stderr still extant on entry, lsp can't
			 * have been destroyed by stdout/stdin processing
			 */
			lsp->stdwsi[LWS_STDERR]->desc.filefd = NULL;
			lsp->stdwsi[LWS_STDERR] = NULL;
			lsp->pipe_fds[LWS_STDERR][0] = NULL;
			lws_set_timeout(wsi1, 1, LWS_TO_KILL_SYNC);
			/*
			 * lsp may have been destroyed above
			 */
		} else
			if (br)
				wsi1->a.protocol->callback(wsi1,
							LWS_CALLBACK_RAW_RX_FILE,
							NULL, NULL, 0);
	}
}



/*
 * Deals with spawning a subprocess and executing it securely with stdin/out/err
 * diverted into pipes
 */

struct lws_spawn_piped *
lws_spawn_piped(const struct lws_spawn_piped_info *i)
{
	const struct lws_protocols *pcol = i->vh->context->vhost_list->protocols;
	struct lws_context *context = i->vh->context;
	struct lws_spawn_piped *lsp;
	PROCESS_INFORMATION pi;
	SECURITY_ATTRIBUTES sa;
	char cli[300], *p;
	STARTUPINFO si;
	int n;

	if (i->protocol_name)
		pcol = lws_vhost_name_to_protocol(i->vh, i->protocol_name);
	if (!pcol) {
		lwsl_err("%s: unknown protocol %s\n", __func__,
			 i->protocol_name ? i->protocol_name : "default");

		return NULL;
	}

	lsp = lws_zalloc(sizeof(*lsp), __func__);
	if (!lsp) {
		lwsl_err("%s: OOM\n", __func__);
		return NULL;
	}

	/* wholesale take a copy of info */
	lsp->info = *i;
	lsp->context = context;
	lsp->reap_retry_budget = 20;

	/*
	 * Prepare the stdin / out / err pipes
	 */

	for (n = 0; n < 3; n++) {
		lsp->pipe_fds[n][0] = NULL;
		lsp->pipe_fds[n][1] = NULL;
	}

	/* create pipes for [stdin|stdout] and [stderr] */

	memset(&sa, 0, sizeof(sa));
	sa.nLength = sizeof(SECURITY_ATTRIBUTES);
	sa.bInheritHandle = TRUE; /* inherit the pipes */
	sa.lpSecurityDescriptor = NULL;

	for (n = 0; n < 3; n++) {
		DWORD waitmode = PIPE_NOWAIT;

		if (!CreatePipe(&lsp->pipe_fds[n][0], &lsp->pipe_fds[n][1],
				&sa, 0)) {
			lwsl_err("%s: CreatePipe() failed\n", __func__);
			goto bail1;
		}

		SetNamedPipeHandleState(lsp->pipe_fds[1][0], &waitmode, NULL, NULL);
		SetNamedPipeHandleState(lsp->pipe_fds[2][0], &waitmode, NULL, NULL);

		/* don't inherit the pipe side that belongs to the parent */

		if (!SetHandleInformation(&lsp->pipe_fds[n][!n],
					  HANDLE_FLAG_INHERIT, 0)) {
			lwsl_err("%s: SetHandleInformation() failed\n", __func__);
			//goto bail1;
		}
	}

	/* create wsis for each stdin/out/err fd */

	for (n = 0; n < 3; n++) {
		lsp->stdwsi[n] = lws_create_basic_wsi(i->vh->context, i->tsi,
					  i->ops ? i->ops : &role_ops_raw_file);
		if (!lsp->stdwsi[n]) {
			lwsl_err("%s: unable to create lsp stdwsi\n", __func__);
			goto bail2;
		}

                __lws_lc_tag(i->vh->context, &i->vh->context->lcg[LWSLCG_WSI],
                	     &lsp->stdwsi[n]->lc, "nspawn-stdwsi-%d", n);

		lsp->stdwsi[n]->lsp_channel = n;
		lws_vhost_bind_wsi(i->vh, lsp->stdwsi[n]);
		lsp->stdwsi[n]->a.protocol = pcol;
		lsp->stdwsi[n]->a.opaque_user_data = i->opaque;

		lsp->stdwsi[n]->desc.filefd = lsp->pipe_fds[n][!n];
		lsp->stdwsi[n]->file_desc = 1;

		lwsl_debug("%s: lsp stdwsi %p: pipe idx %d -> fd %d / %d\n",
			   __func__, lsp->stdwsi[n], n,
			   lsp->pipe_fds[n][!!(n == 0)],
			   lsp->pipe_fds[n][!(n == 0)]);

#if 0

		/* read side is 0, stdin we want the write side, others read */

		lsp->stdwsi[n]->desc.filefd = lsp->pipe_fds[n][!!(n == 0)];
		if (fcntl(lsp->pipe_fds[n][!!(n == 0)], F_SETFL, O_NONBLOCK) < 0) {
			lwsl_err("%s: setting NONBLOCK failed\n", __func__);
			goto bail2;
		}
#endif
	}

	for (n = 0; n < 3; n++)
		if (i->opt_parent) {
			lsp->stdwsi[n]->parent = i->opt_parent;
			lsp->stdwsi[n]->sibling_list = i->opt_parent->child_list;
			i->opt_parent->child_list = lsp->stdwsi[n];
		}

	lwsl_notice("%s: pipe handles in %p, out %p, err %p\n", __func__,
		   lsp->stdwsi[LWS_STDIN]->desc.sockfd,
		   lsp->stdwsi[LWS_STDOUT]->desc.sockfd,
		   lsp->stdwsi[LWS_STDERR]->desc.sockfd);

	/*
	 * Windows nonblocking pipe handling is a mess that is unable
	 * to interoperate with WSA-based wait as far as I can tell.
	 *
	 * Let's set up a sul to poll the pipes and synthesize the
	 * protocol callbacks if anything coming.
	 */
	lws_sul_schedule(context, 0, &lsp->sul_poll, windows_pipe_poll_hack,
			 50 * LWS_US_PER_MS);


	/*
	 * Windows wants a single string commandline
	 */
	p = cli;
	n = 0;
	while (i->exec_array[n]) {
		lws_strncpy(p, i->exec_array[n],
			    sizeof(cli) - lws_ptr_diff(p, cli));
		if (sizeof(cli) - lws_ptr_diff(p, cli) < 4)
			break;
		p += strlen(p);
		*p++ = ' ';
		*p = '\0';
		n++;
	}

	puts(cli);

	memset(&pi, 0, sizeof(pi));
	memset(&si, 0, sizeof(si));

	si.cb		= sizeof(STARTUPINFO);
	si.hStdInput	= lsp->pipe_fds[LWS_STDIN][0];
	si.hStdOutput	= lsp->pipe_fds[LWS_STDOUT][1];
	si.hStdError	= lsp->pipe_fds[LWS_STDERR][1];
	si.dwFlags	= STARTF_USESTDHANDLES | CREATE_NO_WINDOW;
	si.wShowWindow	= TRUE;

	if (!CreateProcess(NULL, cli, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) {
		lwsl_err("%s: CreateProcess failed 0x%x\n", __func__,
				(unsigned long)GetLastError());
		goto bail3;
	}

	lsp->child_pid = pi.hProcess;

	lwsl_notice("%s: lsp %p spawned PID %d\n", __func__, lsp, lsp->child_pid);

	lws_sul_schedule(context, i->tsi, &lsp->sul, lws_spawn_timeout,
			 i->timeout_us ? i->timeout_us : 300 * LWS_US_PER_SEC);

	/*
	 *  close:                stdin:r, stdout:w, stderr:w
	 */
	for (n = 0; n < 3; n++)
		CloseHandle(lsp->pipe_fds[n][n != 0]);

	lsp->pipes_alive = 3;
	lsp->created = lws_now_usecs();

	if (i->owner)
		lws_dll2_add_head(&lsp->dll, i->owner);

	if (i->timeout_us)
		lws_sul_schedule(context, i->tsi, &lsp->sul,
				 lws_spawn_timeout, i->timeout_us);

	return lsp;

bail3:

	lws_sul_cancel(&lsp->sul_poll);

	while (--n >= 0)
		__remove_wsi_socket_from_fds(lsp->stdwsi[n]);
bail2:
	for (n = 0; n < 3; n++)
		if (lsp->stdwsi[n])
			__lws_free_wsi(lsp->stdwsi[n]);

bail1:
	for (n = 0; n < 3; n++) {
		if (lsp->pipe_fds[n][0] >= 0)
			CloseHandle(lsp->pipe_fds[n][0]);
		if (lsp->pipe_fds[n][1] >= 0)
			CloseHandle(lsp->pipe_fds[n][1]);
	}

	lws_free(lsp);

	lwsl_err("%s: failed\n", __func__);

	return NULL;
}

void
lws_spawn_stdwsi_closed(struct lws_spawn_piped *lsp, struct lws *wsi)
{
	int n;

	assert(lsp);
	lsp->pipes_alive--;
	lwsl_debug("%s: pipes alive %d\n", __func__, lsp->pipes_alive);
	if (!lsp->pipes_alive)
		lws_sul_schedule(lsp->info.vh->context, lsp->info.tsi,
				&lsp->sul_reap, lws_spawn_sul_reap, 1);

	for (n = 0; n < 3; n++)
		if (lsp->stdwsi[n] == wsi)
			lsp->stdwsi[n] = NULL;
}

int
lws_spawn_get_stdfd(struct lws *wsi)
{
	return wsi->lsp_channel;
}
