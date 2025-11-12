/*
 * lws-api-test-lws_struct-json
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
 * This second test file shows a worked example for how to express a schema
 * and both consume JSON -> struct and struct -> JSON for it.
 */

#include <libwebsockets.h>

static const char * const test2_json =
"{"
	"\"config\":["
		"{"
			"\"id1\":"		"null,"
			"\"creds\":{"
				"\"key1\":"	"\"\\\"xxxxxxxxx\\\"\","
				"\"key2\":"	"null"
			"},"
			"\"frequency\":"	"0,"
			"\"arg1\":"		"\"val1\","
			"\"arg2\":"		"0,"
			"\"priority\":"		"1,"
			"\"ssid\":"		"\"\\\"nw2\\\"\""
		"}, {"
			"\"id2\":"		"null,"
			"\"creds\": {"
				"\"key1\":"	"\"\\\"xxxxxxxxxxxxx\\\"\","
				"\"key2\":"	"null"
			"},"
			"\"frequency\":"	"11,"
			"\"arg1\":"		"\"val2\","
			"\"arg2\":"		"1420887242594,"
			"\"priority\":"		"3,"
			"\"ssid\":"		"\"\\\"nw1\\\"\""
		"}"
	"]"
"}";

static const char * const test2_json_expected =
	"{\"config\":[{\"creds\":{\"key1\":\"\\u0022xxxxxxxxx\\u0022\"},"
	 "\"arg1\":\"val1\",\"ssid\":\"\\u0022nw2\\u0022\","
	 "\"frequency\":0,\"arg2\":0,\"priority\":1},"
	 "{\"creds\":{\"key1\":\"\\u0022xxxxxxxxxxxxx\\u0022\"},"
	 "\"arg1\":\"val2\",\"ssid\":\"\\u0022nw1\\u0022\","
	 "\"frequency\":11,\"arg2\":1420887242594,\"priority\":3}]}"
;

/*
 * level 3: Credentials object
 */

typedef struct t2_cred {
	const char				*key1;
	const char				*key2;
} t2_cred_t;

static const lws_struct_map_t lsm_t2_cred[] = {
	LSM_STRING_PTR	(t2_cred_t, key1, "key1"),
	LSM_STRING_PTR	(t2_cred_t, key2, "key2"),
};

/*
 * level 2: Configuration object, containing a child credentials object
 */

typedef struct t2_config {
	lws_dll2_t				list;
	t2_cred_t 				*creds;
	const char				*id1;
	const char				*arg1;
	const char				*ssid;
	unsigned int				frequency;
	unsigned long long			arg2;
	unsigned int				priority;
} t2_config_t;

static const lws_struct_map_t lsm_t2_config[] = {
	LSM_CHILD_PTR	(t2_config_t,
			 creds,			/* the child pointer member */
			 t2_cred_t,		/* the child type */
			 NULL, lsm_t2_cred,	/* map object for item type */
			 "creds"),		/* outer json object name */
	LSM_STRING_PTR	(t2_config_t, id1, 	 "id1"),
	LSM_STRING_PTR	(t2_config_t, arg1,	 "arg1"),
	LSM_STRING_PTR	(t2_config_t, ssid,	 "ssid"),

	LSM_UNSIGNED	(t2_config_t, frequency, "frequency"),
	LSM_UNSIGNED	(t2_config_t, arg2,	 "arg2"),
	LSM_UNSIGNED	(t2_config_t, priority,	 "priority"),
};

/*
 * level 1: list-of-configurations object
 */

typedef struct t2_configs {
	lws_dll2_owner_t 			configs;
} t2_configs_t;

static const lws_struct_map_t lsm_t2_configs[] = {
	LSM_LIST	(t2_configs_t, configs, /* the list owner type/member */
			 t2_config_t,  list,	/* the list item type/member */
			 NULL, lsm_t2_config,	/* map object for item type */
			 "config"),		/* outer json object name */
};

/*
 * For parsing, this lists the kind of object we expect to parse so the struct
 * can be allocated polymorphically.
 *
 * Lws uses an explicit "schema" member so the type is known unambiguously.  If
 * in the incoming JSON the first member is not "schema", it will scan the
 * maps listed here and instantiate the first object that has a member of that
 * name.
 */

static const lws_struct_map_t lsm_schema[] = {
	LSM_SCHEMA	(t2_configs_t, NULL, lsm_t2_configs, "t2"),
	/* other schemata that might need parsing... */
};



static int
t2_config_dump(struct lws_dll2 *d, void *user)
{
#if !defined(LWS_WITH_NO_LOGS)
	t2_config_t *c = lws_container_of(d, t2_config_t, list);

	lwsl_notice("%s:   id1 '%s'\n", __func__, c->id1);
	lwsl_notice("%s:   arg1 '%s'\n", __func__, c->arg1);
	lwsl_notice("%s:   ssid '%s'\n", __func__, c->ssid);

	lwsl_notice("%s:   freq %d\n", __func__, c->frequency);
	lwsl_notice("%s:   arg2 %llu\n", __func__, c->arg2);
	lwsl_notice("%s:   priority %d\n", __func__, c->priority);

	lwsl_notice("%s:      key1: %s, key2: %s\n", __func__,
			     c->creds->key1, c->creds->key2);
#endif

	return 0;
}

static int
t2_configs_dump(t2_configs_t *t2cs)
{
	lwsl_notice("%s: number of configs: %d\n", __func__,
		    t2cs->configs.count);

	lws_dll2_foreach_safe(&t2cs->configs, NULL, t2_config_dump);

	return 0;
}


int
test2(void)
{
	lws_struct_serialize_t *ser;
	struct lejp_ctx ctx;
	lws_struct_args_t a;
	t2_configs_t *top;
	uint8_t buf[4096];
	size_t written;
	int n, bad = 1;

	lwsl_notice("%s: start \n", __func__);

	memset(&a, 0, sizeof(a));
	a.map_st[0] = lsm_schema;
	a.map_entries_st[0] = LWS_ARRAY_SIZE(lsm_schema);
	a.ac_block_size = 512;
	lws_struct_json_init_parse(&ctx, NULL, &a);

	n = lejp_parse(&ctx, (uint8_t *)test2_json, (int)strlen(test2_json));
	lwsl_notice("%s: lejp_parse %d\n", __func__, n);
	if (n < 0) {
		lwsl_err("%s: test2 JSON decode failed '%s'\n",
				__func__, lejp_error_to_string(n));
		goto bail;
	}
	lwsac_info(a.ac);

	top = (t2_configs_t *)a.dest; /* the top level object */

	if (!top) {
		lwsl_err("%s: no top level object\n", __func__);
		goto bail;
	}
	t2_configs_dump(top);

	/* 2. Let's reserialize the top level object and see what comes out */

	ser = lws_struct_json_serialize_create(&lsm_schema[0], 1,
					       LSSERJ_FLAG_OMIT_SCHEMA, top);
	if (!ser) {
		lwsl_err("%s: unable to init serialization\n", __func__);
		goto bail;
	}

	do {
		n = (int)lws_struct_json_serialize(ser, buf, sizeof(buf), &written);
		switch (n) {
		case LSJS_RESULT_FINISH:
			puts((const char *)buf);
			break;
		case LSJS_RESULT_CONTINUE:
		case LSJS_RESULT_ERROR:
			goto bail;
		}
	} while (n == LSJS_RESULT_CONTINUE);

	if (strcmp(test2_json_expected, (char *)buf)) {
		lwsl_err("%s: expected %s\n", __func__, test2_json_expected);
		goto bail;
	}

	lws_struct_json_serialize_destroy(&ser);

	bad = 0;

bail:
	lwsac_free(&a.ac);

	return bad;
}
