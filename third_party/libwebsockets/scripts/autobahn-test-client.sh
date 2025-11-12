#!/bin/bash
#
# Requires pip install autobahntestsuite
#
# you should run this from ./build, after building with
# cmake .. -DLWS_WITH_MINIMAL_EXAMPLES=1
#
# It will use the minimal echo client and server to run
# autobahn ws tests as both client and server.

echo
echo "----------------------------------------------"
echo "-------   tests: autobahn as client"
echo

set -u

PARALLEL=1
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
#                   in flight in RFC6455.  lws doesn't waste memory on it
#                   since it is useless.
#
# 12.3.1 / 12.3.2
# 12.4.* / 12.5.*:  Autobahn has been broken for these tests since Aug 2017
#                   https://github.com/crossbario/autobahn-testsuite/issues/71


cat << EOF >fuzzingserver.json
{
   "url": "ws://127.0.0.1:9001",
   "outdir": "./reports/clients",
   "cases": ["*"],
   "exclude-cases": [ "2.10", "2.11", "12.3.1", "12.3.2", "12.4.*", "12.5.*"],
   "exclude-agent-cases": {}
}
EOF

PYTHONHASHSEED=0 wstest -m fuzzingserver &
Q=$!
sleep 2s
ps -p $Q > /dev/null
if [ $? -ne 0 ] ; then
	echo "Problem with autobahn wstest install"
	exit 9
fi

# 1) lws-as-client tests first

ok=1
while [ $ok -eq 1 ] ; do
		$CLIE -s 127.0.0.1 -p 9001 -u "/runCase?case=$N&agent=libwebsockets" -d3
		if [ $? -ne 0 ]; then
			ok=0
		fi
	N=$(( $N + 1 ))
done

# generate the report in ./reports
#
$CLIE -s 127.0.0.1 -p 9001 -u "/updateReports?agent=libwebsockets" -o -d3
sleep 2s
killall wstest
sleep 1s

# this squashes the results into single lines like
#
#  "9.8.4": { "behavior": "OK", "behaviorClose": "OK", "duration": 1312, "remoteCloseCode": 1000, "reportfile": "libwebsockets_case_9_8_4.json"

cat reports/clients/index.json | tr '\n' '!' | sed "s|\},\!|\n|g" | tr '!' ' ' | tr -s ' ' > /tmp/ji

echo -n "AUTOBAHN SERVER / LWS CLIENT: Total tests: " `cat /tmp/ji | wc -l` " : "
R="`cat /tmp/ji | grep -v '"behavior": "OK"' | grep -v '"behavior": "NON-STRICT"' | grep -v '"behavior": "INFORMATIONAL"' | wc -l`"
if [ "$R" == "0" ] ; then
	echo "All pass"
else
	RESULT=1
	echo -n "$R FAIL : "
	cat /tmp/ji | grep -v '"behavior": "OK"' | grep -v '"behavior": "NON-STRICT"' | grep -v '"behavior": "INFORMATIONAL"' | cut -d\" -f2 | tr '\n' ','
	echo
fi

echo $RESULT
exit $RESULT

