#!/bin/bash
#
# run from the build dir

echo
echo "----------------------------------------------"
echo "-------   tests: h2load SMP"
echo

PW=`pwd`

cd ../minimal-examples/http-server/minimal-http-server-smp
$PW/bin/lws-minimal-http-server-smp -s &
R=$!
sleep 0.5s

# check h1 with various loads

h2load -n 10000 -c 1 --h1 https://127.0.0.1:7681
if [ $? -ne 0 ] ; then
	Q=$?
	kill $R
	wait $R
	exit $Q
fi
h2load -n 10000 -c 10 --h1 https://127.0.0.1:7681
if [ $? -ne 0 ] ; then
	Q=$?
	kill $R
	wait $R
	exit $Q
fi
h2load -n 100000 -c 100 --h1 https://127.0.0.1:7681
if [ $? -ne 0 ] ; then
	Q=$?
	kill $R
	wait $R
	exit $Q
fi

# check h2 with various loads

h2load -n 10000 -c 1 https://127.0.0.1:7681
if [ $? -ne 0 ] ; then
	Q=$?
	kill $R
	wait $R
	exit $Q
fi
h2load -n 10000 -c 10 https://127.0.0.1:7681
if [ $? -ne 0 ] ; then
	Q=$?
	kill $R
	wait $R
	exit $Q
fi
h2load -n 100000 -c 100 https://127.0.0.1:7681
Q=$?

kill $R
wait $R
exit $Q

