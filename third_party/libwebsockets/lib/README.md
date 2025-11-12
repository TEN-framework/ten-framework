## Library sources layout

Code that goes in the libwebsockets library itself lives down ./lib

Path|Sources
---|---
lib/core|Core lws code related to generic fd and wsi servicing and management
lib/core-net|Core lws code that applies only if networking enabled
lib/event-libs|Code containing optional event-lib specific adaptations
lib/jose|JOSE / JWS / JWK / JWE implementations
lib/misc|Code for various mostly optional miscellaneous features
lib/plat|Platform-specific adaptation code
lib/roles|Code for specific optional wsi roles, eg, http/1, h2, ws, raw, etc
lib/system|Code for system-level features, eg, dhcpclient
lib/tls|Code supporting the various TLS libraries

