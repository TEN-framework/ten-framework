/*
 * nghttp2 - HTTP/2 C Library
 *
 * Copyright (c) 2012 Tatsuhiro Tsujikawa
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#include "nghttp2_pq_test.h"

#include <stdio.h>

#include "nghttp2_pq.h"

static const MunitTest tests[] = {
  munit_void_test(test_nghttp2_pq),
  munit_void_test(test_nghttp2_pq_update),
  munit_void_test(test_nghttp2_pq_remove),
  munit_test_end(),
};

const MunitSuite pq_suite = {
  "/pq", tests, NULL, 1, MUNIT_SUITE_OPTION_NONE,
};

typedef struct {
  nghttp2_pq_entry ent;
  const char *s;
} string_entry;

static string_entry *string_entry_new(const char *s) {
  nghttp2_mem *mem;
  string_entry *ent;

  mem = nghttp2_mem_default();

  ent = nghttp2_mem_malloc(mem, sizeof(string_entry));
  ent->s = s;

  return ent;
}

static void string_entry_del(string_entry *ent) { free(ent); }

static int pq_less(const void *lhs, const void *rhs) {
  return strcmp(((string_entry *)lhs)->s, ((string_entry *)rhs)->s) < 0;
}

void test_nghttp2_pq(void) {
  int i;
  nghttp2_pq pq;
  string_entry *top;

  nghttp2_pq_init(&pq, pq_less, nghttp2_mem_default());
  assert_true(nghttp2_pq_empty(&pq));
  assert_size(0, ==, nghttp2_pq_size(&pq));
  assert_int(0, ==, nghttp2_pq_push(&pq, &string_entry_new("foo")->ent));
  assert_false(nghttp2_pq_empty(&pq));
  assert_size(1, ==, nghttp2_pq_size(&pq));
  top = (string_entry *)nghttp2_pq_top(&pq);
  assert_string_equal("foo", top->s);
  assert_int(0, ==, nghttp2_pq_push(&pq, &string_entry_new("bar")->ent));
  top = (string_entry *)nghttp2_pq_top(&pq);
  assert_string_equal("bar", top->s);
  assert_int(0, ==, nghttp2_pq_push(&pq, &string_entry_new("baz")->ent));
  top = (string_entry *)nghttp2_pq_top(&pq);
  assert_string_equal("bar", top->s);
  assert_int(0, ==, nghttp2_pq_push(&pq, &string_entry_new("C")->ent));
  assert_size(4, ==, nghttp2_pq_size(&pq));

  top = (string_entry *)nghttp2_pq_top(&pq);
  assert_string_equal("C", top->s);
  string_entry_del(top);
  nghttp2_pq_pop(&pq);

  assert_size(3, ==, nghttp2_pq_size(&pq));

  top = (string_entry *)nghttp2_pq_top(&pq);
  assert_string_equal("bar", top->s);
  nghttp2_pq_pop(&pq);
  string_entry_del(top);

  top = (string_entry *)nghttp2_pq_top(&pq);
  assert_string_equal("baz", top->s);
  nghttp2_pq_pop(&pq);
  string_entry_del(top);

  top = (string_entry *)nghttp2_pq_top(&pq);
  assert_string_equal("foo", top->s);
  nghttp2_pq_pop(&pq);
  string_entry_del(top);

  assert_true(nghttp2_pq_empty(&pq));
  assert_size(0, ==, nghttp2_pq_size(&pq));
  assert_null(nghttp2_pq_top(&pq));

  /* Add bunch of entry to see realloc works */
  for (i = 0; i < 10000; ++i) {
    assert_int(0, ==, nghttp2_pq_push(&pq, &string_entry_new("foo")->ent));
    assert_size((size_t)(i + 1), ==, nghttp2_pq_size(&pq));
  }
  for (i = 10000; i > 0; --i) {
    top = (string_entry *)nghttp2_pq_top(&pq);
    assert_not_null(top);
    nghttp2_pq_pop(&pq);
    string_entry_del(top);
    assert_size((size_t)(i - 1), ==, nghttp2_pq_size(&pq));
  }

  nghttp2_pq_free(&pq);
}

typedef struct {
  nghttp2_pq_entry ent;
  int key;
  int val;
} node;

static int node_less(const void *lhs, const void *rhs) {
  node *ln = (node *)lhs;
  node *rn = (node *)rhs;
  return ln->key < rn->key;
}

static int node_update(nghttp2_pq_entry *item, void *arg) {
  node *nd = (node *)item;
  (void)arg;

  if ((nd->key % 2) == 0) {
    nd->key *= -1;
    return 1;
  } else {
    return 0;
  }
}

void test_nghttp2_pq_update(void) {
  nghttp2_pq pq;
  node nodes[10];
  int i;
  node *nd;
  int ans[] = {-8, -6, -4, -2, 0, 1, 3, 5, 7, 9};

  nghttp2_pq_init(&pq, node_less, nghttp2_mem_default());

  for (i = 0; i < (int)(sizeof(nodes) / sizeof(nodes[0])); ++i) {
    nodes[i].key = i;
    nodes[i].val = i;
    nghttp2_pq_push(&pq, &nodes[i].ent);
  }

  nghttp2_pq_update(&pq, node_update, NULL);

  for (i = 0; i < (int)(sizeof(nodes) / sizeof(nodes[0])); ++i) {
    nd = (node *)nghttp2_pq_top(&pq);
    assert_int(ans[i], ==, nd->key);
    nghttp2_pq_pop(&pq);
  }

  nghttp2_pq_free(&pq);
}

static void push_nodes(nghttp2_pq *pq, node *dest, size_t n) {
  size_t i;
  for (i = 0; i < n; ++i) {
    dest[i].key = (int)i;
    dest[i].val = (int)i;
    nghttp2_pq_push(pq, &dest[i].ent);
  }
}

static void check_nodes(nghttp2_pq *pq, size_t n, int *ans_key, int *ans_val) {
  size_t i;
  for (i = 0; i < n; ++i) {
    node *nd = (node *)nghttp2_pq_top(pq);
    assert_int(ans_key[i], ==, nd->key);
    assert_int(ans_val[i], ==, nd->val);
    nghttp2_pq_pop(pq);
  }
}

void test_nghttp2_pq_remove(void) {
  nghttp2_pq pq;
  node nodes[10];
  int ans_key1[] = {1, 2, 3, 4, 5};
  int ans_val1[] = {1, 2, 3, 4, 5};
  int ans_key2[] = {0, 1, 2, 4, 5};
  int ans_val2[] = {0, 1, 2, 4, 5};
  int ans_key3[] = {0, 1, 2, 3, 4};
  int ans_val3[] = {0, 1, 2, 3, 4};

  nghttp2_pq_init(&pq, node_less, nghttp2_mem_default());

  push_nodes(&pq, nodes, 6);

  nghttp2_pq_remove(&pq, &nodes[0].ent);

  check_nodes(&pq, 5, ans_key1, ans_val1);

  nghttp2_pq_free(&pq);

  nghttp2_pq_init(&pq, node_less, nghttp2_mem_default());

  push_nodes(&pq, nodes, 6);

  nghttp2_pq_remove(&pq, &nodes[3].ent);

  check_nodes(&pq, 5, ans_key2, ans_val2);

  nghttp2_pq_free(&pq);

  nghttp2_pq_init(&pq, node_less, nghttp2_mem_default());

  push_nodes(&pq, nodes, 6);

  nghttp2_pq_remove(&pq, &nodes[5].ent);

  check_nodes(&pq, 5, ans_key3, ans_val3);

  nghttp2_pq_free(&pq);
}
