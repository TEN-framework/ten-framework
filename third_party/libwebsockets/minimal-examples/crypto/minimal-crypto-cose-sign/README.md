# lws minimal example for cose_sign

Demonstrates how to sign and verify using cose_sign and cose_key, providing a
commandline tool for signing and verifying stdin.

## build

```
 $ cmake . && make
```

## usage

|Option|Sig|Val|Meaning|
|---|---|---|---|
|-s|o|||Select signing mode (stdin is payload)|
|-k <keyset filepath>|o|o|One or a set of cose_keys|
|--kid string|o|mac0|Specifies the key ID to use as a string|
|--kid-hex HEXSTRING|o|mac0|Specifies the key ID to use as a hex blob|
|--cose-sign|o|if no tag|Sets cose-sign mode|
|--cose-sign1|o|if no tag|Sets cose-sign1 mode|
|--cose-mac|o|if no tag|Sets cose-sign1 mode|
|--cose-mac0|o|if no tag|Sets cose-sign1 mode|
|--extra HEXSTRING|o|o|Optional extra payload data|

HEXSTRING above means a string like `1a2b3c`

Stdin is either the plaintext (if signing) or cose_sign (if verifying).

For convenience, a keyset from the COSE RFC is provided in
`minimal-examples/crypto/minimal-crypto-cose-sign/set1.cks`.  Six example
cose_sign1 and cose_sign are also provided in that directory signed with keys
from the provided keyset.

## Examples

### Validation

The RFC8152 sign1_pass01.sig is a cose_sign1 that contains the ES256 alg
parameter along with a kid hint that it was signed with the key with kid "11"
from the RFC8152 key set.  So we just need to provide the signature and the key
set and lws can sort it out.

```
$ cat sign1_pass01.sig | ./lws-crypto-cose-sign -k set1.cks
[2021/07/26 05:41:29:1663] N: lws_create_context: LWS: 4.2.99-v4.2.0-133-g300f3f3250, NET CLI SRV H1 H2 WS ConMon IPV6-on
[2021/07/26 05:41:29:3892] N: results count 1
[2021/07/26 05:41:29:3901] N: result: 0 (alg ES256, kid 3131)
[2021/07/26 05:41:29:4168] N: main: PASS
```

Notice how the validation just delivers a results list and leaves it to the user
code to iterate it, and confirm that it's happy with the result, the alg used,
and the kid that was used.

RFC8152 sign1_pass02.sig is similar but contains extra application data in the
signature, that must be given at validation too.

```
$cat sign1_pass02.sig | ./lws-crypto-cose-sign -k set1.cks --extra 11aa22bb33cc44dd55006699
[2021/07/26 05:55:50:9103] N: lws_create_context: LWS: 4.2.99-v4.2.0-133-g300f3f3250, NET CLI SRV H1 H2 WS ConMon IPV6-on
[2021/07/26 05:55:50:9381] N: 12
[2021/07/26 05:55:51:0924] N: 
[2021/07/26 05:55:51:0939] N: 0000: 11 AA 22 BB 33 CC 44 DD 55 00 66 99                ..".3.D.U.f.    
[2021/07/26 05:55:51:0943] N: 
[2021/07/26 05:55:51:1368] N: results count 1
[2021/07/26 05:55:51:1377] N: result: 0 (alg ES256, kid 3131)
[2021/07/26 05:55:51:1657] N: main: PASS
```

### Signing

Generate a cose-sign1 using ES256 and the key set key with id "11" for the
payload given on stdin

```
$ echo -n "This is the content." |\
   ./bin/lws-crypto-cose-sign -s -k set1.cks \
   --kid 11 --alg ES256 > ./test.sig

00000000  d2 84 43 a1 01 26 a1 04  42 31 31 54 54 68 69 73  |..C..&..B11TThis|
00000010  20 69 73 20 74 68 65 20  63 6f 6e 74 65 6e 74 2e  | is the content.|
00000020  58 40 b9 a8 85 09 17 7f  01 f6 78 5d 39 62 d0 44  |X@........x]9b.D|
00000030  08 0b fa b4 b4 5b 17 80  c2 e3 ba a3 af 33 6f e6  |.....[.......3o.|
00000040  44 09 13 1f cf 4f 17 5c  62 9f 8d 29 29 1c ab 28  |D....O.\b..))..(|
00000050  b2 f4 e6 af f9 62 ea 69  52 90 07 0e 2c 40 72 d3  |.....b.iR...,@r.|
00000060  12 cf                                             |..|

```

Same as above, but force it to use cose-sign layout

```
$ echo -n "This is the content." |\
   ./bin/lws-crypto-cose-sign -s -k set1.cks \
   --kid 11 --alg ES256 --cose-sign > ./test.sig

00000000  d8 62 84 40 40 54 54 68  69 73 20 69 73 20 74 68  |.b.@@TThis is th|
00000010  65 20 63 6f 6e 74 65 6e  74 2e 81 83 a1 01 26 a1  |e content.....&.|
00000020  04 42 31 31 58 40 37 5d  93 48 20 b0 d0 75 16 41  |.B11X@7].H ..u.A|
00000030  db 95 95 5b 39 7d 6d 92  6e 52 c9 78 96 d8 a2 9b  |...[9}m.nR.x....|
00000040  62 62 89 9e e5 26 31 63  4b 90 d1 37 86 ca 82 a2  |bb...&1cK..7....|
00000050  28 9a d2 82 a7 6d 24 23  cd de 58 91 47 98 bb 11  |(....m$#..X.G...|
00000060  e4 b9 08 18 48 65                                 |....He|
```
