#!/bin/sh

echo -e -n "content-type: text/html\x0d\x0a"
echo -e -n "transfer-encoding: chunked\x0d\x0a"
echo -e -n "\x0d\x0a"

echo "<html><meta charset="UTF-8"><body>"
echo "<h1>lwstest script stdout</h1>"
>&2 echo -n "lwstest script stderr: REQUEST_METHOD was $REQUEST_METHOD"

echo "<h2>REQUEST_METHOD=$REQUEST_METHOD</h2>"

if [ "$REQUEST_METHOD" = "POST" ] ; then
	>&2 echo "lwstest script stderr: doing read"
	echo "CONTENT_LENGTH=$CONTENT_LENGTH"
	read -n $CONTENT_LENGTH line
	>&2 echo "lwstest script stderr: done read $line"

	echo "read=\"$line\""
else
	echo "<table>"
	echo "<tr><td colspan=\"2\">/proc/meminfo</td></tr>"
	cat /proc/meminfo | while read line ; do
		A=`echo "$line" | cut -d: -f1`
		B=`echo "$line" | tr -s ' ' | cut -d' ' -f2-`
		echo -e "<tr><td>$A</td>"
		echo -e "<td>$B</td></tr>"
	done
	echo "</table>"
fi

echo "<br/>done"
echo "</body></html>"
sleep 0.5
exit 0

