#!/usr/bin/env python3
from io import StringIO

from gentokenlookup import gentokenlookup

# copied from llhttp.h, and stripped trailing spaces and backslashes.
SRC = '''
  XX(0, DELETE, DELETE)
  XX(1, GET, GET)
  XX(2, HEAD, HEAD)
  XX(3, POST, POST)
  XX(4, PUT, PUT)
  XX(5, CONNECT, CONNECT)
  XX(6, OPTIONS, OPTIONS)
  XX(7, TRACE, TRACE)
  XX(8, COPY, COPY)
  XX(9, LOCK, LOCK)
  XX(10, MKCOL, MKCOL)
  XX(11, MOVE, MOVE)
  XX(12, PROPFIND, PROPFIND)
  XX(13, PROPPATCH, PROPPATCH)
  XX(14, SEARCH, SEARCH)
  XX(15, UNLOCK, UNLOCK)
  XX(16, BIND, BIND)
  XX(17, REBIND, REBIND)
  XX(18, UNBIND, UNBIND)
  XX(19, ACL, ACL)
  XX(20, REPORT, REPORT)
  XX(21, MKACTIVITY, MKACTIVITY)
  XX(22, CHECKOUT, CHECKOUT)
  XX(23, MERGE, MERGE)
  XX(24, MSEARCH, M-SEARCH)
  XX(25, NOTIFY, NOTIFY)
  XX(26, SUBSCRIBE, SUBSCRIBE)
  XX(27, UNSUBSCRIBE, UNSUBSCRIBE)
  XX(28, PATCH, PATCH)
  XX(29, PURGE, PURGE)
  XX(30, MKCALENDAR, MKCALENDAR)
  XX(31, LINK, LINK)
  XX(32, UNLINK, UNLINK)
  XX(33, SOURCE, SOURCE)
'''

if __name__ == '__main__':
    methods = []
    for line in StringIO(SRC):
        line = line.strip()
        if not line.startswith('XX'):
            continue
        _, m, _ = line.split(',', 2)
        methods.append(m.strip())
    gentokenlookup(methods, 'HTTP_')
