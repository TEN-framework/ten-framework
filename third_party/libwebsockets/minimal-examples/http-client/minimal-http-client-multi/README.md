# lws minimal http client multi

## build

```
 $ cmake . && make
```

## usage

The application goes to https://warmcat.com and receives the page data
same as minimal http client.

However it does it for 8 client connections concurrently.

## Commandline Options

Option|Meaning
---|---
-s|Stagger the connections by 100ms, the last by 1s
-p|Use http/1.1 pipelining or h2 simultaneous streams
--h1|Force http/1 only
-l|Connect to server on https://localhost:7681 instead of https://warmcat.com:443
-n|Read numbered files like /1.png, /2.png etc.  Default is just read /
--uv|Use libuv event loop if lws built for it
--event|Use libevent event loop if lws built for it
--ev|Use libev event loop if lws built for it
--post|POST to the server rather than GET
-c<n>|Create n connections (n can be 1 .. 8)
--path <path>|Force the URL path (should start with /)