#!/bin/sh

openssl genrsa -out ca.key 2048 && \
printf "\\n\\n\\n\\n\\n\\n\\n" | \
openssl req -config tmp.cnf -x509 -new -nodes -key ca.key -sha256 -days 1024 -out ca.pem

