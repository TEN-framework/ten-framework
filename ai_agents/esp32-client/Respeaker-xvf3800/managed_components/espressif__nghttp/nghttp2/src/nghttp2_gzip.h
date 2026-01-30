/*
 * nghttp2 - HTTP/2 C Library
 *
 * Copyright (c) 2012 Tatsuhiro Tsujikawa
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#ifndef NGHTTP2_GZIP_H

#  ifdef HAVE_CONFIG_H
#    include <config.h>
#  endif /* HAVE_CONFIG_H */
#  include <zlib.h>

#  include <nghttp2/nghttp2.h>

#  ifdef __cplusplus
extern "C" {
#  endif

/**
 * @struct
 *
 * The gzip stream to inflate data.
 */
typedef struct {
  z_stream zst;
  int8_t finished;
} nghttp2_gzip;

/**
 * @function
 *
 * A helper function to set up a per request gzip stream to inflate
 * data.
 *
 * This function returns 0 if it succeeds, or -1.
 */
int nghttp2_gzip_inflate_new(nghttp2_gzip **inflater_ptr);

/**
 * @function
 *
 * Frees the inflate stream.  The |inflater| may be ``NULL``.
 */
void nghttp2_gzip_inflate_del(nghttp2_gzip *inflater);

/**
 * @function
 *
 * Inflates data in |in| with the length |*inlen_ptr| and stores the
 * inflated data to |out| which has allocated size at least
 * |*outlen_ptr|.  On return, |*outlen_ptr| is updated to represent
 * the number of data written in |out|.  Similarly, |*inlen_ptr| is
 * updated to represent the number of input bytes processed.
 *
 * This function returns 0 if it succeeds, or -1.
 *
 * The example follows::
 *
 *     void on_data_chunk_recv_callback(nghttp2_session *session,
 *                                      uint8_t flags,
 *                                      int32_t stream_id,
 *                                      const uint8_t *data, size_t len,
 *                                      void *user_data)
 *     {
 *         ...
 *         req = nghttp2_session_get_stream_user_data(session, stream_id);
 *         nghttp2_gzip *inflater = req->inflater;
 *         while(len > 0) {
 *             uint8_t out[MAX_OUTLEN];
 *             size_t outlen = MAX_OUTLEN;
 *             size_t tlen = len;
 *             int rv;
 *             rv = nghttp2_gzip_inflate(inflater, out, &outlen, data, &tlen);
 *             if(rv != 0) {
 *                 nghttp2_submit_rst_stream(session, stream_id,
 *                                           NGHTTP2_INTERNAL_ERROR);
 *                 break;
 *             }
 *             ... Do stuff ...
 *             data += tlen;
 *             len -= tlen;
 *         }
 *         ....
 *     }
 */
int nghttp2_gzip_inflate(nghttp2_gzip *inflater, uint8_t *out,
                         size_t *outlen_ptr, const uint8_t *in,
                         size_t *inlen_ptr);

/**
 * @function
 *
 * Returns nonzero if |inflater| sees the end of deflate stream.
 * After this function returns nonzero, `nghttp2_gzip_inflate()` with
 * |inflater| gets to return error.
 */
int nghttp2_gzip_inflate_finished(nghttp2_gzip *inflater);

#  ifdef __cplusplus
}
#  endif

#endif /* NGHTTP2_GZIP_H */
