#!/usr/bin/env python3

def to_enum_hd(k, prefix):
    res = prefix
    for c in k.upper():
        if c == ':' or c == '-':
            res += '_'
            continue
        res += c
    return res

def build_header(headers):
    res = {}
    for k in headers:
        size = len(k)
        if size not in res:
            res[size] = {}
        ent = res[size]
        c = k[-1]
        if c not in ent:
            ent[c] = []
        ent[c].append(k)

    return res

def gen_enum(tokens, prefix):
    print('''\
enum {''')
    for k in sorted(tokens):
        print('''\
  {},'''.format(to_enum_hd(k, prefix)))
    print('''\
  {}MAXIDX,
}};'''.format(prefix))

def gen_index_header(tokens, prefix, comp_fun, return_type, fail_value):
    print('''\
{} lookup_token(const StringRef &name) {{
  switch (name.size()) {{'''.format(return_type))
    b = build_header(tokens)
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
      if ({}("{}"_sr, name, {})) {{
        return {};
      }}'''.format(comp_fun, k[:-1], size - 1, to_enum_hd(k, prefix)))
            print('''\
      break;''')
        print('''\
    }
    break;''')
    print('''\
  }}
  return {};
}}'''.format(fail_value))

def gentokenlookup(tokens, prefix, comp_fun='util::streq', return_type='int', fail_value='-1'):
    gen_enum(tokens, prefix)
    print()
    gen_index_header(tokens, prefix, comp_fun, return_type, fail_value)
