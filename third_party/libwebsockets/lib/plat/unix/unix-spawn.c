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

#if !defined(_GNU_SOURCE)
#define _GNU_SOURCE
#endif

#include "private-lib-core.h"
#include <unistd.h>

#if defined(__OpenBSD__) || defined(__NetBSD__)
#include <sys/resource.h>
#include <sys/wait.h>
#endif

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
lws_create_stdwsi(struct lws_context *context, int tsi,
		     const struct lws_role_ops *ops)
{
	struct lws_context_per_thread *pt = &context->pt[tsi];
	struct lws *new_wsi;

	if (!context->vhost_list)
		return NULL;

	if ((unsigned int)pt->fds_count == context->fd_limit_per_thread - 1) {
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

	/*
	 * these can only be set once the protocol is known
	 * we set an unestablished connection's protocol pointer
	 * to the start of the defauly vhost supported list, so it can look
	 * for matching ones during the handshake
	 */

	new_wsi->user_space = NULL;

	return new_wsi;
}

void
lws_spawn_piped_destroy(struct lws_spawn_piped **_lsp)
{
	struct lws_spawn_piped *lsp = *_lsp;
	int n;

	if (!lsp)
		return;

	lws_dll2_remove(&lsp->dll);

	lws_sul_cancel(&lsp->sul);
	lws_sul_cancel(&lsp->sul_reap);

	for (n = 0; n < 3; n++) {
#if 0
		if (lsp->pipe_fds[n][!!(n == 0)] == 0)
			lwsl_err("ZERO FD IN CGI CLOSE");

		if (lsp->pipe_fds[n][!!(n == 0)] >= 0) {
			close(lsp->pipe_fds[n][!!(n == 0)]);
			lsp->pipe_fds[n][!!(n == 0)] = LWS_SOCK_INVALID;
		}
#endif
		if (lsp->stdwsi[n]) {
			lws_set_timeout(lsp->stdwsi[n], 1, LWS_TO_KILL_ASYNC);
			lsp->stdwsi[n] = NULL;
		}
	}

	lws_free_set_NULL((*_lsp));
}

int
lws_spawn_reap(struct lws_spawn_piped *lsp)
{
	long hz = sysconf(_SC_CLK_TCK); /* accounting Hz */
	void *opaque = lsp->info.opaque;
	lsp_cb_t cb = lsp->info.reap_cb;
	struct lws_spawn_piped temp;
	struct tms tms;
#if defined(__OpenBSD__) || defined(__NetBSD__)
	struct rusage rusa;
	int status;
#endif
	int n;

	if (lsp->child_pid < 1)
		return 0;

	/* check if exited, do not reap yet */

	memset(&lsp->si, 0, sizeof(lsp->si));
#if defined(__OpenBSD__) || defined(__NetBSD__)
	n = wait4(lsp->child_pid, &status, WNOHANG, &rusa);
	if (!n)
		return 0;
	lsp->si.si_code = WIFEXITED(status);
#else
	n = waitid(P_PID, (id_t)lsp->child_pid, &lsp->si, WEXITED | WNOHANG | WNOWAIT);
#endif
	if (n < 0) {
		lwsl_info("%s: child %d still running\n", __func__, lsp->child_pid);
		return 0;
	}

	if (!lsp->si.si_code)
		return 0;

	/* his process has exited... */

	if (!lsp->reaped) {
		/* mark the earliest time we knew he had gone */
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
		lwsl_info("%s: %d stdwsi alive, not reaping\n", __func__,
				lsp->pipes_alive);
		return 0;
	}

	/* we reached the reap point, no need for timeout wait */

	lws_sul_cancel(&lsp->sul);

	/*
	 * All the stdwsi went down, nothing more is coming... it's over
	 * Collect the final information and then reap the dead process
	 */

	if (times(&tms) != (clock_t) -1) {
		/*
		 * Cpu accounting in us
		 */
		lsp->accounting[0] = (lws_usec_t)((uint64_t)tms.tms_cstime * 1000000) / hz;
		lsp->accounting[1] = (lws_usec_t)((uint64_t)tms.tms_cutime * 1000000) / hz;
		lsp->accounting[2] = (lws_usec_t)((uint64_t)tms.tms_stime * 1000000) / hz;
		lsp->accounting[3] = (lws_usec_t)((uint64_t)tms.tms_utime * 1000000) / hz;
	}

	temp = *lsp;
#if defined(__OpenBSD__) || defined(__NetBSD__)
	n = wait4(lsp->child_pid, &status, WNOHANG, &rusa);
	if (!n)
		return 0;
	lsp->si.si_code = WIFEXITED(status);
	if (lsp->si.si_code == CLD_EXITED)
		temp.si.si_code = CLD_EXITED;
	temp.si.si_status = WEXITSTATUS(status);
#else
	n = waitid(P_PID, (id_t)lsp->child_pid, &temp.si, WEXITED | WNOHANG);
#endif
	temp.si.si_status &= 0xff; /* we use b8 + for flags */
	lwsl_info("%s: waitd says %d, process exit %d\n",
		    __func__, n, temp.si.si_status);

	lsp->child_pid = -1;

	/* destroy the lsp itself first (it's freed and plsp set NULL */

	if (lsp->info.plsp)
		lws_spawn_piped_destroy(lsp->info.plsp);

	/* then do the parent callback informing it's destroyed */

	if (cb)
		cb(opaque, temp.accounting, &temp.si,
		   temp.we_killed_him_timeout |
			   (temp.we_killed_him_spew << 1));

	return 1; /* was reaped */
}

int
lws_spawn_piped_kill_child_process(struct lws_spawn_piped *lsp)
{
	int status, n;

	if (lsp->child_pid <= 0)
		return 1;

	lsp->ungraceful = 1; /* don't wait for flushing, just kill it */

	if (lws_spawn_reap(lsp))
		/* that may have invalidated lsp */
		return 0;

	/* kill the process group */
	n = kill(-lsp->child_pid, SIGTERM);
	lwsl_debug("%s: SIGTERM child PID %d says %d (errno %d)\n", __func__,
		   lsp->child_pid, n, errno);
	if (n < 0) {
		/*
		 * hum seen errno=3 when process is listed in ps,
		 * it seems we don't always retain process grouping
		 *
		 * Direct these fallback attempt to the exact child
		 */
		n = kill(lsp->child_pid, SIGTERM);
		if (n < 0) {
			n = kill(lsp->child_pid, SIGPIPE);
			if (n < 0) {
				n = kill(lsp->child_pid, SIGKILL);
				if (n < 0)
					lwsl_info("%s: SIGKILL PID %d "
						 "failed errno %d "
						 "(maybe zombie)\n", __func__,
						 lsp->child_pid, errno);
			}
		}
	}

	/* He could be unkillable because he's a zombie */

	n = 1;
	while (n > 0) {
		n = waitpid(-lsp->child_pid, &status, WNOHANG);
		if (n > 0)
			lwsl_debug("%s: reaped PID %d\n", __func__, n);
		if (n <= 0) {
			n = waitpid(lsp->child_pid, &status, WNOHANG);
			if (n > 0)
				lwsl_debug("%s: reaped PID %d\n", __func__, n);
		}
	}

	lws_spawn_reap(lsp);
	/* that may have invalidated lsp */

	return 0;
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
	const char *wd;
	int n, m;

	if (i->protocol_name)
		pcol = lws_vhost_name_to_protocol(i->vh, i->protocol_name);
	if (!pcol) {
		lwsl_err("%s: unknown protocol %s\n", __func__,
			 i->protocol_name ? i->protocol_name : "default");

		return NULL;
	}

	lsp = lws_zalloc(sizeof(*lsp), __func__);
	if (!lsp)
		return NULL;

	/* wholesale take a copy of info */
	lsp->info = *i;
	lsp->reap_retry_budget = 20;

	/*
	 * Prepare the stdin / out / err pipes
	 */

	for (n = 0; n < 3; n++) {
		lsp->pipe_fds[n][0] = -1;
		lsp->pipe_fds[n][1] = -1;
	}

	/* create pipes for [stdin|stdout] and [stderr] */

	for (n = 0; n < 3; n++) {
		if (pipe(lsp->pipe_fds[n]) == -1)
			goto bail1;
		lws_plat_apply_FD_CLOEXEC(lsp->pipe_fds[n][n == 0]);
	}

	/*
	 * At this point, we have 6 pipe fds open on lws side and no wsis
	 * bound to them
	 */

	/* create wsis for each stdin/out/err fd */

	for (n = 0; n < 3; n++) {
		lsp->stdwsi[n] = lws_create_stdwsi(i->vh->context, i->tsi,
					  i->ops ? i->ops : &role_ops_raw_file);
		if (!lsp->stdwsi[n]) {
			lwsl_err("%s: unable to create lsp stdwsi\n", __func__);
			goto bail2;
		}

                __lws_lc_tag(i->vh->context, &i->vh->context->lcg[LWSLCG_WSI],
                	     &lsp->stdwsi[n]->lc, "nspawn-stdwsi-%d", n);

		lsp->stdwsi[n]->lsp_channel = (uint8_t)n;
		lws_vhost_bind_wsi(i->vh, lsp->stdwsi[n]);
		lsp->stdwsi[n]->a.protocol = pcol;
		lsp->stdwsi[n]->a.opaque_user_data = i->opaque;

		lwsl_debug("%s: lsp stdwsi %p: pipe idx %d -> fd %d / %d\n", __func__,
			   lsp->stdwsi[n], n, lsp->pipe_fds[n][n == 0],
			   lsp->pipe_fds[n][n != 0]);

		/* read side is 0, stdin we want the write side, others read */

		lsp->stdwsi[n]->desc.sockfd = lsp->pipe_fds[n][n == 0];
		if (fcntl(lsp->pipe_fds[n][n == 0], F_SETFL, O_NONBLOCK) < 0) {
			lwsl_err("%s: setting NONBLOCK failed\n", __func__);
			goto bail2;
		}

		/*
		 * We have bound 3 x pipe fds to wsis, wr side of stdin and rd
		 * side of stdout / stderr... those are marked CLOEXEC so they
		 * won't go through the fork
		 *
		 * rd side of stdin and wr side of stdout / stderr are open but
		 * not bound to anything on lws side.
		 */
	}

	/*
	 * Stitch the wsi fd into the poll wait
	 */

	for (n = 0; n < 3; n++) {
		if (context->event_loop_ops->sock_accept)
			if (context->event_loop_ops->sock_accept(lsp->stdwsi[n]))
				goto bail3;

		if (__insert_wsi_socket_into_fds(context, lsp->stdwsi[n]))
			goto bail3;
		if (i->opt_parent) {
			lsp->stdwsi[n]->parent = i->opt_parent;
			lsp->stdwsi[n]->sibling_list = i->opt_parent->child_list;
			i->opt_parent->child_list = lsp->stdwsi[n];
		}
	}

	if (lws_change_pollfd(lsp->stdwsi[LWS_STDIN], LWS_POLLIN, LWS_POLLOUT))
		goto bail3;
	if (lws_change_pollfd(lsp->stdwsi[LWS_STDOUT], LWS_POLLOUT, LWS_POLLIN))
		goto bail3;
	if (lws_change_pollfd(lsp->stdwsi[LWS_STDERR], LWS_POLLOUT, LWS_POLLIN))
		goto bail3;

	lwsl_info("%s: fds in %d, out %d, err %d\n", __func__,
		   lsp->stdwsi[LWS_STDIN]->desc.sockfd,
		   lsp->stdwsi[LWS_STDOUT]->desc.sockfd,
		   lsp->stdwsi[LWS_STDERR]->desc.sockfd);

	/* we are ready with the redirection pipes... do the (v)fork */
#if defined(__sun) || !defined(LWS_HAVE_VFORK) || !defined(LWS_HAVE_EXECVPE)
	lsp->child_pid = fork();
#else
	lsp->child_pid = vfork();
#endif
	if (lsp->child_pid < 0) {
		lwsl_err("%s: fork failed, errno %d", __func__, errno);
		goto bail3;
	}

#if defined(__linux__)
	if (!lsp->child_pid)
		prctl(PR_SET_PDEATHSIG, SIGTERM);
#endif

	if (lsp->info.disable_ctrlc)
		/* stops non-daemonized main processess getting SIGINT
		 * from TTY */
#if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
		setpgid(0, 0);
#else
		setpgrp();
#endif

	if (lsp->child_pid) {

		/*
		 * We are the parent process.  We can close our copy of the
		 * "other" side of the pipe fds, ie, rd for stdin and wr for
		 * stdout / stderr.
		 */
		for (n = 0; n < 3; n++)
			/* these guys didn't have any wsi footprint */
			close(lsp->pipe_fds[n][n != 0]);

		lsp->pipes_alive = 3;
		lsp->created = lws_now_usecs();

		lwsl_info("%s: lsp %p spawned PID %d\n", __func__, lsp,
			  lsp->child_pid);

		lws_sul_schedule(context, i->tsi, &lsp->sul, lws_spawn_timeout,
				 i->timeout_us ? i->timeout_us :
						   300 * LWS_US_PER_SEC);

		if (i->owner)
			lws_dll2_add_head(&lsp->dll, i->owner);

		if (i->timeout_us)
			lws_sul_schedule(context, i->tsi, &lsp->sul,
					 lws_spawn_timeout, i->timeout_us);

		return lsp;
	}

	/*
	 * We are the forked process, redirect and kill inherited things.
	 *
	 * Because of vfork(), we cannot do anything that changes pages in
	 * the parent environment.  Stuff that changes kernel state for the
	 * process is OK.  Stuff that happens after the execvpe() is OK.
	 */

	if (i->chroot_path && chroot(i->chroot_path)) {
		lwsl_err("%s: child chroot %s failed, errno %d\n",
			 __func__, i->chroot_path, errno);

		exit(2);
	}

	/* cwd: somewhere we can at least read things and enter it */

	wd = i->wd;
	if (!wd)
		wd = "/tmp";
	if (chdir(wd))
		lwsl_notice("%s: Failed to cd to %s\n", __func__, wd);

	/*
	 * Bind the child's stdin / out / err to its side of our pipes
	 */

	for (m = 0; m < 3; m++) {
		if (dup2(lsp->pipe_fds[m][m != 0], m) < 0) {
			lwsl_err("%s: stdin dup2 failed\n", __func__);
			goto bail3;
		}
		/*
		 * CLOEXEC on the lws-side of the pipe fds should have already
		 * dealt with closing those for the child perspective.
		 *
		 * Now it has done the dup, the child should close its original
		 * copies of its side of the pipes.
		 */

		close(lsp->pipe_fds[m][m != 0]);
	}

#if defined(__sun) || !defined(LWS_HAVE_VFORK) || !defined(LWS_HAVE_EXECVPE)
#if defined(__linux__) || defined(__APPLE__) || defined(__sun)
	m = 0;
	while (i->env_array[m]){
		const char *p = strchr(i->env_array[m], '=');
		int naml = lws_ptr_diff(p, i->env_array[m]);
		char enam[32];

		lws_strnncpy(enam, i->env_array[m], naml, sizeof(enam));
		setenv(enam, p, 1);
		m++;
	}
#endif
	execvp(i->exec_array[0], (char * const *)&i->exec_array[0]);
#else
	execvpe(i->exec_array[0], (char * const *)&i->exec_array[0],
		(char **)&i->env_array[0]);
#endif

	lwsl_err("%s: child exec of %s failed %d\n", __func__, i->exec_array[0],
		 LWS_ERRNO);

	_exit(1);

bail3:

	while (--n >= 0)
		__remove_wsi_socket_from_fds(lsp->stdwsi[n]);
bail2:
	for (n = 0; n < 3; n++)
		if (lsp->stdwsi[n])
			__lws_free_wsi(lsp->stdwsi[n]);

bail1:
	for (n = 0; n < 3; n++) {
		if (lsp->pipe_fds[n][0] >= 0)
			close(lsp->pipe_fds[n][0]);
		if (lsp->pipe_fds[n][1] >= 0)
			close(lsp->pipe_fds[n][1]);
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
