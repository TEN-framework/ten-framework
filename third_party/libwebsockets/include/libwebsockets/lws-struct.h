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
 */

#if defined(LWS_WITH_STRUCT_SQLITE3)
#include <sqlite3.h>
#endif

typedef enum {
	LSMT_SIGNED,
	LSMT_UNSIGNED,
	LSMT_BOOLEAN,
	LSMT_STRING_CHAR_ARRAY,
	LSMT_STRING_PTR,
	LSMT_LIST,
	LSMT_CHILD_PTR,
	LSMT_SCHEMA,
	LSMT_BLOB_PTR,

} lws_struct_map_type_eum;

typedef struct lejp_collation {
	struct lws_dll2 chunks;
	int len;
	char buf[LEJP_STRING_CHUNK + 1];
} lejp_collation_t;

typedef struct lws_struct_map {
	const char *colname;
	const struct lws_struct_map *child_map;
	lejp_callback lejp_cb;
	size_t ofs;			/* child dll2; points to dll2_owner */
	size_t aux;
	size_t ofs_clist;
	size_t child_map_size;
	lws_struct_map_type_eum type;
} lws_struct_map_t;

typedef int (*lws_struct_args_cb)(void *obj, void *cb_arg);

typedef struct lws_struct_args {
	const lws_struct_map_t *map_st[LEJP_MAX_PARSING_STACK_DEPTH];
	lws_struct_args_cb cb;
	struct lwsac *ac;
	void *cb_arg;
	void *dest;

	size_t dest_len;
	size_t toplevel_dll2_ofs;
	size_t map_entries_st[LEJP_MAX_PARSING_STACK_DEPTH];
	size_t ac_block_size;
	int subtype;

	int top_schema_index;

	/*
	 * temp ac used to collate unknown possibly huge strings before final
	 * allocation and copy
	 */
	struct lwsac *ac_chunks;
	struct lws_dll2_owner chunks_owner;
	size_t chunks_length;
} lws_struct_args_t;

#define LSM_SIGNED(type, name, qname) \
	{ \
	  qname, \
	  NULL, \
	  NULL, \
	  offsetof(type, name), \
	  sizeof ((type *)0)->name, \
	  0, \
	  0, \
	  LSMT_SIGNED \
	}

#define LSM_UNSIGNED(type, name, qname) \
	{ \
	  qname, \
	  NULL, \
	  NULL, \
	  offsetof(type, name), \
	  sizeof ((type *)0)->name, \
	  0, \
	  0, \
	  LSMT_UNSIGNED \
	}

#define LSM_BOOLEAN(type, name, qname) \
	{ \
	  qname, \
	  NULL, \
	  NULL, \
	  offsetof(type, name), \
	  sizeof ((type *)0)->name, \
	  0, \
	  0, \
	  LSMT_BOOLEAN \
	}

#define LSM_CARRAY(type, name, qname) \
	{ \
	  qname, \
	  NULL, \
	  NULL, \
	  offsetof(type, name), \
	  sizeof (((type *)0)->name), \
	  0, \
	  0, \
	  LSMT_STRING_CHAR_ARRAY \
	}

#define LSM_STRING_PTR(type, name, qname) \
	{ \
	  qname, \
	  NULL, \
	  NULL, \
	  offsetof(type, name), \
	  sizeof (((type *)0)->name), \
	  0, \
	  0, \
	  LSMT_STRING_PTR \
	}

#define LSM_LIST(ptype, pname, ctype, cname, lejp_cb, cmap, qname) \
	{ \
	  qname, \
	  cmap, \
	  lejp_cb, \
	  offsetof(ptype, pname), \
	  sizeof (ctype), \
	  offsetof(ctype, cname), \
	  LWS_ARRAY_SIZE(cmap), \
	  LSMT_LIST \
	}

#define LSM_CHILD_PTR(ptype, pname, ctype, lejp_cb, cmap, qname) \
	{ \
	  qname, \
	  cmap, \
	  lejp_cb, \
	  offsetof(ptype, pname), \
	  sizeof (ctype), \
	  0, \
	  LWS_ARRAY_SIZE(cmap), \
	  LSMT_CHILD_PTR \
	}

#define LSM_SCHEMA(ctype, lejp_cb, map, schema_name) \
	{ \
	  schema_name, \
	  map, \
	  lejp_cb, \
	  0, \
	  sizeof (ctype), \
	  0, \
	  LWS_ARRAY_SIZE(map), \
	  LSMT_SCHEMA \
	}

#define LSM_SCHEMA_DLL2(ctype, cdll2mem, lejp_cb, map, schema_name) \
	{ \
	  schema_name, \
	  map, \
	  lejp_cb, \
	  offsetof(ctype, cdll2mem), \
	  sizeof (ctype), \
	  0, \
	  LWS_ARRAY_SIZE(map), \
	  LSMT_SCHEMA \
	}

/*
 * This is just used to create the table schema, it is not part of serialization
 * and deserialization.  Blobs should be accessed separately.
 */

#define LSM_BLOB_PTR(type, blobptr_name, qname) \
	{ \
	  qname, /* JSON item, or sqlite3 column name */ \
	  NULL, \
	  NULL, \
	  offsetof(type, blobptr_name),       /* member that points to blob */ \
	  sizeof (((type *)0)->blobptr_name),       /* size of blob pointer */ \
	  0,		 /* member holding blob len */ \
	  0, /* size of blob length member */ \
	  LSMT_BLOB_PTR \
	}

typedef struct lws_struct_serialize_st {
	const struct lws_dll2 *dllpos;
	const lws_struct_map_t *map;
	const char *obj;
	size_t map_entries;
	size_t map_entry;
	size_t size;
	char subsequent;
	char idt;
} lws_struct_serialize_st_t;

enum {
	LSSERJ_FLAG_PRETTY	= (1 << 0),
	LSSERJ_FLAG_OMIT_SCHEMA = (1 << 1)
};

typedef struct lws_struct_serialize {
	lws_struct_serialize_st_t st[LEJP_MAX_PARSING_STACK_DEPTH];

	size_t offset;
	size_t remaining;

	int sp;
	int flags;
} lws_struct_serialize_t;

typedef enum {
	LSJS_RESULT_CONTINUE,
	LSJS_RESULT_FINISH,
	LSJS_RESULT_ERROR
} lws_struct_json_serialize_result_t;

LWS_VISIBLE LWS_EXTERN int
lws_struct_json_init_parse(struct lejp_ctx *ctx, lejp_callback cb,
			   void *user);

LWS_VISIBLE LWS_EXTERN signed char
lws_struct_schema_only_lejp_cb(struct lejp_ctx *ctx, char reason);

LWS_VISIBLE LWS_EXTERN signed char
lws_struct_default_lejp_cb(struct lejp_ctx *ctx, char reason);

LWS_VISIBLE LWS_EXTERN lws_struct_serialize_t *
lws_struct_json_serialize_create(const lws_struct_map_t *map,
				 size_t map_entries, int flags,
				 const void *ptoplevel);

LWS_VISIBLE LWS_EXTERN void
lws_struct_json_serialize_destroy(lws_struct_serialize_t **pjs);

LWS_VISIBLE LWS_EXTERN lws_struct_json_serialize_result_t
lws_struct_json_serialize(lws_struct_serialize_t *js, uint8_t *buf,
			  size_t len, size_t *written);

typedef struct sqlite3 sqlite3;

LWS_VISIBLE LWS_EXTERN int
lws_struct_sq3_serialize(sqlite3 *pdb, const lws_struct_map_t *schema,
			 lws_dll2_owner_t *owner, uint32_t manual_idx);

LWS_VISIBLE LWS_EXTERN int
lws_struct_sq3_deserialize(sqlite3 *pdb, const char *filter, const char *order,
			   const lws_struct_map_t *schema, lws_dll2_owner_t *o,
			   struct lwsac **ac, int start, int limit);

LWS_VISIBLE LWS_EXTERN int
lws_struct_sq3_create_table(sqlite3 *pdb, const lws_struct_map_t *schema);

LWS_VISIBLE LWS_EXTERN int
lws_struct_sq3_open(struct lws_context *context, const char *sqlite3_path,
		    char create_if_missing, sqlite3 **pdb);

LWS_VISIBLE LWS_EXTERN int
lws_struct_sq3_close(sqlite3 **pdb);

