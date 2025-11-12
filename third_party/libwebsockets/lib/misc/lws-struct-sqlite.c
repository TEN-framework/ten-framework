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

#include <libwebsockets.h>
#include <private-lib-core.h>

#include <sqlite3.h>

/*
 * we get one of these per matching result from the query
 */

static int
lws_struct_sq3_deser_cb(void *priv, int cols, char **cv, char **cn)
{
	lws_struct_args_t *a = (lws_struct_args_t *)priv;
	char *u = lwsac_use_zero(&a->ac, a->dest_len, a->ac_block_size);
	lws_dll2_owner_t *o = (lws_dll2_owner_t *)a->cb_arg;
	const lws_struct_map_t *map = a->map_st[0];
	int n, mems = (int)(ssize_t)a->map_entries_st[0];
	long long li;
	size_t lim;
	char **pp;
	char *s;

	if (!u) {
		lwsl_err("OOM\n");

		return 1;
	}

	lws_dll2_add_tail((lws_dll2_t *)((char *)u + a->toplevel_dll2_ofs), o);

	while (mems--) {
		for (n = 0; n < cols; n++) {
			if (!cv[n] || strcmp(cn[n], map->colname))
				continue;

			switch (map->type) {
			case LSMT_SIGNED:
				if (map->aux == sizeof(signed char)) {
					signed char *pc;
					pc = (signed char *)(u + map->ofs);
					*pc = (signed char)atoi(cv[n]);
					break;
				}
				if (map->aux == sizeof(short)) {
					short *ps;
					ps = (short *)(u + map->ofs);
					*ps = (short)atoi(cv[n]);
					break;
				}
				if (map->aux == sizeof(int)) {
					int *pi;
					pi = (int *)(u + map->ofs);
					*pi = (int)atoll(cv[n]); /* 32-bit OS */
					break;
				}
				if (map->aux == sizeof(long)) {
					long *pl;
					pl = (long *)(u + map->ofs);
					*pl = (long)atoll(cv[n]); /* 32-bit OS */
					break;
				}
				{
					long long *pll;
					pll = (long long *)(u + map->ofs);
					*pll = atoll(cv[n]);
				}
				break;

			case LSMT_UNSIGNED:
				if (map->aux == sizeof(unsigned char)) {
					unsigned char *pc;
					pc = (unsigned char *)(u + map->ofs);
					*pc = (unsigned char)(unsigned int)atoi(cv[n]);
					break;
				}
				if (map->aux == sizeof(unsigned short)) {
					unsigned short *ps;
					ps = (unsigned short *)(u + map->ofs);
					*ps = (unsigned short)atoi(cv[n]);
					break;
				}
				if (map->aux == sizeof(unsigned int)) {
					unsigned int *pi;
					pi = (unsigned int *)(u + map->ofs);
					*pi = (unsigned int)atoi(cv[n]);
					break;
				}
				if (map->aux == sizeof(unsigned long)) {
					unsigned long *pl;
					pl = (unsigned long *)(u + map->ofs);
					*pl = (unsigned long)atol(cv[n]);
					break;
				}
				{
					unsigned long long *pll;
					pll = (unsigned long long *)(u + map->ofs);
					*pll = (unsigned long long)atoll(cv[n]);
				}
				break;

			case LSMT_BOOLEAN:
				li = 0;
				if (!strcmp(cv[n], "true") ||
				    !strcmp(cv[n], "TRUE") || cv[n][0] == '1')
					li = 1;
				if (map->aux == sizeof(char)) {
					char *pc;
					pc = (char *)(u + map->ofs);
					*pc = (char)li;
					break;
				}
				if (map->aux == sizeof(int)) {
					int *pi;
					pi = (int *)(u + map->ofs);
					*pi = (int)li;
				} else {
					uint64_t *p64;
					p64 = (uint64_t *)(u + map->ofs);
					*p64 = (uint64_t)li;
				}
				break;

			case LSMT_STRING_CHAR_ARRAY:
				s = (char *)(u + map->ofs);
				lim = map->aux;
				lws_strncpy(s, cv[n], lim);
				break;

			case LSMT_STRING_PTR:
				pp = (char **)(u + map->ofs);
				lim = strlen(cv[n]);
				s = lwsac_use(&a->ac, lim + 1, a->ac_block_size);
				if (!s)
					return 1;
				*pp = s;
				memcpy(s, cv[n], lim);
				s[lim] = '\0';
				break;
			default:
				break;
			}
		}
		map++;
	}

	return 0;
}

/*
 * Call this with an LSM_SCHEMA map, its colname is the table name and its
 * type information describes the toplevel type.  Schema is dereferenced and
 * put in args before the actual sq3 query, which is given the child map.
 */

int
lws_struct_sq3_deserialize(sqlite3 *pdb, const char *filter, const char *order,
			   const lws_struct_map_t *schema, lws_dll2_owner_t *o,
			   struct lwsac **ac, int start, int _limit)
{
	int limit = _limit < 0 ? -_limit : _limit;
	char s[768], results[512], where[250];
	lws_struct_args_t a;
	int n, m;

	if (!order)
		order = "_lws_idx";

	memset(&a, 0, sizeof(a));
	a.ac = *ac;
	a.cb_arg = o; /* lws_dll2_owner tracking query result objects */
	a.map_st[0]  = schema->child_map;
	a.map_entries_st[0] = schema->child_map_size;
	a.dest_len = schema->aux; /* size of toplevel object to allocate */
	a.toplevel_dll2_ofs = schema->ofs;

	lws_dll2_owner_clear(o);

	/*
	 * Explicitly list the columns instead of use *, so we can skip blobs
	 */

	m = 0;
	for (n = 0; n < (int)schema->child_map_size; n++)
		m += lws_snprintf(&results[m], sizeof(results) - (unsigned int)n - 1,
				  "%s%c", schema->child_map[n].colname,
				  n + 1 == (int)schema->child_map_size ? ' ' : ',');

	where[0] = '\0';
	lws_snprintf(where, sizeof(where), " where _lws_idx >= %llu %s",
			     (unsigned long long)start, filter ? filter : "");

	lws_snprintf(s, sizeof(s) - 1, "select %s "
		     "from %s %s order by %s %slimit %d;", results,
		     schema->colname, where, order,
				     _limit < 0 ? "desc " : "", limit);



	if (sqlite3_exec(pdb, s, lws_struct_sq3_deser_cb, &a, NULL) != SQLITE_OK) {
		lwsl_err("%s: %s: fail %s\n", __func__, sqlite3_errmsg(pdb), s);
		lwsac_free(&a.ac);
		return -1;
	}

	*ac = a.ac;

	return 0;
}

/*
 * This takes a struct and turns it into an sqlite3 UPDATE, using the given
 * schema... which has one LSM_SCHEMA_DLL2 entry wrapping the actual schema
 */

static int
_lws_struct_sq3_ser_one(sqlite3 *pdb, const lws_struct_map_t *schema,
			uint32_t idx, void *st)
{
	const lws_struct_map_t *map = schema->child_map;
	int n, m, pk = 0, nentries = (int)(ssize_t)schema->child_map_size, nef = 0, did;
	size_t sql_est = 46 + strlen(schema->colname) + 1;
		/* "insert into  (_lws_idx, ) values (00000001,);" ...
		 * plus the table name */
	uint8_t *stb = (uint8_t *)st;
	const char *p;
	char *sql;

	/*
	 * Figure out effective number of columns, exluding BLOB.
	 *
	 * The first UNSIGNED is a hidden index.  Blobs are not handled by
	 * lws_struct except to create the column in the schema.
	 */

	pk = 0;
	nef = 0;
	for (n = 0; n < nentries; n++) {
		if (!pk && map[n].type == LSMT_UNSIGNED) {
			pk = 1;
			continue;
		}
		if (map[n].type == LSMT_BLOB_PTR)
			continue;

		nef++;
	}

	/*
	 * Figure out an estimate for the length of the populated sqlite
	 * command, and then malloc it up
	 */

	for (n = 0; n < nentries; n++) {
		sql_est += strlen(map[n].colname) + 2;
		switch (map[n].type) {
		case LSMT_SIGNED:
		case LSMT_UNSIGNED:
		case LSMT_BOOLEAN:

			switch (map[n].aux) {
			case 1:
				sql_est += 3 + 2;
				break;
			case 2:
				sql_est += 5 + 2;
				break;
			case 4:
				sql_est += 10 + 2;
				break;
			case 8:
				sql_est += 20 + 2;
				break;
			}

			if (map[n].type == LSMT_SIGNED)
				sql_est++; /* minus sign */

			break;
		case LSMT_STRING_CHAR_ARRAY:
			sql_est += (unsigned int)lws_sql_purify_len((const char *)st +
							map[n].ofs) + 2;
			break;

		case LSMT_STRING_PTR:
			p = *((const char * const *)&stb[map[n].ofs]);
			sql_est += (unsigned int)((p ? lws_sql_purify_len(p) : 0) + 2);
			break;

		case LSMT_BLOB_PTR:
			/* we don't deal with blobs actually */
			sql_est -= strlen(map[n].colname) + 2;
			break;

		default:
			lwsl_err("%s: unsupported type\n", __func__);
			assert(0);
			break;
		}
	}

	sql = malloc(sql_est);
	if (!sql)
		return -1;

	m = lws_snprintf(sql, sql_est, "insert into %s(_lws_idx, ",
			 schema->colname);

	/*
	 * First explicit integer type is primary key autoincrement, should
	 * not be specified
	 */

	pk = 0;
	did = 0;
	for (n = 0; n < nentries; n++) {
		if (!pk && map[n].type == LSMT_UNSIGNED) {
			pk = 1;
			continue;
		}
		if (map[n].type == LSMT_BLOB_PTR)
			continue;

		did++;
		m += lws_snprintf(sql + m, sql_est - (unsigned int)m,
				  did == nef ? "%s" : "%s, ",
				  map[n].colname);
	}

	m += lws_snprintf(sql + m, sql_est - (unsigned int)m, ") values(%u, ", idx);

	pk = 0;
	did = 0;
	for (n = 0; n < nentries; n++) {
		uint64_t uu64;
		size_t q;

		if (!pk && map[n].type == LSMT_UNSIGNED) {
			pk = 1;
			continue;
		}

		switch (map[n].type) {
		case LSMT_SIGNED:
		case LSMT_UNSIGNED:
		case LSMT_BOOLEAN:

			uu64 = 0;
			for (q = 0; q < map[n].aux; q++)
				uu64 |= ((uint64_t)stb[map[n].ofs + q] <<
								(q << 3));

			if (map[n].type == LSMT_SIGNED)
				m += lws_snprintf(sql + m, sql_est - (unsigned int)m, "%lld",
						  (long long)(int64_t)uu64);
			else
				m += lws_snprintf(sql + m, sql_est - (unsigned int)m, "%llu",
						  (unsigned long long)uu64);
			break;

		case LSMT_STRING_CHAR_ARRAY:
			sql[m++] = '\'';
			lws_sql_purify(sql + m, (const char *)&stb[map[n].ofs],
				       sql_est - (size_t)(ssize_t)m - 4);
			m += (int)(ssize_t)strlen(sql + m);
			sql[m++] = '\'';
			break;
		case LSMT_STRING_PTR:
			p = *((const char * const *)&stb[map[n].ofs]);
			sql[m++] = '\'';
			if (p) {
				lws_sql_purify(sql + m, p, sql_est - (unsigned int)m - 4);
				m += (int)(ssize_t)strlen(sql + m);
			}
			sql[m++] = '\'';
			break;

		case LSMT_BLOB_PTR:
			continue;

		default:
			lwsl_err("%s: unsupported type\n", __func__);
			assert(0);
			break;
		}

		did++;
		if (did != nef) {
			if (sql_est - (unsigned int)m < 6)
				return -1;
			sql[m++] = ',';
			sql[m++] = ' ';
		}
	}

	lws_snprintf(sql + m, sql_est - (unsigned int)m, ");");

	n = sqlite3_exec(pdb, sql, NULL, NULL, NULL);
	if (n != SQLITE_OK) {
		lwsl_err("%s\n", sql);
		free(sql);
		lwsl_err("%s: %s: fail\n", __func__, sqlite3_errmsg(pdb));
		return -1;
	}
	free(sql);

	return 0;
}

int
lws_struct_sq3_serialize(sqlite3 *pdb, const lws_struct_map_t *schema,
			 lws_dll2_owner_t *owner, uint32_t manual_idx)
{
	uint32_t idx = manual_idx;

	lws_start_foreach_dll(struct lws_dll2 *, p, owner->head) {
		void *item = (void *)((uint8_t *)p - schema->ofs_clist);
		if (_lws_struct_sq3_ser_one(pdb, schema, idx++, item))
			return 1;

	} lws_end_foreach_dll(p);

	return 0;
}

int
lws_struct_sq3_create_table(sqlite3 *pdb, const lws_struct_map_t *schema)
{
	const lws_struct_map_t *map = schema->child_map;
	int map_size = (int)(ssize_t)schema->child_map_size, subsequent = 0;
	char s[2048], *p = s, *end = &s[sizeof(s) - 1],
	     *pri = " primary key autoincrement", *use;

	p += lws_snprintf(p, (unsigned int)lws_ptr_diff(end, p),
			  "create table if not exists %s (_lws_idx integer, ",
			  schema->colname);

	while (map_size--) {
		if (map->type > LSMT_STRING_PTR && map->type != LSMT_BLOB_PTR) {
			map++;
			continue;
		}
		if (subsequent && (end - p) > 4) {
			*p++ = ',';
			*p++ = ' ';
		}
		subsequent = 1;
		if (map->type == LSMT_BLOB_PTR) {

			p += lws_snprintf(p, (unsigned int)lws_ptr_diff(end, p), "%s blob", map->colname);

		} else {
			if (map->type < LSMT_STRING_CHAR_ARRAY) {
				use = "";
				if (map->colname[0] != '_') /* _lws_idx is not primary key */
					use = pri;
				p += lws_snprintf(p, (unsigned int)lws_ptr_diff(end, p), "%s integer%s",
						map->colname, use);
				if (map->colname[0] != '_')
					pri = "";
			} else
				p += lws_snprintf(p, (unsigned int)lws_ptr_diff(end, p), "%s varchar",
						map->colname);
		}

		map++;
	}

	p += lws_snprintf(p, (unsigned int)lws_ptr_diff(end, p), ");");

	if (sqlite3_exec(pdb, s, NULL, NULL, NULL) != SQLITE_OK) {
		lwsl_err("%s: %s: fail\n", __func__, sqlite3_errmsg(pdb));

		return -1;
	}

	return 0;
}

int
lws_struct_sq3_open(struct lws_context *context, const char *sqlite3_path,
		    char create_if_missing, sqlite3 **pdb)
{
#if !defined(WIN32)
	uid_t uid = 0;
	gid_t gid = 0;
#endif

	if (sqlite3_open_v2(sqlite3_path, pdb,
			    SQLITE_OPEN_READWRITE |
			    (create_if_missing ? SQLITE_OPEN_CREATE : 0),
			    NULL) != SQLITE_OK) {
		lwsl_info("%s: Unable to open db %s: %s\n",
			 __func__, sqlite3_path, sqlite3_errmsg(*pdb));

		return 1;
	}

#if !defined(WIN32)
	lws_get_effective_uid_gid(context, &uid, &gid);
	if (uid)
		if (chown(sqlite3_path, uid, gid))
			lwsl_err("%s: failed to chown %s\n", __func__, sqlite3_path);
	chmod(sqlite3_path, 0600);

	lwsl_debug("%s: created %s owned by %u:%u mode 0600\n", __func__,
			sqlite3_path, (unsigned int)uid, (unsigned int)gid);
#else
	lwsl_debug("%s: created %s\n", __func__, sqlite3_path);
#endif
	sqlite3_extended_result_codes(*pdb, 1);

	return 0;
}

int
lws_struct_sq3_close(sqlite3 **pdb)
{
	int n;

	if (!*pdb)
		return 0;

	n = sqlite3_close(*pdb);
	if (n != SQLITE_OK) {
		/*
		 * trouble...
		 */
		lwsl_err("%s: failed to close: %d\n", __func__, n);
		return 1;
	}
	*pdb = NULL;

	return 0;
}
