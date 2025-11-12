# Notes on http parser corner cases

## Dealing with %00

%00 is considered illegal in

 - the path part of the URL.  A lot of user code handles it as a NUL terminated string,
   even though the header get apis are based around length.  So it is disallowed to
   avoid ambiguity.

 - the name part of a urlarg, like ?name=value

%00 is valid in

 - the value part of a urlarg, like ?name=value

When the parser sees %00 where it is not allowed, it simply drops the connection.

## Note on proper urlarg handling

urlargs are allowed to contain non-NUL terminated binary.  So it is important to
use the length-based urlarg apis

 - `lws_hdr_copy_fragment()`
 - `lws_get_urlarg_by_name_safe()`

The non-length based urlarg api

 - `lws_get_urlarg_by_name()`

...is soft-deprecated, it's still allowed but it will be fooled by the first %00
seen in the argument into truncating the argument.  Use `lws_get_urlarg_by_name_safe()`
instead.
