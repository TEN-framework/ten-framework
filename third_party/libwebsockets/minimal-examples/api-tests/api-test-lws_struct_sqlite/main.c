/*
 * lws-api-test-lws_struct-sqlite
 *
 * Written in 2010-2020 by Andy Green <andy@warmcat.com>
 *
 * This file is made available under the Creative Commons CC0 1.0
 * Universal Public Domain Dedication.
 *
 * lws_struct apis are used to serialize and deserialize your C structs and
 * linked-lists in a standardized way that's very modest on memory but
 * convenient and easy to maintain.
 *
 * The API test shows how to serialize and deserialize a struct with a linked-
 * list of child structs in JSON using lws_struct APIs.
 */

#include <libwebsockets.h>

typedef struct teststruct {
	lws_dll2_t		list; /* not directly serialized */

	char 			str1[32];
	const char		*str2;
	uint8_t			u8;
	uint16_t		u16;
	uint32_t		u32;
	uint64_t		u64;
	int32_t			s32;
} teststruct_t;

/*
 * These are the members that we will serialize and deserialize, not every
 * member in the struct (eg, the dll2 list member)
 */

static const lws_struct_map_t lsm_teststruct[] = {
	LSM_CARRAY	(teststruct_t, str1,		"str1"),
	LSM_STRING_PTR  (teststruct_t, str2,		"str2"),
	LSM_UNSIGNED	(teststruct_t, u8,		"u8"),
	LSM_UNSIGNED	(teststruct_t, u16,		"u16"),
	LSM_UNSIGNED	(teststruct_t, u32,		"u32"),
	LSM_UNSIGNED	(teststruct_t, u64,		"u64"),
	LSM_SIGNED	(teststruct_t, s32,		"s32"),
};

static const lws_struct_map_t lsm_schema_apitest[] = {
	LSM_SCHEMA_DLL2	(teststruct_t, list, NULL, lsm_teststruct, "apitest")
};

static const char *test_string =
	"No one would have believed in the last years of the nineteenth "
	"century that this world was being watched keenly and closely by "
	"intelligences greater than man's and yet as mortal as his own; that as "
	"men busied themselves about their various concerns they were "
	"scrutinised and studied, perhaps almost as narrowly as a man with a "
	"microscope might scrutinise the transient creatures that swarm and "
	"multiply in a drop of water.  With infinite complacency men went to "
	"and fro over this globe about their little affairs, serene in their "
	"assurance of their empire over matter. It is possible that the "
	"infusoria under the microscope do the same.  No one gave a thought to "
	"the older worlds of space as sources of human danger, or thought of "
	"them only to dismiss the idea of life upon them as impossible or "
	"improbable.  It is curious to recall some of the mental habits of "
	"those departed days.  At most terrestrial men fancied there might be "
	"other men upon Mars, perhaps inferior to themselves and ready to "
	"welcome a missionary enterprise. Yet across the gulf of space, minds "
	"that are to our minds as ours are to those of the beasts that perish, "
	"intellects vast and cool and unsympathetic, regarded this earth with "
	"envious eyes, and slowly and surely drew their plans against us.  And "
	"early in the twentieth century came the great disillusionment. ";

int main(int argc, const char **argv)
{
	int e = 0, logs = LLL_USER | LLL_ERR | LLL_WARN | LLL_NOTICE;
	struct lws_context_creation_info info;
	struct lws_context *context;
	struct lwsac *ac = NULL;
	lws_dll2_owner_t resown;
	teststruct_t ts, *pts;
	const char *p;
	sqlite3 *db;

	if ((p = lws_cmdline_option(argc, argv, "-d")))
		logs = atoi(p);

	lws_set_log_level(logs, NULL);
	lwsl_user("LWS API selftest: lws_struct SQLite\n");

	memset(&info, 0, sizeof info); /* otherwise uninitialized garbage */
#if defined(LWS_WITH_NETWORK)
	info.port = CONTEXT_PORT_NO_LISTEN;
#endif
	context = lws_create_context(&info);
	if (!context) {
		lwsl_err("lws init failed\n");
		return 1;
	}


	unlink("_lws_apitest.sq3");

	if (lws_struct_sq3_open(context, "_lws_apitest.sq3", 1, &db)) {
		lwsl_err("%s: failed to open table\n", __func__);
		goto bail;
	}

	/* 1. populate the struct */

	memset(&ts, 0, sizeof(ts));

	lws_strncpy(ts.str1, "hello", sizeof(ts.str1));
	ts.str2 = test_string;
	ts.u8 = 1;
	ts.u16 = 512,
	ts.u32 = 0x55aa1234; /* 1437209140, */
	ts.u64 = 0x34abcdef01ull;
	ts.s32 = -1;

	/* add our struct to the dll2 owner list */

	lws_dll2_owner_clear(&resown);
	lws_dll2_add_head(&ts.list, &resown);

	/* gratuitously create the table */

	if (lws_struct_sq3_create_table(db, lsm_schema_apitest)) {
		lwsl_err("%s: Create table failed\n", __func__);
		e++;
		goto done;
	}

	/* serialize the items on the dll2 owner */

	if (lws_struct_sq3_serialize(db, lsm_schema_apitest, &resown, 0)) {
		lwsl_err("%s: Serialize failed\n", __func__);
		e++;
		goto done;
	}

	/* resown should be cleared by deserialize, ac is already NULL */

	lws_dll2_owner_clear(&resown); /* make sure old resown data is gone */

	if (lws_struct_sq3_deserialize(db, NULL, NULL, lsm_schema_apitest,
				       &resown, &ac, 0, 1)) {
		lwsl_err("%s: Deserialize failed\n", __func__);
		e++;
		goto done;
	}

	/* we should have 1 entry in resown now (created into the ac) */

	if (resown.count != 1) {
		lwsl_err("%s: Expected 1 result got %d\n", __func__,
				resown.count);
		e++;
		goto done;
	}

	/*
	 * Convert the pointer to the embedded lws_dll2 into a pointer
	 * to the actual struct with the correct type
	 */

	pts = lws_container_of(lws_dll2_get_head(&resown),
			       teststruct_t, list);

	if (strcmp(pts->str1, "hello") ||
	    strcmp(pts->str2, test_string) ||
	    pts->u8 != 1 ||
	    pts->u16 != 512 ||
	    pts->u32 != 0x55aa1234 ||
	    pts->u64 != 0x34abcdef01ull ||
	    pts->s32 != -1) {
		lwsl_err("%s: unexpected deser values: %s\n", __func__, pts->str1);
		lwsl_err("%s: %s\n", __func__, pts->str2);
		lwsl_err("%s: %u %u %u 0x%llx %d\n", __func__, pts->u8, pts->u16,
			 pts->u32, (unsigned long long)pts->u64, pts->s32);

		e++;
		goto done;
	}

done:
	lwsac_free(&ac);
	lws_struct_sq3_close(&db);


	if (e)
		goto bail;

	lws_context_destroy(context);

	lwsl_user("Completed: PASS\n");

	return 0;

bail:
	lws_context_destroy(context);

	lwsl_user("Completed: FAIL\n");

	return 1;
}
