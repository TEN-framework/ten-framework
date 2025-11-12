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
 *
 * Socks5 Client -related helpers
 */

#include "private-lib-core.h"

int
lws_set_socks(struct lws_vhost *vhost, const char *socks)
{
	char *p_at, *p_colon;
	char user[96];
	char password[96];

	if (!socks)
		return -1;

	vhost->socks_user[0] = '\0';
	vhost->socks_password[0] = '\0';

	p_at = strrchr(socks, '@');
	if (p_at) { /* auth is around */
		if (lws_ptr_diff_size_t(p_at, socks) > (sizeof(user) +
							sizeof(password) - 2)) {
			lwsl_vhost_err(vhost, "auth too long");
			goto bail;
		}

		p_colon = strchr(socks, ':');
		if (p_colon) {
			if (lws_ptr_diff_size_t(p_colon, socks) >
							     sizeof(user) - 1) {
				lwsl_vhost_err(vhost, "user too long");
				goto bail;
			}
			if (lws_ptr_diff_size_t(p_at, p_colon) >
						         sizeof(password) - 1) {
				lwsl_vhost_err(vhost, "pw too long");
				goto bail;
			}

			lws_strncpy(vhost->socks_user, socks,
				    lws_ptr_diff_size_t(p_colon, socks) + 1);
			lws_strncpy(vhost->socks_password, p_colon + 1,
				lws_ptr_diff_size_t(p_at, (p_colon + 1)) + 1);
		}

		lwsl_vhost_info(vhost, " Socks auth, user: %s, password: %s",
				       vhost->socks_user,
				       vhost->socks_password);

		socks = p_at + 1;
	}

	lws_strncpy(vhost->socks_proxy_address, socks,
		    sizeof(vhost->socks_proxy_address));

	p_colon = strchr(vhost->socks_proxy_address, ':');
	if (!p_colon && !vhost->socks_proxy_port) {
		lwsl_vhost_err(vhost, "socks_proxy needs to be address:port");

		return -1;
	}

	if (p_colon) {
		*p_colon = '\0';
		vhost->socks_proxy_port = (unsigned int)atoi(p_colon + 1);
	}

	lwsl_vhost_debug(vhost, "Connections via Socks5 %s:%u",
				vhost->socks_proxy_address,
				vhost->socks_proxy_port);

	return 0;

bail:
	return -1;
}

int
lws_socks5c_generate_msg(struct lws *wsi, enum socks_msg_type type,
			 ssize_t *msg_len)
{
	struct lws_context *context = wsi->a.context;
	struct lws_context_per_thread *pt = &context->pt[(int)wsi->tsi];
	uint8_t *p = pt->serv_buf, *end = &p[context->pt_serv_buf_size];
	ssize_t n, passwd_len;
	short net_num;
	char *cp;

	switch (type) {
	case SOCKS_MSG_GREETING:
		if (lws_ptr_diff(end, p) < 4)
			return 1;
		/* socks version, version 5 only */
		*p++ = SOCKS_VERSION_5;
		/* number of methods */
		*p++ = 2;
		/* username password method */
		*p++ = SOCKS_AUTH_USERNAME_PASSWORD;
		/* no authentication method */
		*p++ = SOCKS_AUTH_NO_AUTH;
		break;

	case SOCKS_MSG_USERNAME_PASSWORD:
		n = (ssize_t)strlen(wsi->a.vhost->socks_user);
		passwd_len = (ssize_t)strlen(wsi->a.vhost->socks_password);

		if (n > 254 || passwd_len > 254)
			return 1;

		if (lws_ptr_diff(end, p) < 3 + n + passwd_len)
			return 1;

		/* the subnegotiation version */
		*p++ = SOCKS_SUBNEGOTIATION_VERSION_1;

		/* length of the user name */
		*p++ = (uint8_t)n;
		/* user name */
		memcpy(p, wsi->a.vhost->socks_user, (size_t)n);
		p += (uint8_t)n;

		/* length of the password */
		*p++ = (uint8_t)passwd_len;

		/* password */
		memcpy(p, wsi->a.vhost->socks_password, (size_t)passwd_len);
		p += passwd_len;
		break;

	case SOCKS_MSG_CONNECT:
		n = (ssize_t)strlen(wsi->stash->cis[CIS_ADDRESS]);

		if (n > 254 || lws_ptr_diff(end, p) < 5 + n + 2)
			return 1;

		cp = (char *)&net_num;

		/* socks version */
		*p++ = SOCKS_VERSION_5;
		/* socks command */
		*p++ = SOCKS_COMMAND_CONNECT;
		/* reserved */
		*p++ = 0;
		/* address type */
		*p++ = SOCKS_ATYP_DOMAINNAME;
		/* length of ---> */
		*p++ = (uint8_t)n;

		/* the address we tell SOCKS proxy to connect to */
		memcpy(p, wsi->stash->cis[CIS_ADDRESS], (size_t)n);
		p += n;

		net_num = (short)htons(wsi->c_port);

		/* the port we tell SOCKS proxy to connect to */
		*p++ = (uint8_t)cp[0];
		*p++ = (uint8_t)cp[1];

		break;

	default:
		return 1;
	}

	*msg_len = lws_ptr_diff(p, pt->serv_buf);

	return 0;
}

int
lws_socks5c_ads_server(struct lws_vhost *vh,
		      const struct lws_context_creation_info *info)
{
	/* socks proxy */
	if (info->socks_proxy_address) {
		/* override for backwards compatibility */
		if (info->socks_proxy_port)
			vh->socks_proxy_port = info->socks_proxy_port;
		lws_set_socks(vh, info->socks_proxy_address);

		return 0;
	}
#ifdef LWS_HAVE_GETENV
	{
		char *p = getenv("socks_proxy");

		if (p && strlen(p) > 0 && strlen(p) < 95)
			lws_set_socks(vh, p);
	}
#endif

	return 0;
}

/*
 * Returns 0 = nothing for caller to do, 1 = return wsi, -1 = goto failed
 */

int
lws_socks5c_greet(struct lws *wsi, const char **pcce)
{
	struct lws_context_per_thread *pt = &wsi->a.context->pt[(int)wsi->tsi];
	ssize_t plen;
	int n;

	/* socks proxy */
	if (!wsi->a.vhost->socks_proxy_port)
		return 0;

	if (lws_socks5c_generate_msg(wsi, SOCKS_MSG_GREETING, &plen)) {
		*pcce = "socks msg too large";
		return -1;
	}
	// lwsl_hexdump_notice(pt->serv_buf, plen);
	n = (int)send(wsi->desc.sockfd, (char *)pt->serv_buf, (size_t)plen,
		      MSG_NOSIGNAL);
	if (n < 0) {
		lwsl_wsi_debug(wsi, "ERROR writing socks greeting");
		*pcce = "socks write failed";
		return -1;
	}

	lws_set_timeout(wsi, PENDING_TIMEOUT_AWAITING_SOCKS_GREETING_REPLY,
			(int)wsi->a.context->timeout_secs);

	lwsi_set_state(wsi, LRS_WAITING_SOCKS_GREETING_REPLY);

	return 1;
}

int
lws_socks5c_handle_state(struct lws *wsi, struct lws_pollfd *pollfd,
			 const char **pcce)
{
	struct lws_context_per_thread *pt = &wsi->a.context->pt[(int)wsi->tsi];
	int conn_mode = 0, pending_timeout = 0;
	ssize_t len;
	int n;

	/* handle proxy hung up on us */

	if (pollfd->revents & LWS_POLLHUP) {
		lwsl_wsi_warn(wsi, "SOCKS fd=%d dead", pollfd->fd);
		*pcce = "socks conn dead";
		return LW5CHS_RET_BAIL3;
	}

	n = (int)recv(wsi->desc.sockfd, (void *)pt->serv_buf,
		 wsi->a.context->pt_serv_buf_size, 0);
	if (n < 0) {
		if (LWS_ERRNO == LWS_EAGAIN) {
			lwsl_wsi_debug(wsi, "SOCKS read EAGAIN, retrying");
			return LW5CHS_RET_RET0;
		}
		lwsl_wsi_err(wsi, "ERROR reading from SOCKS socket");
		*pcce = "socks recv fail";
		return LW5CHS_RET_BAIL3;
	}

	// lwsl_hexdump_warn(pt->serv_buf, n);

	switch (lwsi_state(wsi)) {

	case LRS_WAITING_SOCKS_GREETING_REPLY:
		if (pt->serv_buf[0] != SOCKS_VERSION_5)
			goto socks_reply_fail;

		if (pt->serv_buf[1] == SOCKS_AUTH_NO_AUTH) {
			lwsl_wsi_client(wsi, "SOCKS GR: No Auth Method");
			if (lws_socks5c_generate_msg(wsi, SOCKS_MSG_CONNECT,
						     &len)) {
				lwsl_wsi_err(wsi, "generate connect msg fail");
				goto socks_send_msg_fail;
			}
			conn_mode = LRS_WAITING_SOCKS_CONNECT_REPLY;
			pending_timeout =
			   PENDING_TIMEOUT_AWAITING_SOCKS_CONNECT_REPLY;
			goto socks_send;
		}

		if (pt->serv_buf[1] == SOCKS_AUTH_USERNAME_PASSWORD) {
			lwsl_wsi_client(wsi, "SOCKS GR: User/Pw Method");
			if (lws_socks5c_generate_msg(wsi,
					   SOCKS_MSG_USERNAME_PASSWORD,
					   &len))
				goto socks_send_msg_fail;
			conn_mode = LRS_WAITING_SOCKS_AUTH_REPLY;
			pending_timeout =
			      PENDING_TIMEOUT_AWAITING_SOCKS_AUTH_REPLY;
			goto socks_send;
		}
		goto socks_reply_fail;

	case LRS_WAITING_SOCKS_AUTH_REPLY:
		if (pt->serv_buf[0] != SOCKS_SUBNEGOTIATION_VERSION_1 ||
		    pt->serv_buf[1] !=
				    SOCKS_SUBNEGOTIATION_STATUS_SUCCESS)
			goto socks_reply_fail;

		lwsl_wsi_client(wsi, "SOCKS password OK, sending connect");
		if (lws_socks5c_generate_msg(wsi, SOCKS_MSG_CONNECT, &len)) {
socks_send_msg_fail:
			*pcce = "socks gen msg fail";
			return LW5CHS_RET_BAIL3;
		}
		conn_mode = LRS_WAITING_SOCKS_CONNECT_REPLY;
		pending_timeout =
			   PENDING_TIMEOUT_AWAITING_SOCKS_CONNECT_REPLY;
socks_send:
		// lwsl_hexdump_notice(pt->serv_buf, len);
		n = (int)send(wsi->desc.sockfd, (char *)pt->serv_buf,
			      (size_t)len, MSG_NOSIGNAL);
		if (n < 0) {
			lwsl_wsi_debug(wsi, "ERROR writing to socks proxy");
			*pcce = "socks write fail";
			return LW5CHS_RET_BAIL3;
		}

		lws_set_timeout(wsi, (enum pending_timeout)pending_timeout,
				(int)wsi->a.context->timeout_secs);
		lwsi_set_state(wsi, (lws_wsi_state_t)conn_mode);
		break;

socks_reply_fail:
		lwsl_wsi_err(wsi, "socks reply: v%d, err %d",
			     pt->serv_buf[0], pt->serv_buf[1]);
		*pcce = "socks reply fail";
		return LW5CHS_RET_BAIL3;

	case LRS_WAITING_SOCKS_CONNECT_REPLY:
		if (pt->serv_buf[0] != SOCKS_VERSION_5 ||
		    pt->serv_buf[1] != SOCKS_REQUEST_REPLY_SUCCESS)
			goto socks_reply_fail;

		lwsl_wsi_client(wsi, "socks connect OK");

#if defined(LWS_ROLE_H1) || defined(LWS_ROLE_H2)
		if (lwsi_role_http(wsi) &&
		    lws_hdr_simple_create(wsi, _WSI_TOKEN_CLIENT_PEER_ADDRESS,
					  wsi->a.vhost->socks_proxy_address)) {
			*pcce = "socks connect fail";
			return LW5CHS_RET_BAIL3;
		}
#endif

		wsi->c_port = (uint16_t)wsi->a.vhost->socks_proxy_port;

		/* clear his proxy connection timeout */
		lws_set_timeout(wsi, NO_PENDING_TIMEOUT, 0);
		return LW5CHS_RET_STARTHS;
	default:
		break;
	}

	return LW5CHS_RET_NOTHING;
}
