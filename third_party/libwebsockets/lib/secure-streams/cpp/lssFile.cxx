/*
 * libwebsockets - small server side websockets and web server implementation
 *
 * Copyright (C) 2020 Andy Green <andy@warmcat.com>
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
 * C++ classes for Secure Streams - file transaction
 */

#include <libwebsockets.hxx>

#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

static lws_ss_state_return_t
lssfile_rx(void *userobj, const uint8_t *buf, size_t len, int flags)
{
	lssFile *lf = (lssFile *)userobj_to_lss(userobj);

	return lf->write(buf, len, flags);
}

static lws_ss_state_return_t
lssfile_tx(void *userobj, lws_ss_tx_ordinal_t ord,uint8_t *buf, size_t *len,
   int *flags)
{
	/*
	 * TODO: we don't know how to send things yet
	 */
	return LWSSSSRET_TX_DONT_SEND;
}

static lws_ss_state_return_t
lssfile_state(void *userobj, void *h_src, lws_ss_constate_t state,
      lws_ss_tx_ordinal_t ack)
{
	lssFile *lf = (lssFile *)userobj_to_lss(userobj);

	lwsl_info("%s: state %s\n", __func__, lws_ss_state_name(state));

	switch (state) {

	/*
	 * These reflect some kind of final disposition for the transaction,
	 * that we want to report along with the completion.  If no other chance
	 * we'll report DESTROYING
	 */

	case LWSSSCS_DESTROYING:
	case LWSSSCS_ALL_RETRIES_FAILED:
	case LWSSSCS_QOS_ACK_REMOTE:
	case LWSSSCS_QOS_NACK_REMOTE:
		lf->call_completion(state);

		if (state == LWSSSCS_DESTROYING) {
			/*
			 * we get DESTROYING because we are already in the
			 * middle of destroying the m_ss, unlink the C++ lss
			 * from the ss handle so it won't recursively try to
			 * destroy it
			 */
			lf->m_ss = NULL;
			delete lf;
		}

		break;
	}

	return LWSSSSRET_OK;
}

lws_ss_state_return_t lssFile::write(const uint8_t *buf, size_t len, int flags)
{
	if (fd == LWS_INVALID_FILE) {

		fd = open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0640);
		if (fd == LWS_INVALID_FILE)
			return LWSSSSRET_DESTROY_ME;
	}

	if (::write(fd, buf, len) != len) {
		close(fd);
		fd = LWS_INVALID_FILE;

		return LWSSSSRET_DESTROY_ME;
	}

	rxlen += len;

	if (flags & LWSSS_FLAG_EOM) {
		close(fd);
		fd = LWS_INVALID_FILE;
	}

	return LWSSSSRET_OK;
}

lssFile::lssFile(lws_ctx_t ctx, std::string uri, std::string _path,
		 lsscomp_t comp, bool _psh) :
	 lss(ctx, uri, comp, _psh, lssfile_rx, lssfile_tx, lssfile_state)
{
	path = _path;
	push = _psh;
	fd = LWS_INVALID_FILE;
}

lssFile::~lssFile()
{
	if (fd == LWS_INVALID_FILE)
		return;

	close(fd);
	fd = LWS_INVALID_FILE;
}
