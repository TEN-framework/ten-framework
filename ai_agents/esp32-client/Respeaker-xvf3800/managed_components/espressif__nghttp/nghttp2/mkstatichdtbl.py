#!/usr/bin/env python3
# -*- coding: utf-8 -*-

# This scripts reads static table entries [1] and generates
# nghttp2_hd_static_entry table.  This table is used in
# lib/nghttp2_hd.c.
#
# [1] https://httpwg.org/specs/rfc7541.html

import re, sys

def hd_map_hash(name):
  h = 2166136261

  # FNV hash variant: http://isthe.com/chongo/tech/comp/fnv/
  for c in name:
    h ^= ord(c)
    h *= 16777619
    h &= 0xffffffff

  return h

entries = []
for line in sys.stdin:
    m = re.match(r'(\d+)\s+(\S+)\s+(\S.*)?', line)
    val = m.group(3).strip() if m.group(3) else ''
    entries.append((int(m.group(1)), m.group(2), val))

print('static nghttp2_hd_entry static_table[] = {')
idx = 0
for i, ent in enumerate(entries):
    if entries[idx][1] != ent[1]:
        idx = i
    print('MAKE_STATIC_ENT("{}", "{}", {}, {}u),'\
        .format(ent[1], ent[2], entries[idx][0] - 1, hd_map_hash(ent[1])))
print('};')
