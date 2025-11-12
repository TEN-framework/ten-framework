# lws minimal http server form POST file

## build

```
 $ cmake . && make
```

## usage

```
 $ ./lws-minimal-http-server-form-post-file
[2018/03/29 09:58:30:8800] USER: LWS minimal http server POST file | visit http://localhost:7681
[2018/03/29 09:58:30:8800] NOTICE: Creating Vhost 'default' port 7681, 1 protocols, IPv6 off
[2018/03/29 09:58:45:3284] USER: file_upload_cb: upload done, written 2729 to wss-over-h2.png
[2018/03/29 09:58:45:3284] USER: text1: (len 3) 'xxx'
[2018/03/29 09:58:45:3284] USER: send: (len 6) 'Submit'
```

Visit http://localhost:7681, select a file to upload and submit the form.

The file is uploaded and saved in the cwd, the form parameters are dumped to the log and
you are redirected to a different page.
