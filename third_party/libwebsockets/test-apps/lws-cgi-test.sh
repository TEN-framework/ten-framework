#!/bin/sh

echo -e -n "content-type: text/html\x0d\x0a"
echo -e -n "transfer-encoding: chunked\x0d\x0a"
echo -e -n "\x0d\x0a"

echo "<html><body>"
echo "<h1>lwstest script stdout</h1>"
>&2 echo "lwstest script stderr: $REQUEST_METHOD"

echo "<h2>REQUEST_METHOD=$REQUEST_METHOD</h2>"

if [ "$REQUEST_METHOD" = "POST" ] ; then
	>&2 echo "lwstest script stderr: doing read"
	echo "CONTENT_LENGTH=$CONTENT_LENGTH"
	read -n $CONTENT_LENGTH line
	>&2 echo "lwstest script stderr: done read"

	echo "read=\"$line\""
else
	echo "<table>"
	echo "<tr><td colspan=\"2\" style=\"font-size:120%;text-align:center\">/proc/meminfo</td></tr>"
	cat /proc/meminfo | while read line ; do
		A=`echo "$line" | cut -d: -f1`
		B=`echo "$line" | tr -s ' ' | cut -d' ' -f2-`
		echo -e "<tr><td style=\"background-color:#f0e8c0\">$A</td>"
		echo -e "<td style=\"text-align:right\">$B</td></tr>"
	done
	echo "</table>"
fi

echo "<br/>done"
echo "</body></html>"

exit 0

