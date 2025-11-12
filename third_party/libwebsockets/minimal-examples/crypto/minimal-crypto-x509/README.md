# lws minimal example for X509

The example shows how to:

 - confirm one PEM cert or chain (-c) was signed by a trusted PEM cert (-t)
 - convert a certificate public key to JWK
 - convert a certificate public key and its private key PEM to a private JWK

The examples work for EC and RSA certs and on mbedtls and OpenSSL the same.

Notice the logging is on stderr, and only the JWK is output on stdout.

## build

```
 $ cmake . && make
```

## usage

Commandline option|Meaning
---|---
-d <loglevel>|Debug verbosity in decimal, eg, -d15
-c <PEM certificate path>|Required PEM Certificate(s) to operate on... may be multiple concatednated PEM
-t <PEM certificate path>|Single PEM trusted certificate
-p <PEM private key path>|Optional private key matching certificate given in -c.  If given, only the private JWK is printed to stdout

Example for confirming trust relationship.  Notice the PEM in -c must contain not only
the final certificate but also the certificates for any intermediate CAs.

```
 $ ./lws-crypto-x509 -c ec-cert.pem -t ca-cert.pem
[2019/01/02 20:31:13:2031] USER: LWS X509 api example
[2019/01/02 20:31:13:2032] NOTICE: Creating Vhost 'default' (serving disabled), 1 protocols, IPv6 off
[2019/01/02 20:31:13:2043] NOTICE: main: certs loaded OK
[2019/01/02 20:31:13:2043] NOTICE: main: verified OK  <<<<======
[2019/01/02 20:31:13:2045] NOTICE: Cert Public JWK
{"crv":"P-521","kty":"EC","x":"_uRNBbIbm0zhk8v6ujvQX9924264ZkqJhit0qamAoCegzuJbLf434kN7_aFEt6u-QWUu6-N1R8t6OlvrLo2jrNY","y":"AU-29XpNyB7e5e3s5t0ylzGEnF601A8A7Tx8m8xxngARZX_bn22itGJ3Y57BTcclPMoG80KjWAMnRVtrKqrD_aGD"}

[2019/01/02 20:31:13:2045] NOTICE: main: OK
```

Example creating JWKs for public and public + private cert + PEM keys:

```
 $ ./lws-crypto-x509 -c ec-cert.pem -p ec-key.pem
[2019/01/02 20:14:43:4966] USER: LWS X509 api example
[2019/01/02 20:14:43:5225] NOTICE: Creating Vhost 'default' (serving disabled), 1 protocols, IPv6 off
[2019/01/02 20:14:43:5707] NOTICE: lws_x509_public_to_jwk: EC key
[2019/01/02 20:24:59:9514] USER: LWS X509 api example
[2019/01/02 20:24:59:9741] NOTICE: Creating Vhost 'default' (serving disabled), 1 protocols, IPv6 off
[2019/01/02 20:25:00:1261] NOTICE: lws_x509_public_to_jwk: key type 408 "id-ecPublicKey"
[2019/01/02 20:25:00:1269] NOTICE: lws_x509_public_to_jwk: EC key
[2019/01/02 20:25:00:2097] NOTICE: Cert + Key Private JWK
{"crv":"P-521","d":"AU3iQSKfPskMTW4ZncrYLhipUYzLYty2XhemTQ_nSuUB1vB76jHmOYUTRXFBLkVCW8cQYyMa5dMa3Bvv-cdvH0IB","kty":"EC","x":"_uRNBbIbm0zhk8v6ujvQX9924264ZkqJhit0qamAoCegzuJbLf434kN7_aFEt6u-QWUu6-N1R8t6OlvrLo2jrNY","y":"AU-29XpNyB7e5e3s5t0ylzGEnF601A8A7Tx8m8xxngARZX_bn22itGJ3Y57BTcclPMoG80KjWAMnRVtrKqrD_aGD"}

[2019/01/02 20:25:00:2207] NOTICE: main: OK
```

