#!/usr/bin/env python3

from gentokenlookup import gentokenlookup

HEADERS = [
    ':authority',
    ':method',
    ':path',
    ':scheme',
    ':status',
    ':host', # for spdy
    ':protocol',
    'expect',
    'host',
    'if-modified-since',
    "te",
    "cookie",
    "http2-settings",
    "server",
    "via",
    "forwarded",
    "x-forwarded-for",
    "x-forwarded-proto",
    "alt-svc",
    "content-length",
    "location",
    "trailer",
    "link",
    "accept-encoding",
    "accept-language",
    "cache-control",
    "user-agent",
    "date",
    "content-type",
    "early-data",
    "sec-websocket-accept",
    "sec-websocket-key",
    "priority",
    # disallowed h1 headers
    'connection',
    'keep-alive',
    'proxy-connection',
    'transfer-encoding',
    'upgrade'
]

if __name__ == '__main__':
    gentokenlookup(HEADERS, 'HD_')
