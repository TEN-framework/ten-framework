#!/bin/sh

echo "Starting $0"

bin/lws-minimal-dbus-ws-proxy 2> /tmp/dbuss&

echo "  server starting"
sleep 1s
PID_PROX=$!

echo "  client starting"
bin/lws-minimal-dbus-ws-proxy-testclient -x 10 2> /tmp/dbusc
R=$?

kill -2 $PID_PROX

if [ $R -ne 0 ] ; then
	echo "$0 FAILED"
	cat /tmp/dbuss
	cat /tmp/dbusc
	exit 1
fi

if [ -z "`cat /tmp/dbusc | grep 'rx: 9, tx: 9'`" ] ; then
	echo "$0 FAILED"
	cat /tmp/dbuss
	cat /tmp/dbusc
	exit 1
fi

echo "$0 PASSED"

exit 0

