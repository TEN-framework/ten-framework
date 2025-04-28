# lws minimal secure streams policy2c

This application parses a JSON policy passed on stdin and emits the
equivalent of it in C structs ready for compilation.

This is useful in the case your platform doesn't use a dynamic JSON
policy and is space-constrained, you can still form and maintain the
policy in JSON, but with this utility convert it into compileable C.

**Notice** this depends on LWS_ROLE_H1, LWS_ROLE_H2, LWS_ROLE_WS and
LWS_ROLE_MQTT build of lws, since it has to be able to work with any kind
of policy content.

## build

```
 $ cmake . && make
```

## usage

Commandline option|Meaning
---|---
-d <loglevel>|Debug verbosity in decimal, eg, -d15

```
$ cat mypolicy.json | lws-minimal-secure-streams-policy2c

(on stdout) 

static const uint32_t _rbo_bo_0[] = {
 1000,  2000,  3000,  5000,  10000, 
};
static const lws_retry_bo_t _rbo_0 = {
	.retry_ms_table = _rbo_bo_0,
	.retry_ms_table_count = 5,
	.conceal_count = 5,
	.secs_since_valid_ping = 30,
	.secs_since_valid_hangup = 35,
	.jitter_percent = 20,
};
static const uint8_t _ss_der_amazon_root_ca_1[] = {
	/* 0x  0 */ 0x30, 0x82, 0x03, 0x41, 0x30, 0x82, 0x02, 0x29, 
	/* 0x  8 */ 0xA0, 0x03, 0x02, 0x01, 0x02, 0x02, 0x13, 0x06, 
	/* 0x 10 */ 0x6C, 0x9F, 0xCF, 0x99, 0xBF, 0x8C, 0x0A, 0x39, 
	/* 0x 18 */ 0xE2, 0xF0, 0x78, 0x8A, 0x43, 0xE6, 0x96, 0x36, 
	/* 0x 20 */ 0x5B, 0xCA, 0x30, 0x0D, 0x06, 0x09, 0x2A, 0x86, 
...
```
