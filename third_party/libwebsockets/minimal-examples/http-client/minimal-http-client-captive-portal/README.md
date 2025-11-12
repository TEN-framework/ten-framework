# lws minimal http client captive portal detect

This demonstrates how to perform captive portal detection integrated
with `lws_system` states.

After reaching the `lws_system` DHCP state, the application tries to
connect through to `http://connectivitycheck.android.com/generate_204`
over http... if it succeeds, it will get a 204 response and set the
captive portal detection state to `LWS_CPD_INTERNET_OK` and perform
a GET from warmcat.com.

If there is a problem detected, the captive portal detection state is
set accordingly and the app will respond by exiting without trying the
read from warmcat.com.

The captive portal detection scheme is implemented in the user code
and can be modified according to the strategy that's desired for
captive portal detection.

## build

```
 $ cmake . && make
```

## usage

```
$ ./bin/lws-minimal-http-client-captive-portal
[2020/03/11 13:07:07:4519] U: LWS minimal http client captive portal detect
[2020/03/11 13:07:07:4519] N: lws_create_context: using ss proxy bind '(null)', port 0, ads '(null)'
[2020/03/11 13:07:07:5022] U: callback_cpd_http: established with resp 204
[2020/03/11 13:07:07:5023] U: app_system_state_nf: OPERATIONAL, cpd 1
[2020/03/11 13:07:07:5896] U: Connected to 46.105.127.147, http response: 200
[2020/03/11 13:07:07:5931] U: RECEIVE_CLIENT_HTTP_READ: read 4087
[2020/03/11 13:07:07:5931] U: RECEIVE_CLIENT_HTTP_READ: read 4096
[2020/03/11 13:07:07:6092] U: RECEIVE_CLIENT_HTTP_READ: read 4087
[2020/03/11 13:07:07:6092] U: RECEIVE_CLIENT_HTTP_READ: read 4096
[2020/03/11 13:07:07:6112] U: RECEIVE_CLIENT_HTTP_READ: read 4087
[2020/03/11 13:07:07:6113] U: RECEIVE_CLIENT_HTTP_READ: read 4096
[2020/03/11 13:07:07:6113] U: RECEIVE_CLIENT_HTTP_READ: read 2657
[2020/03/11 13:07:07:6113] U: LWS_CALLBACK_COMPLETED_CLIENT_HTTP
[2020/03/11 13:07:07:6119] U: main: finished OK
```

