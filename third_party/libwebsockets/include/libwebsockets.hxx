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
 * C++ classes for Secure Streams
 */

#include <map>
#include <set>
#include <list>
#include <string>
#include <vector>
#include <exception>

#include "libwebsockets.h"

class lss;

/*
 * Exception subclass for lss-specific issues
 */

class lssException : public std::exception
{
private:
	std::string details;
public:
	lssException(std::string _details) { details = _details; }
	~lssException() throw() { }
	virtual const char *what() const throw() { return details.c_str(); }
};

typedef struct lssbuf {
	uint8_t				*buf;
	size_t				len;
} lssbuf_t;

class lssAc
{
private:
	struct lwsac			*ac;
	struct lwsac			*iter;
	lssAc() { ac = NULL; }
	~lssAc() { lwsac_free(&ac); }

public:
	void append(lssbuf_t *lb);
	void start(bool atomic);
	int get(lssbuf_t *lb);
};

/*
 * Fixed userdata priv used with ss creation... userdata lives in the lss
 * subclasses' members
 */

class lssPriv
{
public:
	struct lws_ss_handle		*m_ss;
	void				*m_plss;
};

#define userobj_to_lss(uo) ((lss *)(((lssPriv *)userobj)->m_plss))

/*
 * The completion callback... it's called once, and state will be one of
 *
 * LWSSSCS_QOS_ACK_REMOTE:     it completed OK
 * LWSSSCS_DESTROYING:         we didn't complete
 * LWSSSCS_ALL_RETRIES_FAILED:  "
 * LWSSSCS_QOS_NACK_REMOTE:     "
 */

typedef int (*lsscomp_t)(lss *lss, lws_ss_constate_t state, void *arg);

/*
 * Base class for Secure Stream objects
 */

class lss
{
public:
	lss(lws_ctx_t _ctx, std::string _uri, lsscomp_t _comp, bool _psh,
	    lws_sscb_rx rx, lws_sscb_tx tx, lws_sscb_state state);
	virtual ~lss();
	int call_completion(lws_ss_constate_t state);

	lsscomp_t			comp;
	struct lws_ss_handle		*m_ss;
	uint64_t			rxlen;
	lws_usec_t			us_start;

private:
	lws_ctx_t			ctx;
	char				*uri;
	lws_ss_policy_t			pol;
	bool				comp_done;
};

/*
 * Subclass of lss for atomic messages on heap
 */

class lssMsg : public lss
{
public:
	lssMsg(lws_ctx_t _ctx, lsscomp_t _comp, std::string _uri);
	virtual ~lssMsg();
};

/*
 * Subclass of lss for file transactions
 */

class lssFile : public lss
{
public:
	lssFile(lws_ctx_t _ctx, std::string _uri, std::string _path,
		lsscomp_t _comp, bool _psh);
	virtual ~lssFile();
	lws_ss_state_return_t write(const uint8_t *buf, size_t len, int flags);

	std::string			path;

private:
	lws_filefd_type			fd;
	bool				push;
};
