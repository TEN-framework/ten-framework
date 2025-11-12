lws-acme-client Plugin
======================

## Introduction

lws-acme-client is a protcol plugin for libwebsockets that implements an
ACME client able to communicate with let's encrypt and other certificate
providers.

It implements `tls-sni-01` challenge, and is able to provision tls certificates
"from thin air" that are accepted by all the major browsers.  It also manages
re-requesting the certificate when it only has two weeks left to run.

It works with both the OpenSSL and mbedTLS backends.

## Overview for use

You need to:

 - Provide name resolution to the IP with your server, ie, myserver.com needs to
 resolve to the IP that hosts your server

 - Enable port forwarding / external firewall access to your port, usually 443

 - Enable the "lws-acme-client" plugin on the vhosts you want it to manage
   certs for

 - Add per-vhost options describing what should be in the certificate

After that the plugin will sort everything else out.

## Example lwsws setup

```
 "vhosts": [ {
	"name": 		   "home.warmcat.com",
	"port":			   "443",
        "host-ssl-cert":           "/etc/lwsws/acme/home.warmcat.com.crt.pem",
        "host-ssl-key":            "/etc/lwsws/acme/home.warmcat.com.key.pem",
        "ignore-missing-cert":     "1",
	"access-log": 		   "/var/log/lwsws/test-access-log",
        "ws-protocols": [{
	  "lws-acme-client": {
	    "auth-path":	   "/etc/lwsws/acme/auth.jwk",
	    "cert-path":           "/etc/lwsws/acme/home.warmcat.com.crt.pem",
	    "key-path":            "/etc/lwsws/acme/home.warmcat.com.key.pem",
	    "directory-url":       "https://acme-staging.api.letsencrypt.org/directory",
	    "country":             "TW",
	    "state":               "Taipei",
	    "locality":            "Xiaobitan",
	    "organization":        "Crash Barrier Ltd",
	    "common-name":         "home.warmcat.com",
	    "email":               "andy@warmcat.com"
	  },
	  ...
```

## Required PVOs

Notice that the `"host-ssl-cert"` and `"host-ssl-key"` entries have the same
meaning as usual, they point to your certificate and private key.  However
because the ACME plugin can provision these, you should also mark the vhost with
`"ignore-missing-cert" : "1"`, so lwsws will ignore what will initially be
missing certificate / keys on that vhost, and will set about creating the
necessary certs and keys instead of erroring out.

You must make sure the directories mentioned here exist, lws doesn't create them
for you.  They should be 0700 root:root, even if you drop lws privileges.

If you are implementing support in code, this corresponds to making sure the
vhost creating `info.options` has the `LWS_SERVER_OPTION_IGNORE_MISSING_CERT`
bit set.

Similarly, in code, the each of the per-vhost options shown above can be
provided in a linked-list of structs at vhost creation time.  See
`./test-apps/test-server-v2.0.c` for example code for providing pvos.

### auth-path

This is where the plugin will store the auth keys it generated.

### cert-path

Where the plugin will store the certificate file.  Should match `host-ssl-cert`
that the vhost wants to use.

The path should include at least one 0700 root:root directory.

### key-path

Where the plugin will store the certificate keys.  Again it should match
`host-ssl-key` the vhost is trying to use.

The path should include at least one 0700 root:root directory.

### directory-url

This defines the URL of the certification server you will get your
certificates from.  For let's encrypt, they have a "practice" one

 - `https://acme-staging.api.letsencrypt.org/directory`

and they have a "real" one

 - `https://acme-v01.api.letsencrypt.org/directory`

the main difference is the CA certificate for the real one is in most browsers
already, but the staging one's CA certificate isn't.  The staging server will
also let you abuse it more in terms of repeated testing etc.

It's recommended you confirm expected operation with the staging directory-url,
and then switch to the "real" URL.

### common-name

Your server DNS name, like "libwebsockets.org".  The remote ACME server will
use this to find your server to perform the SNI challenges.

### email

The contact email address for the certificate.

## Optional PVOs

These are not included in the cert by letsencrypt

### country

Two-letter country code for the certificate

### state

State "or province" for the certificate

### locality

Locality for the certificate

### organization

Your company name

## Security / Key storage considerations

The `lws-acme-client` plugin is able to provision and update your certificate
and keys in an entirely root-only storage environment, even though lws runs
as a different uid / gid with no privileges to access the storage dir.

It does this by opening and holding two WRONLY fds on "update paths" inside the
root directory structure for each cert and key it manages; these are the normal
cert and key paths with `.upd` appended.  If during the time the server is up
the certs become within two weeks of expiry, the `lws-acme-client` plugin will
negotiate new certs and write them to the file descriptors.

Next time the server starts, if it sees `.upd` cert and keys, it will back up
the old ones and copy them into place as the new ones, before dropping privs.

To also handle the long-uptime server case, lws will update the vhost with the
new certs using in-memory temporary copies of the cert and key after updating
the cert.

In this way the cert and key live in root-only storage but the vhost is kept up
to date dynamically with any cert changes as well.

## Multiple vhosts using same cert

In the case you have multiple vhosts using of the same cert, just attach
the `lws-acme-client` plugin to one instance.  When the cert updates, all the
vhosts are informed and vhosts using the same filepath to access the cert will
be able to update their cert.

## Implementation point

You will need to remove the auth keys when switching from OpenSSL to
mbedTLS.  They will be regenerated automatically.  It's the file at this
path:

```
"auth-path":	   "/etc/lwsws/acme/auth.jwk",
```
