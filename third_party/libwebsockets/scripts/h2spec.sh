#!/bin/bash
#
# run from the build subdir
#

echo
echo "----------------------------------------------"
echo "-------   tests: h2spec"
echo


if [ ! -e h2spec ] ; then
	wget https://github.com/summerwind/h2spec/releases/download/v2.1.0/h2spec_linux_amd64.tar.gz &&\
	tar xf h2spec_linux_amd64.tar.gz
	if [ ! -e h2spec ] ; then
		echo "Couldn't get h2spec"
		exit 1
	fi
fi

cd ../minimal-examples/http-server/minimal-http-server-tls
../../../build/bin/lws-minimal-http-server-tls&

sleep 1s

P=$!
../../../build/h2spec -h 127.0.0.1 -p 7681 -t -k -S > /tmp/hlog
kill $P 2>/dev/null
wait $P 2>/dev/null

if [ ! -z "`cat /tmp/hlog | grep "Failures:"`" ] ; then
	cat /tmp/hlog | sed '/Failures:/,$!d'

	exit 1
fi

cat /tmp/hlog | sed '/Finished\ in/,$!d'


exit 0

