#!/bin/sh

if [ ! -z "$1" ] ; then
 OUT=$1
else
 OUT="/tmp/lws-abi-track-htdocs"
fi

D=`dirname $0`
if [ ! -z "$D" ] ; then
 D=$D/
fi
J=$D"libwebsockets.json"

abi-monitor -get -build-new $J
abi-tracker -build $J
abi-tracker -deploy $OUT $J
