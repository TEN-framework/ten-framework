# Deaddrop: File upload and sharing plugin

## Building the plugin

Just configure lws with `cmake .. -DLWS_WITH_PLUGINS=1` and build lws as normal.

## Configurable settings

|pvo name|value meaning|
|---|---|
|upload-dir|A writeable directory where uploaded files will go|
|max-size|Maximum individual file size in bytes|
|basic-auth|Path to basic auth credential file so wss can also be protected|

## Required mounts

To use deaddrop meaningfully, all the mounts and the ws protocol must be
protected by basic auth.  And to use basic auth securely, the connection must
be protected from snooping by tls.

1) Set the basic-auth pvo to require valid credentials as described above

2) Protect your basic fileserving mount by the same basic auth file... this is
   used to serve index.html, the css etc.

3) Add a callback mount into "lws-deaddrop" protocol at "upload"... so if your
   URL for deaddrop is "/tools/share", this would be at "/tools/share/upload".
   It must also be protected by the basic auth file.

4) Add a fileserving mount at the url "get" (continuing the example above, it
   would be "/tools/share/get" whose origin matches the "upload-dir" pvo
   value you selected.  This mount needs any additional mimtype mappings since
   it's where the uploaded files are shared from.

## Using with C

See ./minimal-examples/http-server/minimal-example-http-server-deaddrop for
how to use the plugin directly with C.

## Using with lwsws / lejp-conf

As a plugin, you can configure the mounts and pvos per-vhost easily in JSON.

All the snippets here 

The mountpoints would look something like this (added to vhost/mounts)

```
	{
         "mountpoint": "/tools/share",
         "origin": "file:///var/www/deaddrop",
         "default": "index.html",
         "basic-auth": "/var/www/ba"
        }, {
         "mountpoint": "/tools/share/upload",
         "origin": "callback://lws-deaddrop",
         "basic-auth": "/var/www/ba"
        }, {
         "mountpoint": "/tools/share/get",
         "origin": "file:///var/cache/deaddrop-uploads",
         "basic-auth": "/var/www/ba",

	 "extra-mimetypes": {
		".bin": "application/octet-stream",
		".ttf": "application/x-font-truetype",
		".otf": "application/font-sfnt",
		".zip": "application/zip",
		".webm": "video/webm",
		".romfs": "application/octet-stream",
		".pdf": "application/pdf",
		".odt": "application/vnd.oasis.opendocument.text",
		".tgz": "application/x-gzip",
		".tar.gz": "application/x-gzip"
	  }
	}
```

This enables the plugin on the vhost, configures the pvos, and makes
the wss serving also depend on having a valid basic auth credential.

```
         "ws-protocols": [{
                  "lws-deaddrop": {
                  "status": "ok",
                  "upload-dir": "/var/cache/deaddrop-uploads",
                  "max-size": "52428800",
                  "basic-auth": "/var/www/ba"
                }
          }],
```

