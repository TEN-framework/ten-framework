#!/bin/sh

if [ -z "$1" ] ; then
	echo "Usage $0 <name>"
	exit 1
fi

mkdir -p certs
openssl genrsa -out $1.key 4096 && \
printf "\\n\\n\\n\\n\\nlocalhost\\n\\n1234\\n\\n" | \
 openssl req -config tmp.cnf -new -key $1.key -out $1.csr && \
openssl ca -config tmp.cnf \
 	-keyfile ca.key \
	-cert ca.pem \
	-extensions server_cert \
	-days 375 \
	-notext \
	-md sha256 \
       	-in $1.csr \
	-out $1.pem

