## Secure Streams client C++ API

Enable for build by selecting `-DLWS_WITH_SECURE_STREAMS=1 -DLWS_WITH_SECURE_STREAMS_CPP=1` at
cmake.

Because it's designed for OpenSSL + system trust bundle, the minimal
example minimal-secure-streams-cpp requires `-DLWS_WITH_MINIMAL_EXAMPLES=1 -DLWS_WITH_MBEDTLS=0`

By default the -cpp example downloads https://warmcat.com/test-a.bin to the local
file /tmp/test-a.bin.  By giving, eg, -c 4, you can run four concurrent downloads of
files test-a.bin through test-d.bin... up to 12 files may be downloaded concurrently.

By default it will connect over h2 and share the single connection between all the
downloads.

### File level api

```
#include <libwebsockets.hxx>

...

	new lssFile(context, "https://warmcat.com/index.html",
			"/tmp/index.html", lss_completion, 0);
```

This will copy the remote url to the given local file, and call the
completion callback when it has succeeded or failed.

