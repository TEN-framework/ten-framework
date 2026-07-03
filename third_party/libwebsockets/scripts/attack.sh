#!/bin/bash
#
# attack the test server and try to make it fall over
#
# Requires the library to have been built with
#
# cmake .. -DCMAKE_BUILD_TYPE=DEBUG -DLWS_WITH_MINIMAL_EXAMPLES=1
#
# run it from the build dir

echo
echo "----------------------------------------------"
echo "-------   tests: lws attack.sh"
echo

SERVER=127.0.0.1
PORT=7681
LOG=/tmp/lwslog

A=`which libwebsockets-test-server`
INSTALLED=`dirname $A`

SHAREDIR=$INSTALLED/../share/libwebsockets-test-server
CORPUS=$SHAREDIR/test.html

LWS_NC=./bin/lws-minimal-raw-netcat

CPID=
LEN=0

function check {
	kill -0 $CPID
	if [ $? -ne 0 ] ; then
		echo "(killed it) *******"
		exit 1
	fi
	#dd if=$LOG bs=1 skip=$LEN 2>/dev/null

	if [ "$1" = "default" ] ; then
		diff /tmp/lwscap $CORPUS > /dev/null
		if [ $? -ne 0 ] ; then
			echo "FAIL: got something other than $CORPUS back"
			exit 1
		fi
	fi
	if [ "$1" = "defaultplusforbidden" ] ; then
	cat $CORPUS > /tmp/plusforb
	echo -e -n "HTTP/1.0 403 Forbidden\x0d\x0acontent-type: text/html\x0d\x0acontent-length: 173\x0d\x0a\x0d\x0a<html><head><meta charset=utf-8 http-equiv=\"Content-Language\" content=\"en\"/><link rel=\"stylesheet\" type=\"text/css\" href=\"/error.css\"/></head><body><h1>403</h1></body></html>" >> /tmp/plusforb
		diff /tmp/lwscap /tmp/plusforb > /dev/null
		if [ $? -ne 0 ] ; then
			cat $CORPUS > /tmp/plusforb

			echo -e -n "HTTP/1.1 403 Forbidden\x0d\x0acontent-type: text/html\x0d\x0acontent-length: 173\x0d\x0a\x0d\x0a<html><head><meta charset=utf-8 http-equiv=\"Content-Language\" content=\"en\"/><link rel=\"stylesheet\" type=\"text/css\" href=\"/error.css\"/></head><body><h1>403</h1></body></html>" >> /tmp/plusforb
			diff /tmp/lwscap /tmp/plusforb > /dev/null
			if [ $? -ne 0 ] ; then

				echo "FAIL: got something other than $CORPUS + forbidden back"
				tail -n 10 /tmp/lwscap
				tail -n 100 $LOG
				exit 1
			fi
		fi
	fi

	if [ "$1" = "forbidden" ] ; then
		if [ -z "`grep '<h1>403</h1>' /tmp/lwscap`" ] ; then
			echo "FAIL: should have told forbidden (test server has no dirs)"
			exit 1
		fi
	fi

	if [ "$1" = "notfound" ] ; then
		if [ -z "`grep '<h1>404</h1>' /tmp/lwscap`" ] ; then
			echo "FAIL: should have told not found"
			exit 1
		fi
	fi


	if [ "$1" = "rejected" ] ; then
		if [ -z "`grep '<h1>404</h1>' /tmp/lwscap`" ] ; then
			echo "FAIL: should have told forbidden (test server has no dirs)"
			exit 1
		fi
	fi


	if [ "$1" = "media" ] ; then
		if [ -z "`grep '<h1>404</h1>' /tmp/lwscap`" ] ; then
			echo "FAIL: should have told unknown media type"
			exit 1
		fi
	fi

	if [ "$1" == "0" ] ; then
		a="`dd if=$LOG bs=1 skip=$LEN 2>/dev/null |grep "get\ \ =" | tr -s ' ' | cut -d' ' -f4-`"
		if [ "$a" != "$2" ] ; then
			echo "URL path '$a' not $2"
			exit 1
		fi
	fi

	if [ "$1" == "1" ] ; then
		a="`dd if=$LOG bs=1 skip=$LEN 2>/dev/null |grep URI\ Arg\ 1\: | tr -s ' ' | cut -d' ' -f7-`"
		if [ "$a" != "$2" ] ; then
			echo "Arg 1 '$a' not $2"
			exit 1
		fi
	fi

	if [ "$1" == "2" ] ; then
		a="`dd if=$LOG bs=1 skip=$LEN 2>/dev/null |grep URI\ Arg\ 2\: | tr -s ' ' | cut -d' ' -f7-`"
		if [ "$a" != "$2" ] ; then
			echo "Arg 2 '$a' not $2"
			exit 1
		fi
	fi
	if [ "$1" == "3" ] ; then
		a="`dd if=$LOG bs=1 skip=$LEN 2>/dev/null |grep URI\ Arg\ 3\: | tr -s ' ' | cut -d' ' -f7-`"
		if [ "$a" != "$2" ] ; then
			echo "Arg 3 '$a' not $2"
			exit 1
		fi
	fi

	if [ -z "$1" ] ; then
		LEN=`stat $LOG -c %s`
	fi
}


rm -rf $LOG
killall libwebsockets-test-server 2>/dev/null
libwebsockets-test-server -d15 2>> $LOG >/dev/null &
CPID=$!

echo "Started server on PID $CPID"

while [ -z "`grep ort\ 7681 $LOG`" ] ; do
	sleep 0.5s
done
check

echo
echo "---- /cgi-bin/settingsjs?UPDATE_SETTINGS=1&Root_Channels_1_Channel_name_http_post=%3F&Root_Channels_1_Channel_location_http_post=%3F"
rm -f /tmp/lwscap
echo -n -e "GET /cgi-bin/settingsjs?UPDATE_SETTINGS=1&Root_Channels_1_Channel_name_http_post=%3F&Root_Channels_1_Channel_location_http_post=%3F HTTP/1.0\x0d\x0a\x0d\x0a" | $LWS_NC --server $SERVER --port $PORT 2>/dev/null | sed '1,/^\r$/d'> /tmp/lwscap
cat /tmp/lwscap
check 1 "UPDATE_SETTINGS=1"
check 2 "Root_Channels_1_Channel_name_http_post=?"
check 3 "Root_Channels_1_Channel_location_http_post=?"
check

echo
echo "---- ? processing (/cgi-bin/settings.js?key1=value1)"
rm -f /tmp/lwscap
echo -n -e "GET /cgi-bin/settings.js?key1=value1 HTTP/1.0\x0d\x0a\x0d\x0a" | $LWS_NC --server $SERVER --port $PORT 2>/dev/null | sed '1,/^\r$/d'> /tmp/lwscap
check 1 "key1=value1"
check

echo
echo "---- ? processing (/t%3dest?key1%3d2=value1)"
rm -f /tmp/lwscap
echo -n -e "GET /t%3dest?key1%3d2=value1 HTTP/1.0\x0d\x0a\x0d\x0a" | $LWS_NC --server $SERVER --port $PORT 2>/dev/null | sed '1,/^\r$/d'> /tmp/lwscap
check 0 "/t=est"
check 1 "key1_2=value1"
check

echo
echo "---- ? processing (%2f%2e%2e%2f%2e./xxtest.html?arg=1)"
rm -f /tmp/lwscap
echo  -n -e "GET %2f%2e%2e%2f%2e./xxtest.html?arg=1 HTTP/1.0\x0d\x0a\x0d\x0a" | $LWS_NC --server $SERVER --port $PORT 2>/dev/null | sed '1,/^\r$/d'> /tmp/lwscap
check 1 "arg=1"
check

echo
echo "---- ? processing (%2f%2e%2e%2f%2e./xxtest.html?arg=/../.)"
rm -f /tmp/lwscap
echo -n -e "GET %2f%2e%2e%2f%2e./xxtest.html?arg=/../. HTTP/1.0\x0d\x0a\x0d\x0a" | $LWS_NC --server $SERVER --port $PORT 2>/dev/null | sed '1,/^\r$/d'> /tmp/lwscap
check 1 "arg=/../."
check

echo
echo "---- spam enough crap to not be GET"
echo "not GET" | $LWS_NC --server $SERVER --port $PORT 2>/dev/null > /tmp/lwscap
check

echo
echo "---- spam more than the name buffer of crap"
dd if=/dev/urandom bs=1 count=80 2>/dev/null | $LWS_NC --server $SERVER --port $PORT 2>/dev/null > /tmp/lwscap
check

echo
echo "---- spam 10MB of crap"
dd if=/dev/urandom bs=1 count=655360 | $LWS_NC --server $SERVER --port $PORT 2>/dev/null > /tmp/lwscap
check

echo
echo "---- malformed URI"
echo "GET nonsense................................................................................................................" \
	| $LWS_NC --server $SERVER --port $PORT 2>/dev/null > /tmp/lwscap
check

echo
echo "---- missing URI"
echo -n -e "GET HTTP/1.0\x0d\x0a\x0d\x0a" | $LWS_NC --server $SERVER --port $PORT 2>/dev/null >/tmp/lwscap
check

echo
echo "---- repeated method"
echo -n -e "GET blah HTTP/1.0\x0d\x0aGET blah HTTP/1.0\x0d\x0a\x0d\x0a" | $LWS_NC --server $SERVER --port $PORT 2>/dev/null >/tmp/lwscap 
check

echo
echo "---- crazy header name part"
echo -n -e "GET blah HTTP/1.0\x0d\x0a................................................................................................................" \
	"......................................................................................................................." \
 	"......................................................................................................................." \
 	"......................................................................................................................." \
 	"......................................................................................................................." \
 	"......................................................................................................................." \
 	"......................................................................................................................." \
 	"......................................................................................................................." \
 	"......................................................................................................................." \
 	"......................................................................................................................." \
	"......................................................................................................................." \
 	"......................................................................................................................." \
 	"......................................................................................................................." \
 	"......................................................................................................................." \
 	"......................................................................................................................." \
 	"......................................................................................................................." \
 	"......................................................................................................................." \
 | $LWS_NC --server $SERVER --port $PORT 2>/dev/null
check

echo
echo "---- excessive uri content"
echo -n -e "GET ................................................................................................................" \
	"......................................................................................................................." \
 	"......................................................................................................................." \
 	"......................................................................................................................." \
 	"......................................................................................................................." \
 	"......................................................................................................................." \
 	"......................................................................................................................." \
 	"......................................................................................................................." \
 	"......................................................................................................................." \
 	"......................................................................................................................." \
	"......................................................................................................................." \
 	"......................................................................................................................." \
 	"......................................................................................................................." \
 	"......................................................................................................................." \
 	"......................................................................................................................." \
 	"......................................................................................................................." \
 	"......................................................................................................................." \
 | $LWS_NC --server $SERVER --port $PORT 2>/dev/null
check

echo
echo "---- good request but http payload coming too (test.html served then forbidden)"
echo -n -e "GET /test.html HTTP/1.1\x0d\x0a\x0d\x0aILLEGAL-PAYLOAD........................................" \
	| $LWS_NC --server $SERVER --port $PORT 2>/dev/null | sed '1,/^\r$/d'> /tmp/lwscap
check defaultplusforbidden
check

echo
echo "---- nonexistent file"
rm -f /tmp/lwscap
echo -n -e "GET /nope HTTP/1.0\x0d\x0a\x0d\x0a" | $LWS_NC --server $SERVER --port $PORT 2>/dev/null | sed '1,/^\r$/d'> /tmp/lwscap
cat /tmp/lwscap
check notfound
check

echo
echo "---- relative uri path"
rm -f /tmp/lwscap
echo -n -e "GET nope HTTP/1.0\x0d\x0a\x0d\x0a" | $LWS_NC --server $SERVER --port $PORT 2>/dev/null | sed '1,/^\r$/d'> /tmp/lwscap
check forbidden
check

echo
echo "---- directory attack 1 (/../../../../etc/passwd should be /etc/passswd)"
rm -f /tmp/lwscap
echo -n -e "GET /../../../../etc/passwd HTTP/1.0\x0d\x0a\x0d\x0a" | $LWS_NC --server $SERVER --port $PORT 2>/dev/null | sed '1,/^\r$/d'> /tmp/lwscap
check notfound
check

echo
echo "---- directory attack 2 (/../ should be /)"
rm -f /tmp/lwscap
echo -e -n "GET /../ HTTP/1.0\x0d\x0a\x0d\x0a" | $LWS_NC --server $SERVER --port $PORT 2>/dev/null | sed '1,/^\r$/d'> /tmp/lwscap
check default
check

echo
echo "---- directory attack 3 (/./ should be /)"
rm -f /tmp/lwscap
echo -e -n "GET /./ HTTP/1.0\x0d\x0a\x0d\x0a" | $LWS_NC --server $SERVER --port $PORT 2>/dev/null | sed '1,/^\r$/d'> /tmp/lwscap
check default
check

echo
echo "---- directory attack 4 (/blah/.. should be /)"
rm -f /tmp/lwscap
echo -e -n "GET /blah/.. HTTP/1.0\x0d\x0a\x0d\x0a" | $LWS_NC --server $SERVER --port $PORT 2>/dev/null | sed '1,/^\r$/d'> /tmp/lwscap
check default
check

echo
echo "---- directory attack 5 (/blah/../ should be /)"
rm -f /tmp/lwscap
echo -e -n "GET /blah/../ HTTP/1.0\x0d\x0a\x0d\x0a" | $LWS_NC --server $SERVER --port $PORT 2>/dev/null | sed '1,/^\r$/d'> /tmp/lwscap
check default
check

echo
echo "---- directory attack 6 (/blah/../. should be /)"
rm -f /tmp/lwscap
echo -e -n "GET /blah/../. HTTP/1.0\x0d\x0a\x0d\x0a" | $LWS_NC --server $SERVER --port $PORT 2>/dev/null | sed '1,/^\r$/d'> /tmp/lwscap
check default
check

echo
echo "---- directory attack 7 (/%2e%2e%2f../../../etc/passwd should be /etc/passswd)"
rm -f /tmp/lwscap
echo -e -n "GET /%2e%2e%2f../../../etc/passwd HTTP/1.0\x0d\x0a\x0d\x0a" | $LWS_NC --server $SERVER --port $PORT 2>/dev/null | sed '1,/^\r$/d'> /tmp/lwscap
check notfound
check

echo
echo "---- directory attack 8 (%2f%2e%2e%2f%2e./.%2e/.%2e%2fetc/passwd should be /etc/passswd)"
rm -f /tmp/lwscap
echo -e -n "GET %2f%2e%2e%2f%2e./.%2e/.%2e%2fetc/passwd HTTP/1.0\x0d\x0a\x0d\x0a" | $LWS_NC --server $SERVER --port $PORT 2>/dev/null | sed '1,/^\r$/d'> /tmp/lwscap
check notfound
check

echo
echo "---- http/1.1 pipelining"
rm -f /tmp/lwscap
wget -O/tmp/lwsdump http://localhost:7681/test.html http://localhost:7681/test.html http://localhost:7681/test.html http://localhost:7681/test.html http://localhost:7681/test.html http://localhost:7681/test.html http://localhost:7681/test.html http://localhost:7681/test.html 2>&1 | grep "Downloaded: 8 files" > /tmp/lwscap
good=`cat $CORPUS $CORPUS $CORPUS $CORPUS $CORPUS $CORPUS $CORPUS $CORPUS | md5sum | cut -d' ' -f1`
if [ "$good" != "`md5sum /tmp/lwsdump | cut -d' ' -f 1`" ] ; then
	echo "FAIL: mismatched content good=$good received=`md5sum /tmp/lwsdump`"
	exit 1
fi

echo
echo "---- mass testing uri variations"

rm -f /tmp/results

for i in \
/..../ \
/.../. \
/...// \
/.../a \
/.../w \
"/.../?" \
/.../% \
/../.. \
/.././ \
/../.a \
/../.w \
/../.. \
/../.% \
/..//. \
/../// \
/..//a \
/..//w \
"/..//?" \
/..//% \
/../a. \
/../a/ \
/../aa \
/../aw \
/../a? \
/../a% \
/../w. \
/../w/ \
/../wa \
/../ww \
/../w? \
/../w% \
/../?. \
/../?/ \
/../?a \
/../?w \
/../?? \
/../?% \
/../%. \
/../%/ \
/../%a \
/../%w \
/../%? \
/../%% \
/./... \
/./../ \
/./..a \
/./..w \
/./..? \
/./..% \
/.//.. \
/.a../ \
/.a/.. \
/.w../ \
/.w/.. \
/.?../ \
/../.. \
/.%../ \
/.%/.. \
//.... \
//.../ \
//...a \
//...w \
//...? \
//...% \
//../. \
//..// \
//../a \
//../w \
//../? \
//../% \
//..a. \
//..a/ \
//..aa \
//..aw \
//..a? \
//..a% \
//..w. \
//..w/ \
//..wa \
//..ww \
//..w? \
//..w% \
//..?. \
//..?/ \
//..?a \
//..?w \
//..?? \
//..?% \
//..%. \
//..%/ \
//..%a \
//..%w \
//..%? \
//..%% \
//./.. \
///... \
///../ \
///..a \
///..w \
///..? \
///..% \
////.. \
//a../ \
//a/.. \
//w../ \
//w/.. \
//?../ \
//?/.. \
//%../ \
//%/.. \
/a.../ \
/a../. \
/a..// \
/a../a \
/a../w \
/a../? \
/a../% \
/a./.. \
/a/... \
/a/../ \
/a/..a \
/a/..w \
/a/..? \
/a/..% \
/a//.. \
/aa../ \
/aa/.. \
/aw../ \
/aw/.. \
/a?../ \
/a?/.. \
/a%../ \
/a%/.. \
/w.../ \
/w../. \
/w..// \
/w../a \
/w../w \
/w../? \
/w../% \
/w./.. \
/w/... \
/w/../ \
/w/..a \
/w/..w \
/w/..? \
/w/..% \
/w//.. \
/wa../ \
/wa/.. \
/ww../ \
/ww/.. \
/w?../ \
/w?/.. \
/w%../ \
/w%/.. \
/?.../ \
/?../. \
/?..// \
/?../a \
/?../w \
/?../? \
/?../% \
/?./.. \
/?/... \
/?/../ \
/?/..a \
/?/..w \
/?/..? \
/?/..% \
/?//.. \
/?a../ \
/?a/.. \
/?w../ \
/?w/.. \
/??../ \
/??/.. \
/?%../ \
/?%/.. \
/%.../ \
/%../. \
/%..// \
/%../a \
/%../w \
/%../? \
/%../% \
/%./.. \
/%/... \
/%/../ \
/%/..a \
/%/..w \
/%/..? \
/%/..% \
/%//.. \
/%a../ \
/%a/.. \
/%w../ \
/%w/.. \
/%?../ \
/%?/.. \
/%%../ \
/%%/.. \
/a/w/../a \
/path/to/dir/../other/dir \
; do
LEN=`stat $LOG -c %s`
rm -f /tmp/lwscap1
echo -n -e "GET $i HTTP/1.0\r\n\r\n" | $LWS_NC --server $SERVER --port $PORT 2>/dev/null > /tmp/lwscap1
R=`cat /tmp/lwscap1| head -n 1 | cut -d' ' -f 2`
#cat $LOG
#echo ==== $R


if [ "$R" != "403" ]; then
	U=`dd if=$LOG bs=1 skip=$LEN 2>/dev/null| grep "Method:" | tr -s ' ' | cut -d"'" -f4`
#dd if=$LOG bs=1 skip=$LEN 2>/dev/null
	echo "- \"$i\" -> $R \"$U\"" >>/tmp/results
else
	echo "- \"$i\" -> $R" >>/tmp/results
fi
done

cat <<EOF >/tmp/lwsresult1
- "/..../" -> 404 "/..../"
- "/.../." -> 404 "/.../"
- "/...//" -> 404 "/.../"
- "/.../a" -> 404 "/.../a"
- "/.../w" -> 404 "/.../w"
- "/.../?" -> 404 "/.../"
- "/.../%" -> 403
- "/../.." -> 200 "/"
- "/.././" -> 200 "/"
- "/../.a" -> 404 "/.a"
- "/../.w" -> 404 "/.w"
- "/../.." -> 200 "/"
- "/../.%" -> 403
- "/..//." -> 200 "/"
- "/..///" -> 200 "/"
- "/..//a" -> 404 "/a"
- "/..//w" -> 404 "/w"
- "/..//?" -> 200 "/"
- "/..//%" -> 403
- "/../a." -> 404 "/a."
- "/../a/" -> 404 "/a/"
- "/../aa" -> 404 "/aa"
- "/../aw" -> 404 "/aw"
- "/../a?" -> 404 "/a"
- "/../a%" -> 403
- "/../w." -> 404 "/w."
- "/../w/" -> 404 "/w/"
- "/../wa" -> 404 "/wa"
- "/../ww" -> 404 "/ww"
- "/../w?" -> 404 "/w"
- "/../w%" -> 403
- "/../?." -> 200 "/"
- "/../?/" -> 200 "/"
- "/../?a" -> 200 "/"
- "/../?w" -> 200 "/"
- "/../??" -> 200 "/"
- "/../?%" -> 403
- "/../%." -> 403
- "/../%/" -> 403
- "/../%a" -> 403
- "/../%w" -> 403
- "/../%?" -> 403
- "/../%%" -> 403
- "/./..." -> 404 "/..."
- "/./../" -> 200 "/"
- "/./..a" -> 404 "/..a"
- "/./..w" -> 404 "/..w"
- "/./..?" -> 200 "/"
- "/./..%" -> 403
- "/.//.." -> 200 "/"
- "/.a../" -> 404 "/.a../"
- "/.a/.." -> 200 "/"
- "/.w../" -> 404 "/.w../"
- "/.w/.." -> 200 "/"
- "/.?../" -> 404 "/."
- "/../.." -> 200 "/"
- "/.%../" -> 403
- "/.%/.." -> 403
- "//...." -> 404 "/...."
- "//.../" -> 404 "/.../"
- "//...a" -> 404 "/...a"
- "//...w" -> 404 "/...w"
- "//...?" -> 404 "/..."
- "//...%" -> 403
- "//../." -> 200 "/"
- "//..//" -> 200 "/"
- "//../a" -> 404 "/a"
- "//../w" -> 404 "/w"
- "//../?" -> 200 "/"
- "//../%" -> 403
- "//..a." -> 404 "/..a."
- "//..a/" -> 404 "/..a/"
- "//..aa" -> 404 "/..aa"
- "//..aw" -> 404 "/..aw"
- "//..a?" -> 404 "/..a"
- "//..a%" -> 403
- "//..w." -> 404 "/..w."
- "//..w/" -> 404 "/..w/"
- "//..wa" -> 404 "/..wa"
- "//..ww" -> 404 "/..ww"
- "//..w?" -> 404 "/..w"
- "//..w%" -> 403
- "//..?." -> 200 "/"
- "//..?/" -> 200 "/"
- "//..?a" -> 404 "/a"
- "//..?w" -> 404 "/w"
- "//..??" -> 200 "/"
- "//..?%" -> 403
- "//..%." -> 403
- "//..%/" -> 403
- "//..%a" -> 403
- "//..%w" -> 403
- "//..%?" -> 403
- "//..%%" -> 403
- "//./.." -> 200 "/"
- "///..." -> 404 "/..."
- "///../" -> 200 "/"
- "///..a" -> 404 "/..a"
- "///..w" -> 404 "/..w"
- "///..?" -> 200 "/"
- "///..%" -> 403
- "////.." -> 200 "/"
- "//a../" -> 404 "/a../"
- "//a/.." -> 200 "/"
- "//w../" -> 404 "/w../"
- "//w/.." -> 200 "/"
- "//?../" -> 200 "/"
- "//?/.." -> 200 "/"
- "//%../" -> 403
- "//%/.." -> 403
- "/a.../" -> 404 "/a.../"
- "/a../." -> 404 "/a../"
- "/a..//" -> 404 "/a../"
- "/a../a" -> 404 "/a../a"
- "/a../w" -> 404 "/a../w"
- "/a../?" -> 404 "/a../"
- "/a../%" -> 403
- "/a./.." -> 200 "/"
- "/a/..." -> 404 "/a/..."
- "/a/../" -> 200 "/"
- "/a/..a" -> 404 "/a/..a"
- "/a/..w" -> 404 "/a/..w"
- "/a/..?" -> 200 "/"
- "/a/..%" -> 403
- "/a//.." -> 200 "/"
- "/aa../" -> 404 "/aa../"
- "/aa/.." -> 200 "/"
- "/aw../" -> 404 "/aw../"
- "/aw/.." -> 200 "/"
- "/a?../" -> 404 "/a"
- "/a?/.." -> 404 "/a"
- "/a%../" -> 403
- "/a%/.." -> 403
- "/w.../" -> 404 "/w.../"
- "/w../." -> 404 "/w../"
- "/w..//" -> 404 "/w../"
- "/w../a" -> 404 "/w../a"
- "/w../w" -> 404 "/w../w"
- "/w../?" -> 404 "/w../"
- "/w../%" -> 403
- "/w./.." -> 200 "/"
- "/w/..." -> 404 "/w/..."
- "/w/../" -> 200 "/"
- "/w/..a" -> 404 "/w/..a"
- "/w/..w" -> 404 "/w/..w"
- "/w/..?" -> 200 "/"
- "/w/..%" -> 403
- "/w//.." -> 200 "/"
- "/wa../" -> 404 "/wa../"
- "/wa/.." -> 200 "/"
- "/ww../" -> 404 "/ww../"
- "/ww/.." -> 200 "/"
- "/w?../" -> 404 "/w"
- "/w?/.." -> 404 "/w"
- "/w%../" -> 403
- "/w%/.." -> 403
- "/?.../" -> 200 "/"
- "/?../." -> 200 "/"
- "/?..//" -> 200 "/"
- "/?../a" -> 200 "/"
- "/?../w" -> 200 "/"
- "/?../?" -> 200 "/"
- "/?../%" -> 403
- "/?./.." -> 200 "/"
- "/?/..." -> 200 "/"
- "/?/../" -> 200 "/"
- "/?/..a" -> 200 "/"
- "/?/..w" -> 200 "/"
- "/?/..?" -> 200 "/"
- "/?/..%" -> 403
- "/?//.." -> 200 "/"
- "/?a../" -> 200 "/"
- "/?a/.." -> 200 "/"
- "/?w../" -> 200 "/"
- "/?w/.." -> 200 "/"
- "/??../" -> 200 "/"
- "/??/.." -> 200 "/"
- "/?%../" -> 403
- "/?%/.." -> 403
- "/%.../" -> 403
- "/%../." -> 403
- "/%..//" -> 403
- "/%../a" -> 403
- "/%../w" -> 403
- "/%../?" -> 403
- "/%../%" -> 403
- "/%./.." -> 403
- "/%/..." -> 403
- "/%/../" -> 403
- "/%/..a" -> 403
- "/%/..w" -> 403
- "/%/..?" -> 403
- "/%/..%" -> 403
- "/%//.." -> 403
- "/%a../" -> 403
- "/%a/.." -> 403
- "/%w../" -> 403
- "/%w/.." -> 403
- "/%?../" -> 403
- "/%?/.." -> 403
- "/%%../" -> 403
- "/%%/.." -> 403
- "/a/w/../a" -> 404 "/a/a"
- "/path/to/dir/../other/dir" -> 404 "/path/to/other/dir"
EOF

if [ "`md5sum /tmp/results | cut -d' ' -f 1`" != "`md5sum /tmp/lwsresult1 | cut -d' ' -f1`" ] ; then
	echo "Differences..."
	diff -urN /tmp/lwsresult1 /tmp/results
	cat /tmp/lwscap1
	ls -l /tmp/results
	cat /tmp/results
# this is currently broken on travis
#	exit 1
else
	echo "OK"
fi


echo
echo "--- survived OK ---"
kill -2 $CPID

exit 0

# coverage...
# run the test client against mirror for one period and exit
killall libwebsockets-test-server 2>/dev/null
libwebsockets-test-server -s 2>> $LOG &
CPID=$!
sleep 1s
libwebsockets-test-client 127.0.0.1 -s -O

# https://github.com/curl/curl/issues/1587
curl -v -F text=hello -F send=SEND -F upload=@../README.md https://127.0.0.1:7681/formtest -k

kill -2 $CPID

exit 0


