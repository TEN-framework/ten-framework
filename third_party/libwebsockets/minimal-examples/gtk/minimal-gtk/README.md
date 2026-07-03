# lws minimal http client gtk

The application goes to https://warmcat.com and receives the page data,
from inside a gtk app using gtk / glib main loop directly.

## build

```
 $ cmake . && make
```

## usage


```
$
t1_main: started
[2020/02/08 18:04:07:6647] N: Loading client CA for verification ./warmcat.com.cer
[2020/02/08 18:04:07:7744] U: Connected to 46.105.127.147, http response: 200
[2020/02/08 18:04:07:7762] U: RECEIVE_CLIENT_HTTP_READ: read 4087
[2020/02/08 18:04:07:7762] U: RECEIVE_CLIENT_HTTP_READ: read 4096
[2020/02/08 18:04:07:7928] U: RECEIVE_CLIENT_HTTP_READ: read 4087
[2020/02/08 18:04:07:7929] U: RECEIVE_CLIENT_HTTP_READ: read 4096
[2020/02/08 18:04:07:7956] U: RECEIVE_CLIENT_HTTP_READ: read 4087
[2020/02/08 18:04:07:7956] U: RECEIVE_CLIENT_HTTP_READ: read 4096
[2020/02/08 18:04:07:7956] U: RECEIVE_CLIENT_HTTP_READ: read 1971
[2020/02/08 18:04:07:7956] U: LWS_CALLBACK_COMPLETED_CLIENT_HTTP
Hello World
$
```


