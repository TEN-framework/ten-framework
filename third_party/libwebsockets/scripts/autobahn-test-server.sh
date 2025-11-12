#!/bin/bash
#
# Requires pip install autobahntestsuite
#
# you should run this from ./build, after building with
# cmake .. -DLWS_WITH_MINIMAL_EXAMPLES=1
#
# It will use the minimal echo client and server to run
# autobahn ws tests as both client and server.

set -u

PARALLEL=2
N=1
OS=`uname`

CLIE=bin/lws-minimal-ws-client-echo
SERV=bin/lws-minimal-ws-server-echo

RESULT=0

which wstest 2>/dev/null
if [ $? -ne 0 ]; then
	echo "wstest is not installed"
	exit 8
fi

killall wstest 2>/dev/null

#
# 2.10 / 2.11:      There is no requirement to handle multiple PING / PONG
#                   in flight on a single connection in RFC6455.  lws doesn't
#		    waste memory on supporting it since it is useless.

cat << EOF >fuzzingclient.json
{ 
   "outdir": "./reports/servers",
   "servers": [
      {
         "url": "ws://127.0.0.1:9001"
      }
   ],
   "cases": [ "*" ],
   "exclude-cases": ["2.10", "2.11" ],
   "exclude-agent-cases": {}
}
EOF

echo
echo "----------------------------------------------"
echo "-------   tests: autobahn as server"
echo

$SERV -p 9001 -d3 &
wstest -m fuzzingclient
R=$?
echo "Autobahn client exit $R"

killall lws-minimal-ws-server-echo
sleep 1s

# repeat the client results

R=`cat /tmp/ji | grep -v '"behavior": "OK"' | grep -v '"behavior": "NON-STRICT"' | grep -v '"behavior": "INFORMATIONAL"' | wc -l`
echo -n "AUTOBAHN SERVER / LWS CLIENT: Total tests: " `cat /tmp/ji | wc -l` " : "
if [ "$R" == "0" ] ;then
	echo "All pass"
else
	RESULT=1
	echo -n "$R FAIL : "
	cat /tmp/ji | grep -v '"behavior": "OK"' | grep -v '"behavior": "NON-STRICT"' | grep -v '"behavior": "INFORMATIONAL"' | cut -d\" -f2 | tr '\n' ','
	echo
fi

# and then the server results

cat reports/servers/index.json | tr '\n' '!' | sed "s|\},\!|\n|g" | tr '!' ' ' | tr -s ' ' > /tmp/jis
R=`cat /tmp/jis | grep -v '"behavior": "OK"' | grep -v '"behavior": "NON-STRICT"' | grep -v '"behavior": "INFORMATIONAL"' | wc -l`

echo -n "AUTOBAHN CLIENT / LWS SERVER: Total tests: " `cat /tmp/jis | wc -l` " : "
if [ "$R" == "0" ] ;then
	echo "All pass"
else
	RESULT=$(( $RESULT + 2 ))
	echo -n "$R FAIL : "
	cat /tmp/jis | grep -v '"behavior": "OK"' | grep -v '"behavior": "NON-STRICT"' | grep -v '"behavior": "INFORMATIONAL"' | cut -d\" -f2 | tr '\n' ','
	echo
fi

echo $RESULT
exit $RESULT

