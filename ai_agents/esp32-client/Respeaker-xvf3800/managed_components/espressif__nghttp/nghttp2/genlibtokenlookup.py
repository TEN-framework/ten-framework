#!/usr/bin/env python3

HEADERS = [
    (':authority', 0),
    (':method', 1),
    (':method', 2),
    (':path', 3),
    (':path', 4),
    (':scheme', 5),
    (':scheme', 6),
    (':status', 7),
    (':status', 8),
    (':status', 9),
    (':status', 10),
    (':status', 11),
    (':status', 12),
    (':status', 13),
    ('accept-charset', 14),
    ('accept-encoding', 15),
    ('accept-language', 16),
    ('accept-ranges', 17),
    ('accept', 18),
    ('access-control-allow-origin', 19),
    ('age', 20),
    ('allow', 21),
    ('authorization', 22),
    ('cache-control', 23),
    ('content-disposition', 24),
    ('content-encoding', 25),
    ('content-language', 26),
    ('content-length', 27),
    ('content-location', 28),
    ('content-range', 29),
    ('content-type', 30),
    ('cookie', 31),
    ('date', 32),
    ('etag', 33),
    ('expect', 34),
    ('expires', 35),
    ('from', 36),
    ('host', 37),
    ('if-match', 38),
    ('if-modified-since', 39),
    ('if-none-match', 40),
    ('if-range', 41),
    ('if-unmodified-since', 42),
    ('last-modified', 43),
    ('link', 44),
    ('location', 45),
    ('max-forwards', 46),
    ('proxy-authenticate', 47),
    ('proxy-authorization', 48),
    ('range', 49),
    ('referer', 50),
    ('refresh', 51),
    ('retry-after', 52),
    ('server', 53),
    ('set-cookie', 54),
    ('strict-transport-security', 55),
    ('transfer-encoding', 56),
    ('user-agent', 57),
    ('vary', 58),
    ('via', 59),
    ('www-authenticate', 60),
    ('te', None),
    ('connection', None),
    ('keep-alive',None),
    ('proxy-connection', None),
    ('upgrade', None),
    (':protocol', None),
    ('priority', None),
]

def to_enum_hd(k):
    res = 'NGHTTP2_TOKEN_'
    for c in k.upper():
        if c == ':' or c == '-':
            res += '_'
            continue
        res += c
    return res

def build_header(headers):
    res = {}
    for k, _ in headers:
        size = len(k)
        if size not in res:
            res[size] = {}
        ent = res[size]
        c = k[-1]
        if c not in ent:
            ent[c] = []
        if k not in ent[c]:
            ent[c].append(k)

    return res

def gen_enum():
    name = ''
    print('typedef enum {')
    for k, token in HEADERS:
        if token is None:
            print('  {},'.format(to_enum_hd(k)))
        else:
            if name != k:
                name = k
                print('  {} = {},'.format(to_enum_hd(k), token))
    print('} nghttp2_token;')

def gen_index_header():
    print('''\
static int32_t lookup_token(const uint8_t *name, size_t namelen) {
  switch (namelen) {''')
    b = build_header(HEADERS)
    for size in sorted(b.keys()):
        ents = b[size]
        print('''\
  case {}:'''.format(size))
        print('''\
    switch (name[{}]) {{'''.format(size - 1))
        for c in sorted(ents.keys()):
            headers = sorted(ents[c])
            print('''\
    case '{}':'''.format(c))
            for k in headers:
                print('''\
      if (memeq("{}", name, {})) {{
        return {};
      }}'''.format(k[:-1], size - 1, to_enum_hd(k)))
            print('''\
      break;''')
        print('''\
    }
    break;''')
    print('''\
  }
  return -1;
}''')

if __name__ == '__main__':
    gen_enum()
    print()
    gen_index_header()
