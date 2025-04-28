#!/bin/sh

if [ -z "$1" ] ; then
	echo "Usage $0 <name>"
	exit 1
fi

mkdir -p certs
openssl genrsa -out $1.key 4096 && \
printf "\\n\\n\\n\\n\\n$1\\n\\n1234\\n\\n" | \
 openssl req -config tmp.cnf -new -key $1.key -out $1.csr && \
openssl ca -config tmp.cnf \
 	-keyfile ca.key \
	-cert ca.pem \
	-extensions usr_cert \
	-days 375 \
	-notext \
	-md sha256 \
       	-in $1.csr \
	-out $1.pem && \
openssl pkcs12 -export -in $1.pem -inkey $1.key -out $1.p12

