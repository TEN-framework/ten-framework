# lws minimal secure streams sigv4

The application put a test file to AWS S3, using sigv4 auth.

It does it using Secure Streams... the streamtype is "s3PutObj", along with main
are in ss-s3-main.c

The handler for state changes and payloads for "s3PutObj" is in ss-s3-ss.c


## metadata
 "aws_region" and "aws_service" are configured through metadata. Also, at least
 "x-amz-content-sha256:" and ""x-amz-date:" headers need to be in metadata.


## credentials
credentials are read from ~/.aws/credentials, make sure you have valid keyid and
key.  One need to call lws_ss_sigv4_set_aws_key() to plug in aws credentials into
Secure Streams and the index need to be match of the "blob_index" in entry of "auth"
the policy.  In addition, you need to change the S3 bucket name to your own, as
bucket name is unique globally in S3.


## build

```
 $ cmake . && make
```

## usage


```
[2020/12/19 15:25:06:9763] U: LWS minimal secure streams sigv4
[2020/12/19 15:25:07:0768] U: ss_s3_state: LWSSSCS_CREATING, ord 0x0
[2020/12/19 15:25:07:0769] U: ss_s3_state: LWSSSCS_POLL, ord 0x0
[2020/12/19 15:25:07:0770] U: ss_s3_state: LWSSSCS_CONNECTING, ord 0x0
[2020/12/19 15:25:07:2317] U: SS / TX Payload
[2020/12/19 15:25:07:2317] U: SS / TX Payload Total = 1024, Pos = 0
[2020/12/19 15:25:07:3267] U: ss_s3_state: LWSSSCS_CONNECTED, ord 0x0
[2020/12/19 15:25:07:3267] U: ss_s3_state: LWSSSCS_QOS_ACK_REMOTE, ord 0x0
[2020/12/19 15:25:07:3267] U: ss_s3_state: LWSSSCS_DISCONNECTED, ord 0x0
[2020/12/19 15:25:07:3268] U: ss_s3_state: LWSSSCS_DESTROYING, ord 0x0
[2020/12/19 15:25:07:3269] U: Completed: OK

```
