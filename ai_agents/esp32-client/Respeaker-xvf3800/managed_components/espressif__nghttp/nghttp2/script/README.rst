fetch-ocsp-response is a Python script which performs OCSP query and
get response.  It uses openssl command under the hood.  nghttpx uses
it to enable OCSP stapling feature.

fetch-ocsp-response is a translation from original fetch-ocsp-response
written in Perl and which has been developed as part of h2o project
(https://github.com/h2o/h2o).

fetch-ocsp-response is usually installed under $(pkgdatadir), which is
$(prefix)/share/nghttp2.
