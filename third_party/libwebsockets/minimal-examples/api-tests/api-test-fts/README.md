# lws api test fts

Demonstrates how to create indexes and perform full-text searches.

## build

```
 $ cmake . && make
```

## usage

Commandline option|Meaning
---|---
-d <loglevel>|Debug verbosity in decimal, eg, -d15
-c / --createindex|Create an index file, instead of searching
-i / --index <file>|Use this file as the index

The two modes are:

 - create an index: `--createindex inputfile [inputfile...]`

```
 $ ./lws-api-test-fts -c ./the-picture-of-dorian-gray.txt
[2018/10/15 07:14:15:1175] USER: LWS API selftest: full-text search
[2018/10/15 07:14:15:1531] NOTICE: lws_fts_serialize: index 1 files (0MiB) cpu time 32ms, alloc: 1024KiB + 1024KiB, serialize: 3ms, file: 325KiB 
```

 - perform search[es]: `searchterm [searchterm...]`

```
 $ ./lws-api-test-fts b
[2018/10/15 07:15:44:1442] USER: LWS API selftest: full-text search 
[2018/10/15 07:15:44:1442] NOTICE: lws_fts_search: 'b' Matched: 3 instances, 8 children, 0ms
[2018/10/15 07:15:44:1443] NOTICE: lws_fts_results_dump: AC b: 3 agg hits
[2018/10/15 07:15:44:1443] NOTICE: lws_fts_results_dump: AC be: 472 agg hits
[2018/10/15 07:15:44:1443] NOTICE: lws_fts_results_dump: AC bee: 3 agg hits
[2018/10/15 07:15:44:1443] NOTICE: lws_fts_results_dump: AC been: 236 agg hits
[2018/10/15 07:15:44:1443] NOTICE: lws_fts_results_dump: AC beaut: 1 agg hits
[2018/10/15 07:15:44:1443] NOTICE: lws_fts_results_dump: AC beauty: 55 agg hits
[2018/10/15 07:15:44:1443] NOTICE: lws_fts_results_dump: AC because: 40 agg hits
[2018/10/15 07:15:44:1443] NOTICE: lws_fts_results_dump: AC believe: 49 agg hits
[2018/10/15 07:15:44:1443] NOTICE: lws_fts_results_dump: AC better: 54 agg hits
[2018/10/15 07:15:44:1443] NOTICE: lws_fts_results_dump: AC before: 75 agg hits
[2018/10/15 07:15:44:1443] NOTICE: lws_fts_results_dump: AC beg: 5 agg hits
[2018/10/15 07:15:44:1443] NOTICE: lws_fts_results_dump: AC began: 44 agg hits
[2018/10/15 07:15:44:1443] NOTICE: lws_fts_results_dump: AC but: 401 agg hits
[2018/10/15 07:15:44:1443] NOTICE: lws_fts_results_dump: AC basil: 158 agg hits
[2018/10/15 07:15:44:1443] NOTICE: lws_fts_results_dump: AC broke: 22 agg hits
[2018/10/15 07:15:44:1444] NOTICE: lws_fts_results_dump: AC by: 242 agg hits
[2018/10/15 07:15:44:1444] NOTICE: lws_fts_results_dump: AC boy: 36 agg hits
```

