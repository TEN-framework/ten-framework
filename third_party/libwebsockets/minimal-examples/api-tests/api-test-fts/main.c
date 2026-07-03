/*
 * lws-api-test-fts - lws full-text search api test
 *
 * Written in 2010-2019 by Andy Green <andy@warmcat.com>
 *
 * This file is made available under the Creative Commons CC0 1.0
 * Universal Public Domain Dedication.
 */

#include <libwebsockets.h>
#if defined(LWS_HAS_GETOPT_LONG) || defined(WIN32)
#include <getopt.h>
#endif
#include <fcntl.h>

#if defined(LWS_HAS_GETOPT_LONG) || defined(WIN32)
static struct option options[] = {
	{ "help",	no_argument,		NULL, 'h' },
	{ "createindex", no_argument,		NULL, 'c' },
	{ "index",	required_argument,	NULL, 'i' },
	{ "debug",	required_argument,	NULL, 'd' },
	{ "file",	required_argument,	NULL, 'f' },
	{ "lines",	required_argument,	NULL, 'l' },
	{ NULL, 0, 0, 0 }
};
#endif

static const char *index_filepath = "/tmp/lws-fts-test-index";
static char filepath[256];

int main(int argc, char **argv)
{
	int n, logs = LLL_USER | LLL_ERR | LLL_WARN | LLL_NOTICE;
	int fd, fi, ft, createindex = 0, flags = LWSFTS_F_QUERY_AUTOCOMPLETE;
	struct lws_fts_search_params params;
	struct lws_fts_result *result;
	struct lws_fts_file *jtf;
	struct lws_fts *t;
	char buf[16384];

	do {
#if defined(LWS_HAS_GETOPT_LONG) || defined(WIN32)
		n = getopt_long(argc, argv, "hd:i:cfl", options, NULL);
#else
       n = getopt(argc, argv, "hd:i:cfl");
#endif
		if (n < 0)
			continue;
		switch (n) {
		case 'i':
			strncpy(filepath, optarg, sizeof(filepath) - 1);
			filepath[sizeof(filepath) - 1] = '\0';
			index_filepath = filepath;
			break;
		case 'd':
			logs = atoi(optarg);
			break;
		case 'c':
			createindex = 1;
			break;
		case 'f':
			flags &= ~LWSFTS_F_QUERY_AUTOCOMPLETE;
			flags |= LWSFTS_F_QUERY_FILES;
			break;
		case 'l':
			flags |= LWSFTS_F_QUERY_FILES |
				 LWSFTS_F_QUERY_FILE_LINES;
			break;
		case 'h':
			fprintf(stderr,
				"Usage: %s [--createindex]"
					"[--index=<index filepath>] "
					"[-d <log bitfield>] file1 file2 \n",
					argv[0]);
			exit(1);
		}
	} while (n >= 0);

	lws_set_log_level(logs, NULL);
	lwsl_user("LWS API selftest: full-text search\n");

	if (createindex) {

		lwsl_notice("Creating index\n");

		/*
		 * create an index by shifting through argv and indexing each
		 * file given there into a single combined index
		 */

		ft = open(index_filepath, O_CREAT | O_WRONLY | O_TRUNC, 0600);
		if (ft < 0) {
			lwsl_err("%s: can't open index %s\n", __func__,
				 index_filepath);

			goto bail;
		}

		t = lws_fts_create(ft);
		if (!t) {
			lwsl_err("%s: Unable to allocate trie\n", __func__);

			goto bail1;
		}

		while (optind < argc) {

			fi = lws_fts_file_index(t, argv[optind],
						(int)strlen(argv[optind]), 1);
			if (fi < 0) {
				lwsl_err("%s: Failed to get file idx for %s\n",
					 __func__, argv[optind]);

				goto bail1;
			}

			fd = open(argv[optind], O_RDONLY);
			if (fd < 0) {
				lwsl_err("unable to open %s for read\n",
						argv[optind]);
				goto bail;
			}

			do {
				int n = (int)read(fd, buf, sizeof(buf));

				if (n <= 0)
					break;

				if (lws_fts_fill(t, (uint32_t)fi, buf, (size_t)n)) {
					lwsl_err("%s: lws_fts_fill failed\n",
						 __func__);
					close(fd);

					goto bail;
				}

			} while (1);

			close(fd);
			optind++;
		}

		if (lws_fts_serialize(t)) {
			lwsl_err("%s: serialize failed\n", __func__);

			goto bail;
		}

		lws_fts_destroy(&t);
		close(ft);

		return 0;
	}

	/*
	 * shift through argv searching for each token
	 */

	jtf = lws_fts_open(index_filepath);
	if (!jtf)
		goto bail;

	while (optind < argc) {

		struct lws_fts_result_autocomplete *ac;
		struct lws_fts_result_filepath *fp;
		uint32_t *l, n;

		memset(&params, 0, sizeof(params));

		params.needle = argv[optind];
		params.flags = flags;
		params.max_autocomplete = 20;
		params.max_files = 20;

		result = lws_fts_search(jtf, &params);

		if (!result) {
			lwsl_err("%s: search failed\n", __func__);
			lws_fts_close(jtf);
			goto bail;
		}

		ac = result->autocomplete_head;
		fp = result->filepath_head;

		if (!ac)
			lwsl_notice("%s: no autocomplete results\n", __func__);

		while (ac) {
			lwsl_notice("%s: AC %s: %d agg hits\n", __func__,
				((char *)(ac + 1)), ac->instances);

			ac = ac->next;
		}

		if (!fp)
			lwsl_notice("%s: no filepath results\n", __func__);

		while (fp) {
			lwsl_notice("%s: %s: (%d lines) %d hits \n", __func__,
				(((char *)(fp + 1)) + fp->matches_length),
				fp->lines_in_file, fp->matches);

			if (fp->matches_length) {
				l = (uint32_t *)(fp + 1);
				n = 0;
				while ((int)n++ < fp->matches)
					lwsl_notice(" %d\n", *l++);
			}
			fp = fp->next;
		}

		lwsac_free(&params.results_head);

		optind++;
	}

	lws_fts_close(jtf);

	return 0;

bail1:
	close(ft);
bail:
	lwsl_user("FAILED\n");

	return 1;
}
