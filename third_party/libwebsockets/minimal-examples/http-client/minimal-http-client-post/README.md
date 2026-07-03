# lws minimal http client POST

This example demonstrates a multipart POST to

https://libwebsockets.org/testserver/formtest

setting both a form variable and uploading a
short file.

The result of the POST form processing is captured
and displayed in a hexdump.

This is programmatically POSTing to the same
form you can access at

https://libwebsockets.org/testserver

in the "POST" tab with file upload.

By default the client action occurs using http/2 if
your lws was built with `-DLWS_WITH_HTTP2=1`.

## build

```
 $ cmake . && make
```

## usage

```
 $ ./lws-minimal-http-client-post
[2018/04/03 13:13:10:7891] USER: LWS minimal http client - POST
[2018/04/03 13:13:10:7905] NOTICE: Creating Vhost 'default' (serving disabled), 1 protocols, IPv6 on
[2018/04/03 13:13:10:7984] NOTICE: created client ssl context for default
[2018/04/03 13:13:12:8444] USER: LWS_CALLBACK_CLIENT_APPEND_HANDSHAKE_HEADER
[2018/04/03 13:13:12:8444] USER: LWS_CALLBACK_CLIENT_HTTP_WRITEABLE
[2018/04/03 13:13:12:8445] USER: LWS_CALLBACK_CLIENT_HTTP_WRITEABLE
[2018/04/03 13:13:12:8445] USER: LWS_CALLBACK_CLIENT_HTTP_WRITEABLE
[2018/04/03 13:13:13:1437] USER: LWS_CALLBACK_CLIENT_HTTP_WRITEABLE
[2018/04/03 13:13:13:1440] USER: LWS_CALLBACK_CLIENT_HTTP_WRITEABLE
[2018/04/03 13:13:13:1440] USER: RECEIVE_CLIENT_HTTP_READ: read 402
[2018/04/03 13:13:13:1441] NOTICE: 
[2018/04/03 13:13:13:1441] NOTICE: 0000: 3C 68 74 6D 6C 3E 3C 62 6F 64 79 3E 3C 68 31 3E    <html><body><h1>
[2018/04/03 13:13:13:1441] NOTICE: 0010: 46 6F 72 6D 20 72 65 73 75 6C 74 73 20 28 61 66    Form results (af
[2018/04/03 13:13:13:1441] NOTICE: 0020: 74 65 72 20 75 72 6C 64 65 63 6F 64 69 6E 67 29    ter urldecoding)
[2018/04/03 13:13:13:1441] NOTICE: 0030: 3C 2F 68 31 3E 3C 74 61 62 6C 65 3E 3C 74 72 3E    </h1><table><tr>
[2018/04/03 13:13:13:1441] NOTICE: 0040: 3C 74 64 3E 4E 61 6D 65 3C 2F 74 64 3E 3C 74 64    <td>Name</td><td
[2018/04/03 13:13:13:1441] NOTICE: 0050: 3E 4C 65 6E 67 74 68 3C 2F 74 64 3E 3C 74 64 3E    >Length</td><td>
[2018/04/03 13:13:13:1441] NOTICE: 0060: 56 61 6C 75 65 3C 2F 74 64 3E 3C 2F 74 72 3E 3C    Value</td></tr><
[2018/04/03 13:13:13:1441] NOTICE: 0070: 74 72 3E 3C 74 64 3E 3C 62 3E 74 65 78 74 3C 2F    tr><td><b>text</
[2018/04/03 13:13:13:1441] NOTICE: 0080: 62 3E 3C 2F 74 64 3E 3C 74 64 3E 31 33 3C 2F 74    b></td><td>13</t
[2018/04/03 13:13:13:1441] NOTICE: 0090: 64 3E 3C 74 64 3E 6D 79 20 74 65 78 74 20 66 69    d><td>my text fi
[2018/04/03 13:13:13:1441] NOTICE: 00A0: 65 6C 64 3C 2F 74 64 3E 3C 2F 74 72 3E 3C 74 72    eld</td></tr><tr
[2018/04/03 13:13:13:1441] NOTICE: 00B0: 3E 3C 74 64 3E 3C 62 3E 73 65 6E 64 3C 2F 62 3E    ><td><b>send</b>
[2018/04/03 13:13:13:1441] NOTICE: 00C0: 3C 2F 74 64 3E 3C 74 64 3E 30 3C 2F 74 64 3E 3C    </td><td>0</td><
[2018/04/03 13:13:13:1442] NOTICE: 00D0: 74 64 3E 4E 55 4C 4C 3C 2F 74 64 3E 3C 2F 74 72    td>NULL</td></tr
[2018/04/03 13:13:13:1442] NOTICE: 00E0: 3E 3C 74 72 3E 3C 74 64 3E 3C 62 3E 66 69 6C 65    ><tr><td><b>file
[2018/04/03 13:13:13:1442] NOTICE: 00F0: 3C 2F 62 3E 3C 2F 74 64 3E 3C 74 64 3E 30 3C 2F    </b></td><td>0</
[2018/04/03 13:13:13:1442] NOTICE: 0100: 74 64 3E 3C 74 64 3E 4E 55 4C 4C 3C 2F 74 64 3E    td><td>NULL</td>
[2018/04/03 13:13:13:1442] NOTICE: 0110: 3C 2F 74 72 3E 3C 74 72 3E 3C 74 64 3E 3C 62 3E    </tr><tr><td><b>
[2018/04/03 13:13:13:1442] NOTICE: 0120: 75 70 6C 6F 61 64 3C 2F 62 3E 3C 2F 74 64 3E 3C    upload</b></td><
[2018/04/03 13:13:13:1442] NOTICE: 0130: 74 64 3E 30 3C 2F 74 64 3E 3C 74 64 3E 4E 55 4C    td>0</td><td>NUL
[2018/04/03 13:13:13:1442] NOTICE: 0140: 4C 3C 2F 74 64 3E 3C 2F 74 72 3E 3C 2F 74 61 62    L</td></tr></tab
[2018/04/03 13:13:13:1442] NOTICE: 0150: 6C 65 3E 3C 62 72 3E 3C 62 3E 66 69 6C 65 6E 61    le><br><b>filena
[2018/04/03 13:13:13:1442] NOTICE: 0160: 6D 65 3A 3C 2F 62 3E 20 6D 79 66 69 6C 65 2E 74    me:</b> myfile.t
[2018/04/03 13:13:13:1442] NOTICE: 0170: 78 74 2C 20 3C 62 3E 6C 65 6E 67 74 68 3C 2F 62    xt, <b>length</b
[2018/04/03 13:13:13:1442] NOTICE: 0180: 3E 20 34 34 3C 2F 62 6F 64 79 3E 3C 2F 68 74 6D    > 44</body></htm
[2018/04/03 13:13:13:1442] NOTICE: 0190: 6C 3E                                              l>              
[2018/04/03 13:13:13:1442] NOTICE: 
[2018/04/03 13:13:13:1442] USER: LWS_CALLBACK_COMPLETED_CLIENT_HTTP
[2018/04/03 13:13:13:1455] USER: Completed
```

