# lws api test lws_struct JSON

Demonstrates how to use and performs selftests for lws_struct
JSON serialization and deserialization

## build

```
 $ cmake . && make
```

## usage

Commandline option|Meaning
---|---
-d <loglevel>|Debug verbosity in decimal, eg, -d15

```
 $ ./lws-api-test-lws_struct-json
[2019/03/30 22:09:09:2529] USER: LWS API selftest: lws_struct JSON
[2019/03/30 22:09:09:2625] NOTICE: main: ++++++++++++++++ test 1
[2019/03/30 22:09:09:2812] NOTICE: builder.hostname = 'learn', timeout = 1800, targets (2)
[2019/03/30 22:09:09:2822] NOTICE:     target.name 'target1' (target 0x543a830)
[2019/03/30 22:09:09:2824] NOTICE:     target.name 'target2' (target 0x543a860)
[2019/03/30 22:09:09:2826] NOTICE: main:    .... strarting serialization of test 1
[2019/03/30 22:09:09:2899] NOTICE: ser says 1
{"schema":"com-warmcat-sai-builder","hostname":"learn","nspawn_timeout":1800,"targets":[{"name":"target1"},{"name":"target2"}]}
[2019/03/30 22:09:09:2929] NOTICE: main: ++++++++++++++++ test 2
[2019/03/30 22:09:09:2932] NOTICE: builder.hostname = 'learn', timeout = 0, targets (3)
[2019/03/30 22:09:09:2932] NOTICE:     target.name 'target1' (target 0x543b060)
[2019/03/30 22:09:09:2933] NOTICE:     target.name 'target2' (target 0x543b090)
[2019/03/30 22:09:09:2933] NOTICE:     target.name 'target3' (target 0x543b0c0)
[2019/03/30 22:09:09:2934] NOTICE: main:    .... strarting serialization of test 2
[2019/03/30 22:09:09:2935] NOTICE: ser says 1
{"schema":"com-warmcat-sai-builder","hostname":"learn","nspawn_timeout":0,"targets":[{"name":"target1"},{"name":"target2"},{"name":"target3"}]}
[2019/03/30 22:09:09:2940] NOTICE: main: ++++++++++++++++ test 3
[2019/03/30 22:09:09:2959] NOTICE: builder.hostname = 'learn', timeout = 1800, targets (2)
[2019/03/30 22:09:09:2960] NOTICE:     target.name 'target1' (target 0x543b450)
[2019/03/30 22:09:09:2961] NOTICE:       child 0x543b480, target.child.somename 'abc'
[2019/03/30 22:09:09:2961] NOTICE:     target.name 'target2' (target 0x543b490)
[2019/03/30 22:09:09:2962] NOTICE: main:    .... strarting serialization of test 3
[2019/03/30 22:09:09:2969] NOTICE: ser says 1
{"schema":"com-warmcat-sai-builder","hostname":"learn","nspawn_timeout":1800,"targets":[{"name":"target1","child":{"somename":"abc"}},{"name":"target2"}]}
[2019/03/30 22:09:09:2970] NOTICE: main: ++++++++++++++++ test 4
[2019/03/30 22:09:09:2971] NOTICE: builder.hostname = 'learn', timeout = 1800, targets (0)
[2019/03/30 22:09:09:2971] NOTICE: main:    .... strarting serialization of test 4
[2019/03/30 22:09:09:2973] NOTICE: ser says 1
{"schema":"com-warmcat-sai-builder","hostname":"learn","nspawn_timeout":1800}
[2019/03/30 22:09:09:2974] NOTICE: main: ++++++++++++++++ test 5
[2019/03/30 22:09:09:2978] NOTICE: builder.hostname = '', timeout = 0, targets (0)
[2019/03/30 22:09:09:2979] NOTICE: main:    .... strarting serialization of test 5
[2019/03/30 22:09:09:2980] NOTICE: ser says 1
{"schema":"com-warmcat-sai-builder","hostname":"","nspawn_timeout":0}
[2019/03/30 22:09:09:2982] USER: Completed: PASS
```

