# lws minimal http server-cgi

## build

```
 $ cmake . && make
```

## usage

This example runs a script ./my-cgi-script.sh when you vist /

The script dumps some information from /proc on stdout, which
is proxied back to the browser, script output on stderr is
printed in the console.

It's able to serve the script output over h1 using chunked encoding,
and over h2 having stripped the chunked encoding from the script
output.

```
 $ ./lws-minimal-http-server-cgi
[2019/11/18 16:31:29:5481] U: LWS minimal http server | visit http://localhost:7681
[2019/11/18 16:31:40:2176] N: CGI-stderr: lwstest script stderr: REQUEST_METHOD was GET
```

Visit http://localhost:7681

