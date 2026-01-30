Dockerfile
==========

Dockerfile creates the applications bundled with nghttp2.
These applications are:

- nghttp
- nghttpd
- nghttpx
- h2load

HTTP/3 and eBPF features are enabled.

In order to run nghttpx with HTTP/3 endpoint, you need to run the
image with the escalated privilege.  Here is the example command-line
to run nghttpx to listen to HTTP/3 on port 443, assuming that the
current directory contains a private key and a certificate in
server.key and server.crt respectively:

.. code-block:: text

   $ docker run --rm -it -v /path/to/certs:/shared --net=host --privileged \
         nghttp2 nghttpx \
         /shared/server.key /shared/server.crt \
         -f'*,443;quic'
