# lws minimal http server form POST

## build

```
 $ cmake . && make
```

## usage

```
 $ ./lws-minimal-http-server-form-post
[2018/03/29 08:29:41:7044] USER: LWS minimal http server form POST | visit http://localhost:7681
[2018/03/29 08:29:41:7044] NOTICE: Creating Vhost 'default' port 7681, 1 protocols, IPv6 off
[2018/03/29 08:29:49:8601] USER: text1: (len 4) 'xxxx'
[2018/03/29 08:29:49:8601] USER: send: (len 6) 'Submit'
```

Visit http://localhost:7681, submit the form.

The form parameters are dumped to the log and you are redirected to a different page.
