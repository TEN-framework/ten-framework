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
[2020/05/21 16:36:57:0808] U: LWS API selftest: lws_struct JSON
[2020/05/21 16:36:57:1188] N: main: ++++++++++++++++ test 1
[2020/05/21 16:36:57:1291] N: lws_struct_schema_only_lejp_cb: child map ofs_clist 0
[2020/05/21 16:36:57:1387] N: lws_struct_default_lejp_cb: created 'targets' object size 48
[2020/05/21 16:36:57:1429] N: lws_struct_default_lejp_cb: created 'targets' object size 48
[2020/05/21 16:36:57:1467] N: builder.hostname = 'learn', timeout = 1800, targets (2)
[2020/05/21 16:36:57:1490] N:     target.name 'target1' (target 0x509fe30)
[2020/05/21 16:36:57:1495] N:     target.name 'target2' (target 0x509fe68)
[2020/05/21 16:36:57:1500] N: main:    .... strarting serialization of test 1
{"schema":"com-warmcat-sai-builder","hostname":"learn","nspawn_timeout":1800,"targets":[{"name":"target1","someflag":true},{"name":"target2","someflag":false}]}
[2020/05/21 16:36:57:1648] N: main: ++++++++++++++++ test 2
[2020/05/21 16:36:57:1649] N: lws_struct_schema_only_lejp_cb: child map ofs_clist 0
[2020/05/21 16:36:57:1650] N: lws_struct_default_lejp_cb: created 'targets' object size 48
[2020/05/21 16:36:57:1651] N: lws_struct_default_lejp_cb: created 'targets' object size 48
[2020/05/21 16:36:57:1652] N: lws_struct_default_lejp_cb: created 'targets' object size 48
[2020/05/21 16:36:57:1653] N: builder.hostname = 'learn', timeout = 0, targets (3)
[2020/05/21 16:36:57:1653] N:     target.name 'target1' (target 0x50a0660)
[2020/05/21 16:36:57:1654] N:     target.name 'target2' (target 0x50a0698)
[2020/05/21 16:36:57:1655] N:     target.name 'target3' (target 0x50a06d0)
[2020/05/21 16:36:57:1655] N: main:    .... strarting serialization of test 2
{"schema":"com-warmcat-sai-builder","hostname":"learn","nspawn_timeout":0,"targets":[{"name":"target1","someflag":false},{"name":"target2","someflag":false},{"name":"target3","someflag":false}]}
[2020/05/21 16:36:57:1662] N: main: ++++++++++++++++ test 3
[2020/05/21 16:36:57:1663] N: lws_struct_schema_only_lejp_cb: child map ofs_clist 0
[2020/05/21 16:36:57:1664] N: lws_struct_default_lejp_cb: created 'targets' object size 48
[2020/05/21 16:36:57:1671] N: lws_struct_default_lejp_cb: created 'child' object size 8
[2020/05/21 16:36:57:1685] N: lws_struct_default_lejp_cb: created 'targets' object size 48
[2020/05/21 16:36:57:1685] N: builder.hostname = 'learn', timeout = 1800, targets (2)
[2020/05/21 16:36:57:1686] N:     target.name 'target1' (target 0x50a0a50)
[2020/05/21 16:36:57:1687] N:       child 0x50a0a88, target.child.somename 'abc'
[2020/05/21 16:36:57:1688] N:     target.name 'target2' (target 0x50a0a98)
[2020/05/21 16:36:57:1688] N: main:    .... strarting serialization of test 3
{"schema":"com-warmcat-sai-builder","hostname":"learn","nspawn_timeout":1800,"targets":[{"name":"target1","someflag":false,"child":{"somename":"abc"}},{"name":"target2","someflag":false}]}
[2020/05/21 16:36:57:1697] N: main: ++++++++++++++++ test 4
[2020/05/21 16:36:57:1698] N: lws_struct_schema_only_lejp_cb: child map ofs_clist 0
[2020/05/21 16:36:57:1699] N: builder.hostname = 'learn', timeout = 1800, targets (0)
[2020/05/21 16:36:57:1699] N: main:    .... strarting serialization of test 4
{"schema":"com-warmcat-sai-builder","hostname":"learn","nspawn_timeout":1800}
[2020/05/21 16:36:57:1701] N: main: ++++++++++++++++ test 5
[2020/05/21 16:36:57:1702] N: lws_struct_schema_only_lejp_cb: child map ofs_clist 0
[2020/05/21 16:36:57:1707] N: builder.hostname = '', timeout = 0, targets (0)
[2020/05/21 16:36:57:1708] N: main:    .... strarting serialization of test 5
{"schema":"com-warmcat-sai-builder","hostname":"","nspawn_timeout":0}
[2020/05/21 16:36:57:1709] N: main: ++++++++++++++++ test 6
[2020/05/21 16:36:57:1710] N: lws_struct_schema_only_lejp_cb: child map ofs_clist 0
[2020/05/21 16:36:57:1730] N: builder.hostname = 'PYvtan6kqppjnS0KpYTCaiOLsJkc7Xe', timeout = 0, targets (0)
[2020/05/21 16:36:57:1731] N: main:    .... strarting serialization of test 6
{"schema":"com-warmcat-sai-builder","hostname":"PYvtan6kqppjnS0KpYTCaiOLsJkc7Xe","nspawn_timeout":0}
[2020/05/21 16:36:57:1732] N: main: ++++++++++++++++ test 7
[2020/05/21 16:36:57:1732] N: lws_struct_schema_only_lejp_cb: child map ofs_clist 0
[2020/05/21 16:36:57:1733] N: lws_struct_default_lejp_cb: created 'targets' object size 48
[2020/05/21 16:36:57:1739] N: builder.hostname = '', timeout = 0, targets (1)
[2020/05/21 16:36:57:1751] N:     target.name 'PYvtan6kqppjnS0KpYTCaiOLsJkc7XecAr1kcE0aCIciewYB+JcLG82mO1Vb1mJtjDwUjBxy2I6AzefzoWUWmqZbsv4MXR55j9bKlyz1liiSX63iO0x6JAwACMtE2MkgcLwR86TSWAD9D1QKIWqg5RJ/CRuVsW0DKAUMD52ql4JmPFuJpJgTq28z6PhYNzN3yI3bmQt6bzhA+A/xAsFzSBnb3MHYWzGMprr53FAP1ISo5Ec9i+2ehV40sG6Q470sH3PGQZ0YRPO7Sh/SyrSQ/scONmxRc3AcXl7X/CSs417ii+CV8sq3ZgcxKNB7tNfN7idNx3upZ00G2BZy9jSy03cLKKLNaNUt0TQsxXbH55uDHzSEeZWvxJgT6zB1NoMhdC02w+oXim94M6z6COCnqT3rgkGk8PHMry9Bkh4yVpRmzIRfMmln/lEhdZgxky2+g5hhlSIGJYDCrdynD9kCfvfy6KGOpNIi1X+mhbbWn4lnL9ZKihL/RrfOV+oV4R26IDq+KqUiJBENeo8/GXkGLUH/87iPyzXKEMavr6fkrK0vTGto8yEYxmOyaVz8phG5rwf4jJgmYNoMbGo8gWvhqO7UAGy2g7MWv+B/t1eZZ+1euLsNrWAsFJiFbQKgdFfQT3RjB14iU8knlQ8usoy+pXssY2ddGJGVcGC21oZvstK9eu1eRZftda/wP+N5unT1Hw7kCoVzqxHieiYt47EGIOaaQ7XjZDK6qPN6O/grHnvJZm2vBkxuXgsYVkRQ7AuTWIecphqFsq7Wbc1YNbMW47SVU5zMD0WaCqbaaI0t4uIzRvPlD8cpiiTzFTrEHlIBTf8/uZjjEGGLhJR1jPqA9D1Ej3ChV+ye6F9JTUMlozRMsGuF8U4btDzH5xdnmvRS4Ar6LKEtAXGkj2yuyJln+v4RIWj2xOGPJovOqiXwi0FyM61f8U8gj0OiNA2/QlvrqQVDF7sMXgjvaE7iQt5vMETteZlx+z3f+jTFM/aon5...
[2020/05/21 16:36:57:1752] N: main:    .... strarting serialization of test 7
{"schema":"com-warmcat-sai-builder","hostname":"","nspawn_timeout":0,"targets":[{"name":"PYvtan6kqppjnS0KpYTCaiOLsJkc7XecAr1kcE0aCIciewYB+JcLG82mO1Vb1mJtjDwUjBxy2I6AzefzoWUWmqZbsv4MXR55j9bKlyz1liiSX63iO0x6JAwACMtE2MkgcLwR86TSWAD9D1QKIWqg5RJ/CRuVsW0DKAUMD52ql4JmPFuJpJgTq28z6PhYNzN3yI3bmQt6bzhA+A/xAsFzSBnb3MHYWzGMprr53FAP1ISo5Ec9i+2ehV40sG6Q470sH3PGQZ0YRPO7Sh/SyrSQ/scONmxRc3AcXl7X/CSs417ii+CV8sq3ZgcxKNB7tNfN7idNx3upZ00G2BZy9jSy03cLKKLNaNUt0TQsxXbH55uDHzSEeZWvxJgT6zB1NoMhdC02w+oXim94M6z6COCnqT3rgkGk8PHMry9Bkh4yVpRmzIRfMmln/lEhdZgxky2+g5hhlSIGJYDCrdynD9kCfvfy6KGOpNIi1X+mhbbWn4lnL9ZKihL/RrfOV+oV4R26IDq+KqUiJBENeo8/GXkGLUH/87iPyzXKEMavr6fkrK0vTGto8yEYxmOyaVz8phG5rwf4jJgmYNoMbGo8gWvhqO7UAGy2g7MWv+B/t1eZZ+1euLsNrWAsFJiFbQKgdFfQT3RjB14iU8knlQ8usoy+pXssY2ddGJGVcGC21oZvstK9eu1eRZftda/wP+N5unT1Hw7kCoVzqxHieiYt47EGIOaaQ7XjZDK6qPN6O/grHnvJZm2vBkxuXgsYVkRQ7AuTWIecphqFsq7Wbc1YNbMW47SVU5zMD0WaCqbaaI0t4uIzRvPlD8cpiiTzFTrEHlIBTf8/uZjjEGGLhJR1jPqA9D1Ej3ChV+ye6F9JTUMlozRMsGuF8U4btDzH5xdnmvRS4Ar6LKEtAXGkj2yuyJln+v4RIWj2xOGPJovOqiXwi0FyM61f8U8gj0OiNA2/QlvrqQVDF7sMXgjvaE7iQt5vMETteZlx+z3f+jTFM/aon511W4+ZkRD+6AHwucvM9BEC","someflag":false}]}
[2020/05/21 16:36:57:1756] N: main: ++++++++++++++++ test 8
[2020/05/21 16:36:57:1758] N: lws_struct_schema_only_lejp_cb: child map ofs_clist 0
[2020/05/21 16:36:57:1761] N: other.name = 'somename'
[2020/05/21 16:36:57:1763] N: main:    .... strarting serialization of test 8
{"schema":"com-warmcat-sai-other","name":"somename"}
{"schema":"meta.schema","t":{"name":"mytargetname","someflag":false},"e":{"hostname":"myhostname","nspawn_timeout":0}}
[2020/05/21 16:36:57:1785] N: Test set 2
[2020/05/21 16:36:57:1791] N: lws_struct_schema_only_lejp_cb: child map ofs_clist 0
[2020/05/21 16:36:57:1795] N: Test set 2: 6: 071ab46ab4296e5de674c628fec17c55088254679f7714ad991f8c4873dca
[2020/05/21 16:36:57:1801] N: test2: start 
[2020/05/21 16:36:57:1811] N: lws_struct_schema_only_lejp_cb: child map ofs_clist 0
[2020/05/21 16:36:57:1815] N: lws_struct_default_lejp_cb: created 'config' object size 80
[2020/05/21 16:36:57:1819] N: lws_struct_default_lejp_cb: created 'creds' object size 16
[2020/05/21 16:36:57:1833] N: lws_struct_default_lejp_cb: created 'config' object size 80
[2020/05/21 16:36:57:1834] N: lws_struct_default_lejp_cb: created 'creds' object size 16
[2020/05/21 16:36:57:1837] N: test2: lejp_parse 0
[2020/05/21 16:36:57:1841] N: t2_configs_dump: number of configs: 2
[2020/05/21 16:36:57:1844] N: t2_config_dump:   id1 '(null)'
[2020/05/21 16:36:57:1846] N: t2_config_dump:   arg1 'val1'
[2020/05/21 16:36:57:1848] N: t2_config_dump:   ssid '"nw2"'
[2020/05/21 16:36:57:1850] N: t2_config_dump:   freq 0
[2020/05/21 16:36:57:1852] N: t2_config_dump:   arg2 0
[2020/05/21 16:36:57:1854] N: t2_config_dump:   priority 1
[2020/05/21 16:36:57:1856] N: t2_config_dump:      key1: "xxxxxxxxx", key2: (null)
[2020/05/21 16:36:57:1857] N: t2_config_dump:   id1 '(null)'
[2020/05/21 16:36:57:1858] N: t2_config_dump:   arg1 'val2'
[2020/05/21 16:36:57:1858] N: t2_config_dump:   ssid '"nw1"'
[2020/05/21 16:36:57:1859] N: t2_config_dump:   freq 11
[2020/05/21 16:36:57:1859] N: t2_config_dump:   arg2 1420887242594
[2020/05/21 16:36:57:1860] N: t2_config_dump:   priority 3
[2020/05/21 16:36:57:1860] N: t2_config_dump:      key1: "xxxxxxxxxxxxx", key2: (null)
{"config":[{"creds":{"key1":"\u0022xxxxxxxxx\u0022"},"arg1":"val1","ssid":"\u0022nw2\u0022","frequency":0,"arg2":0,"priority":1},{"creds":{"key1":"\u0022xxxxxxxxxxxxx\u0022"},"arg1":"val2","ssid":"\u0022nw1\u0022","frequency":11,"arg2":1420887242594,"priority":3}]}
[2020/05/21 16:36:57:1880] U: Completed: PASS
```

