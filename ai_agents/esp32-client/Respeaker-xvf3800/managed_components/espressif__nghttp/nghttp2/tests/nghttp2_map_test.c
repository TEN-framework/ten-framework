/*
 * nghttp2 - HTTP/2 C Library
 *
 * Copyright (c) 2017 ngtcp2 contributors
 * Copyright (c) 2012 nghttp2 contributors
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
#include "nghttp2_map_test.h"

#include <stdio.h>

#include "munit.h"

#include "nghttp2_map.h"

static const MunitTest tests[] = {
  munit_void_test(test_nghttp2_map),
  munit_void_test(test_nghttp2_map_functional),
  munit_void_test(test_nghttp2_map_each),
  munit_void_test(test_nghttp2_map_clear),
  munit_test_end(),
};

const MunitSuite map_suite = {
  "/map", tests, NULL, 1, MUNIT_SUITE_OPTION_NONE,
};

typedef struct strentry {
  nghttp2_map_key_type key;
  const char *str;
} strentry;

static void strentry_init(strentry *entry, nghttp2_map_key_type key,
                          const char *str) {
  entry->key = key;
  entry->str = str;
}

void test_nghttp2_map(void) {
  strentry foo, FOO, bar, baz, shrubbery;
  nghttp2_map map;
  nghttp2_map_init(&map, nghttp2_mem_default());

  strentry_init(&foo, 1, "foo");
  strentry_init(&FOO, 1, "FOO");
  strentry_init(&bar, 2, "bar");
  strentry_init(&baz, 3, "baz");
  strentry_init(&shrubbery, 4, "shrubbery");

  assert_int(0, ==, nghttp2_map_insert(&map, foo.key, &foo));
  assert_string_equal("foo", ((strentry *)nghttp2_map_find(&map, 1))->str);
  assert_size(1, ==, nghttp2_map_size(&map));

  assert_int(NGHTTP2_ERR_INVALID_ARGUMENT, ==,
             nghttp2_map_insert(&map, FOO.key, &FOO));

  assert_size(1, ==, nghttp2_map_size(&map));
  assert_string_equal("foo", ((strentry *)nghttp2_map_find(&map, 1))->str);

  assert_int(0, ==, nghttp2_map_insert(&map, bar.key, &bar));
  assert_size(2, ==, nghttp2_map_size(&map));

  assert_int(0, ==, nghttp2_map_insert(&map, baz.key, &baz));
  assert_size(3, ==, nghttp2_map_size(&map));

  assert_int(0, ==, nghttp2_map_insert(&map, shrubbery.key, &shrubbery));
  assert_size(4, ==, nghttp2_map_size(&map));

  assert_string_equal("baz", ((strentry *)nghttp2_map_find(&map, 3))->str);

  nghttp2_map_remove(&map, 3);
  assert_size(3, ==, nghttp2_map_size(&map));
  assert_null(nghttp2_map_find(&map, 3));

  nghttp2_map_remove(&map, 1);
  assert_size(2, ==, nghttp2_map_size(&map));
  assert_null(nghttp2_map_find(&map, 1));

  /* Erasing non-existent entry */
  nghttp2_map_remove(&map, 1);
  assert_size(2, ==, nghttp2_map_size(&map));
  assert_null(nghttp2_map_find(&map, 1));

  assert_string_equal("bar", ((strentry *)nghttp2_map_find(&map, 2))->str);
  assert_string_equal("shrubbery",
                      ((strentry *)nghttp2_map_find(&map, 4))->str);

  nghttp2_map_free(&map);
}

static void shuffle(int *a, int n) {
  int i;
  for (i = n - 1; i >= 1; --i) {
    size_t j = (size_t)((double)(i + 1) * rand() / (RAND_MAX + 1.0));
    int t = a[j];
    a[j] = a[i];
    a[i] = t;
  }
}

static int eachfun(void *data, void *ptr) {
  (void)data;
  (void)ptr;

  return 0;
}

#define NUM_ENT 6000
static strentry arr[NUM_ENT];
static int order[NUM_ENT];

void test_nghttp2_map_functional(void) {
  nghttp2_map map;
  int i;
  strentry *ent;

  nghttp2_map_init(&map, nghttp2_mem_default());
  for (i = 0; i < NUM_ENT; ++i) {
    strentry_init(&arr[i], (nghttp2_map_key_type)(i + 1), "foo");
    order[i] = i + 1;
  }
  /* insertion */
  shuffle(order, NUM_ENT);
  for (i = 0; i < NUM_ENT; ++i) {
    ent = &arr[order[i] - 1];
    assert_int(0, ==, nghttp2_map_insert(&map, ent->key, ent));
  }

  assert_size(NUM_ENT, ==, nghttp2_map_size(&map));

  /* traverse */
  nghttp2_map_each(&map, eachfun, NULL);
  /* find */
  shuffle(order, NUM_ENT);
  for (i = 0; i < NUM_ENT; ++i) {
    assert_not_null(nghttp2_map_find(&map, (nghttp2_map_key_type)order[i]));
  }
  /* remove */
  for (i = 0; i < NUM_ENT; ++i) {
    assert_int(0, ==, nghttp2_map_remove(&map, (nghttp2_map_key_type)order[i]));
  }

  /* each (but no op function for testing purpose) */
  for (i = 0; i < NUM_ENT; ++i) {
    strentry_init(&arr[i], (nghttp2_map_key_type)(i + 1), "foo");
  }
  /* insert once again */
  for (i = 0; i < NUM_ENT; ++i) {
    ent = &arr[i];
    assert_int(0, ==, nghttp2_map_insert(&map, ent->key, ent));
  }
  nghttp2_map_each(&map, eachfun, NULL);
  nghttp2_map_free(&map);
}

static int entry_free(void *data, void *ptr) {
  const nghttp2_mem *mem = ptr;

  mem->free(data, NULL);
  return 0;
}

void test_nghttp2_map_each(void) {
  const nghttp2_mem *mem = nghttp2_mem_default();
  strentry *foo = mem->malloc(sizeof(strentry), NULL),
           *bar = mem->malloc(sizeof(strentry), NULL),
           *baz = mem->malloc(sizeof(strentry), NULL),
           *shrubbery = mem->malloc(sizeof(strentry), NULL);
  nghttp2_map map;
  nghttp2_map_init(&map, nghttp2_mem_default());

  strentry_init(foo, 1, "foo");
  strentry_init(bar, 2, "bar");
  strentry_init(baz, 3, "baz");
  strentry_init(shrubbery, 4, "shrubbery");

  nghttp2_map_insert(&map, foo->key, foo);
  nghttp2_map_insert(&map, bar->key, bar);
  nghttp2_map_insert(&map, baz->key, baz);
  nghttp2_map_insert(&map, shrubbery->key, shrubbery);

  nghttp2_map_each(&map, entry_free, (void *)mem);
  nghttp2_map_free(&map);
}

void test_nghttp2_map_clear(void) {
  nghttp2_mem *mem = nghttp2_mem_default();
  nghttp2_map map;
  strentry foo;

  strentry_init(&foo, 1, "foo");

  nghttp2_map_init(&map, mem);

  assert_int(0, ==, nghttp2_map_insert(&map, foo.key, &foo));

  nghttp2_map_clear(&map);

  assert_size(0, ==, nghttp2_map_size(&map));

  nghttp2_map_free(&map);
}
