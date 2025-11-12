# lws minimal example for cose_key

Demonstrates how to create and dump cose_keys.

## Dump key or key_set

Pipe a cose_key or cose_key_set into stdin to get a textual dump of all the keys
inside.  You can optionally use --kid kid or --kid-hex HEXSTRING to dump one key
from a set.

```
$ cat set1.cks | ./bin/lws-crypto-cose-key
$ cat set1.cks | ./bin/lws-crypto-cose-key --kid 11
```

## Create keys

Stdin is not used, give parameters for the kty and kid etc to create a
new key on stdout (which can be redirected to a file).

```
$ ./bin/lws-crypto-cose-key --kty EC2 --curve P-521 --kid sec512 >ec512.key
```

## build

```
 $ cmake . && make
```

## usage

|Option|Meaning|
|---|---|
|--kty type|Key type, one of OKP, EC2, RSA or SYMMETRIC|
|-k \<keyset filepath\>|One or a set of cose_keys|
|--kid string|Specifies the key ID to use as a string|
|--kid-hex HEXSTRING|Specifies the key ID to use as a hex blob|
|--curve curve|For EC type key creation, specify the curve|
|--stdin filepath|Makes tool fetch from filepath instead of stdin (useful for CI)|
|--stdout filepath|Makes tool write to filepath instead of stdout (useful for CI)|


HEXSTRING above means a string like `1a2b3c`

## Examples

### cose_key dumping

```
$ cat set1.cks | ./bin/lws-crypto-cose-key
[2021/07/30 10:14:31:0420] U: LWS cose-key example tool -k keyset [-s alg-name kid ]
[2021/07/30 10:14:31:0780] N: lws_create_context: LWS: 4.2.99-v4.2.0-134-g8433c8b459, NET CLI SRV H1 H2 WS ConMon IPV6-on
[2021/07/30 10:14:31:0892] N:  ++ [wsi|0|pipe] (1)
[2021/07/30 10:14:31:0926] N:  ++ [vh|0|netlink] (1)
[2021/07/30 10:14:31:0977] N:  ++ [vh|1|default||-1] (2)
[2021/07/30 10:14:31:1057] N: main: importing
Cose key #1
  kty: EC2
  kid: 11
  kty: P-256
  x: bac5b11cad8f99f9c72b05cf4b9e26d244dc189f745228255a219a86d6a09eff
  d: 57c92077664146e876760c9520d054aa93c3afb04e306705db6090308507b4d3
  y: 20138bf82dc1b6d562be0fa54ab7804a3a64b6d72ccfed6b6fb6ed28bbfc117e
Cose key #2
  kty: EC2
  kid: meriadoc.brandybuck@buckland.example
  kty: P-256
  x: 65eda5a12577c2bae829437fe338701a10aaa375e1bb5b5de108de439c08551d
  d: aff907c99f9ad3aae6c4cdf21122bce2bd68b5283e6907154ad911840fa208cf
  y: 1e52ed75701163f7f9e40ddf9f341b3dc9ba860af7e0ca7ca7e9eecd0084d19c
Cose key #3
  kty: SYMMETRIC
  kid: our-secret
  k: 849b57219dae48de646d07dbb533566e976686457c1491be3a76dcea6c427188
Cose key #4
  kty: EC2
  kid: bilbo.baggins@hobbiton.example
  kty: P-521
  x: 0072992cb3ac08ecf3e5c63dedec0d51a8c1f79ef2f82f94f3c737bf5de7986671eac625fe8257bbd0394644caaa3aaf8f27a4585fbbcad0f2457620085e5c8f42ad
  d: 00085138ddabf5ca975f5860f91a08e91d6d5f9a76ad4018766a476680b55cd339e8ab6c72b5facdb2a2a50ac25bd086647dd3e2e6e99e84ca2c3609fdf177feb26d
  y: 01dca6947bce88bc5790485ac97427342bc35f887d86d65a089377e247e60baa55e4e8501e2ada5724ac51d6909008033ebc10ac999b9d7f5cc2519f3fe1ea1d9475
Cose key #5
  kty: SYMMETRIC
  kid: our-secret2
  k: 849b5786457c1491be3a76dcea6c4271
Cose key #6
  kty: EC2
  kid: peregrin.took@tuckborough.example
  kty: P-256
  x: 98f50a4ff6c05861c8860d13a638ea56c3f5ad7590bbfbf054e1c7b4d91d6280
  d: 02d1f7e6f26c43d4868d87ceb2353161740aacf1f7163647984b522a848df1c3
  y: f01400b089867804b8e9fc96c3932161f1934f4223069170d924b7e03bf822bb
Cose key #7
  kty: SYMMETRIC
  kid: 018c0ae5-4d9b-471b-bfd6-eef314bc7037
  use: 849b57219dae48de646d07dbb533566e976686457c1491be3a76dcea6c427188
Cose key #8
  kty: SYMMETRIC
  kid: sec-48
  k: 849b57219dae48de646d07dbb533566e976686457c1491be3a76dcea6c42718800112233778899aa2122232425262728
Cose key #9
  kty: SYMMETRIC
  kid: sec-64
  k: 849b57219dae48de646d07dbb533566e976686457c1491be3a76dcea6c42718800112233778899aa2122232425262728aabbccddeeffa5a6a7a8a9a0b1b2b3b4
Cose key #10
  kty: EC2
  kid: sec384
  kty: P-384
  x: ea2866349fe3a2f9ad4d6bfe7c30c527436e901c5fb22210b67b2150574ffcd0b1dd8c43d5d1e3d5cb849ecec202117c
  d: 4d46a58480d43d5454307edcf501e098ef7c0186cc6b56b41dfd13fe4b9b1ab1425851cf5b23e6636ed18f5bbdde1896
  y: 4c3d245515a688ef25ff68034089ca4f10a01bef51cc57309f12919c3d484142368795c6f2a5d30af650b4e12d0133e4
Cose key #11
  kty: EC2
  kid: sec512
  kty: P-521
  x: 003b81ed66d8a2194b42f29ecb2c9ae48199be695924804a8407194ed0e172f39693f870f32463e2d36950034a21901487c5a0c43a1713a818fb89fa8a5b3b2dc181
  d: 013e0f06ce394ac14a3df3953fc560679ad0dee14779ef0d475787451fca71e3b4b827b6f7cedcf00e23c716fb829b5419234ba5c92c33e0bc94351fe97be21f2b82
  y: 004b9b6b0adf41913b5d700cf43bfe0ee8b79eb58fc308509e574fcb910b3fd5a2ad585affc6776f7fc9d4ff48f5923fe900660ecc6e3720f89c1363eecfffb38b5b
[2021/07/30 10:14:31:1430] N:  -- [wsi|0|pipe] (0) 52.763ms
[2021/07/30 10:14:31:1441] N:  -- [vh|0|netlink] (1) 51.437ms
[2021/07/30 10:14:31:1491] N:  -- [vh|1|default||-1] (0) 51.591ms
[2021/07/30 10:14:31:1536] N: main: PASS

```

### cose_key creation

```
$ ./bin/lws-crypto-cose-key --kty EC2 --curve P-521 --kid sec512 >ec512.key
```

