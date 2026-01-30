package nghttp2

import (
	"bytes"
	"crypto/tls"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"net"
	"net/http"
	"regexp"
	"strings"
	"syscall"
	"testing"
	"time"

	"golang.org/x/net/http2"
	"golang.org/x/net/http2/hpack"
)

// TestH2H1PlainGET tests whether simple HTTP/2 GET request works.
func TestH2H1PlainGET(t *testing.T) {
	st := newServerTester(t, options{})
	defer st.Close()

	res, err := st.http2(requestParam{
		name: "TestH2H1PlainGET",
	})
	if err != nil {
		t.Fatalf("Error st.http2() = %v", err)
	}

	if got, want := res.status, http.StatusOK; got != want {
		t.Errorf("status = %v; want %v", got, want)
	}
}

// TestH2H1AddXfp tests that server appends :scheme to the existing
// x-forwarded-proto header field.
func TestH2H1AddXfp(t *testing.T) {
	opts := options{
		args: []string{"--no-strip-incoming-x-forwarded-proto"},
		handler: func(_ http.ResponseWriter, r *http.Request) {
			xfp := r.Header.Get("X-Forwarded-Proto")
			if got, want := xfp, "foo, http"; got != want {
				t.Errorf("X-Forwarded-Proto = %q; want %q", got, want)
			}
		},
	}

	st := newServerTester(t, opts)
	defer st.Close()

	res, err := st.http2(requestParam{
		name: "TestH2H1AddXfp",
		header: []hpack.HeaderField{
			pair("x-forwarded-proto", "foo"),
		},
	})
	if err != nil {
		t.Fatalf("Error st.http2() = %v", err)
	}

	if got, want := res.status, http.StatusOK; got != want {
		t.Errorf("status = %v; want %v", got, want)
	}
}

// TestH2H1NoAddXfp tests that server does not append :scheme to the
// existing x-forwarded-proto header field.
func TestH2H1NoAddXfp(t *testing.T) {
	opts := options{
		args: []string{
			"--no-add-x-forwarded-proto",
			"--no-strip-incoming-x-forwarded-proto",
		},
		handler: func(_ http.ResponseWriter, r *http.Request) {
			xfp := r.Header.Get("X-Forwarded-Proto")
			if got, want := xfp, "foo"; got != want {
				t.Errorf("X-Forwarded-Proto = %q; want %q", got, want)
			}
		},
	}

	st := newServerTester(t, opts)
	defer st.Close()

	res, err := st.http2(requestParam{
		name: "TestH2H1NoAddXfp",
		header: []hpack.HeaderField{
			pair("x-forwarded-proto", "foo"),
		},
	})
	if err != nil {
		t.Fatalf("Error st.http2() = %v", err)
	}

	if got, want := res.status, http.StatusOK; got != want {
		t.Errorf("status = %v; want %v", got, want)
	}
}

// TestH2H1StripXfp tests that server strips incoming
// x-forwarded-proto header field.
func TestH2H1StripXfp(t *testing.T) {
	opts := options{
		handler: func(_ http.ResponseWriter, r *http.Request) {
			xfp := r.Header.Get("X-Forwarded-Proto")
			if got, want := xfp, "http"; got != want {
				t.Errorf("X-Forwarded-Proto = %q; want %q", got, want)
			}
		},
	}

	st := newServerTester(t, opts)
	defer st.Close()

	res, err := st.http2(requestParam{
		name: "TestH2H1StripXfp",
		header: []hpack.HeaderField{
			pair("x-forwarded-proto", "foo"),
		},
	})
	if err != nil {
		t.Fatalf("Error st.http2() = %v", err)
	}

	if got, want := res.status, http.StatusOK; got != want {
		t.Errorf("status = %v; want %v", got, want)
	}
}

// TestH2H1StripNoAddXfp tests that server strips incoming
// x-forwarded-proto header field, and does not add another.
func TestH2H1StripNoAddXfp(t *testing.T) {
	opts := options{
		args: []string{"--no-add-x-forwarded-proto"},
		handler: func(_ http.ResponseWriter, r *http.Request) {
			if got, found := r.Header["X-Forwarded-Proto"]; found {
				t.Errorf("X-Forwarded-Proto = %q; want nothing", got)
			}
		},
	}

	st := newServerTester(t, opts)
	defer st.Close()

	res, err := st.http2(requestParam{
		name: "TestH2H1StripNoAddXfp",
		header: []hpack.HeaderField{
			pair("x-forwarded-proto", "foo"),
		},
	})
	if err != nil {
		t.Fatalf("Error st.http2() = %v", err)
	}

	if got, want := res.status, http.StatusOK; got != want {
		t.Errorf("status = %v; want %v", got, want)
	}
}

// TestH2H1AddXff tests that server generates X-Forwarded-For header
// field when forwarding request to backend.
func TestH2H1AddXff(t *testing.T) {
	opts := options{
		args: []string{"--add-x-forwarded-for"},
		handler: func(_ http.ResponseWriter, r *http.Request) {
			xff := r.Header.Get("X-Forwarded-For")
			want := "127.0.0.1"
			if xff != want {
				t.Errorf("X-Forwarded-For = %v; want %v", xff, want)
			}
		},
	}

	st := newServerTester(t, opts)
	defer st.Close()

	res, err := st.http2(requestParam{
		name: "TestH2H1AddXff",
	})
	if err != nil {
		t.Fatalf("Error st.http2() = %v", err)
	}

	if got, want := res.status, http.StatusOK; got != want {
		t.Errorf("status = %v; want %v", got, want)
	}
}

// TestH2H1AddXff2 tests that server appends X-Forwarded-For header
// field to existing one when forwarding request to backend.
func TestH2H1AddXff2(t *testing.T) {
	opts := options{
		args: []string{"--add-x-forwarded-for"},
		handler: func(_ http.ResponseWriter, r *http.Request) {
			xff := r.Header.Get("X-Forwarded-For")
			want := "host, 127.0.0.1"
			if xff != want {
				t.Errorf("X-Forwarded-For = %v; want %v", xff, want)
			}
		},
	}

	st := newServerTester(t, opts)
	defer st.Close()

	res, err := st.http2(requestParam{
		name: "TestH2H1AddXff2",
		header: []hpack.HeaderField{
			pair("x-forwarded-for", "host"),
		},
	})
	if err != nil {
		t.Fatalf("Error st.http2() = %v", err)
	}

	if got, want := res.status, http.StatusOK; got != want {
		t.Errorf("status = %v; want %v", got, want)
	}
}

// TestH2H1StripXff tests that --strip-incoming-x-forwarded-for
// option.
func TestH2H1StripXff(t *testing.T) {
	opts := options{
		args: []string{"--strip-incoming-x-forwarded-for"},
		handler: func(_ http.ResponseWriter, r *http.Request) {
			if xff, found := r.Header["X-Forwarded-For"]; found {
				t.Errorf("X-Forwarded-For = %v; want nothing", xff)
			}
		},
	}

	st := newServerTester(t, opts)
	defer st.Close()

	res, err := st.http2(requestParam{
		name: "TestH2H1StripXff",
		header: []hpack.HeaderField{
			pair("x-forwarded-for", "host"),
		},
	})
	if err != nil {
		t.Fatalf("Error st.http2() = %v", err)
	}

	if got, want := res.status, http.StatusOK; got != want {
		t.Errorf("status = %v; want %v", got, want)
	}
}

// TestH2H1StripAddXff tests that --strip-incoming-x-forwarded-for and
// --add-x-forwarded-for options.
func TestH2H1StripAddXff(t *testing.T) {
	opts := options{
		args: []string{
			"--strip-incoming-x-forwarded-for",
			"--add-x-forwarded-for",
		},
		handler: func(_ http.ResponseWriter, r *http.Request) {
			xff := r.Header.Get("X-Forwarded-For")
			want := "127.0.0.1"
			if xff != want {
				t.Errorf("X-Forwarded-For = %v; want %v", xff, want)
			}
		},
	}

	st := newServerTester(t, opts)
	defer st.Close()

	res, err := st.http2(requestParam{
		name: "TestH2H1StripAddXff",
		header: []hpack.HeaderField{
			pair("x-forwarded-for", "host"),
		},
	})
	if err != nil {
		t.Fatalf("Error st.http2() = %v", err)
	}

	if got, want := res.status, http.StatusOK; got != want {
		t.Errorf("status = %v; want %v", got, want)
	}
}

// TestH2H1AddForwardedObfuscated tests that server generates
// Forwarded header field with obfuscated "by" and "for" parameters.
func TestH2H1AddForwardedObfuscated(t *testing.T) {
	opts := options{
		args: []string{"--add-forwarded=by,for,host,proto"},
		handler: func(_ http.ResponseWriter, r *http.Request) {
			pattern := fmt.Sprintf(`by=_[^;]+;for=_[^;]+;host="127\.0\.0\.1:%v";proto=http`, serverPort)
			validFwd := regexp.MustCompile(pattern)
			got := r.Header.Get("Forwarded")

			if !validFwd.MatchString(got) {
				t.Errorf("Forwarded = %v; want pattern %v", got, pattern)
			}
		},
	}

	st := newServerTester(t, opts)
	defer st.Close()

	res, err := st.http2(requestParam{
		name: "TestH2H1AddForwardedObfuscated",
	})
	if err != nil {
		t.Fatalf("Error st.http2() = %v", err)
	}

	if got, want := res.status, http.StatusOK; got != want {
		t.Errorf("status: %v; want %v", got, want)
	}
}

// TestH2H1AddForwardedByIP tests that server generates Forwarded header
// field with IP address in "by" parameter.
func TestH2H1AddForwardedByIP(t *testing.T) {
	opts := options{
		args: []string{"--add-forwarded=by,for", "--forwarded-by=ip"},
		handler: func(_ http.ResponseWriter, r *http.Request) {
			pattern := fmt.Sprintf(`by="127\.0\.0\.1:%v";for=_[^;]+`, serverPort)
			validFwd := regexp.MustCompile(pattern)
			if got := r.Header.Get("Forwarded"); !validFwd.MatchString(got) {
				t.Errorf("Forwarded = %v; want pattern %v", got, pattern)
			}
		},
	}

	st := newServerTester(t, opts)
	defer st.Close()

	res, err := st.http2(requestParam{
		name: "TestH2H1AddForwardedByIP",
	})
	if err != nil {
		t.Fatalf("Error st.http2() = %v", err)
	}

	if got, want := res.status, http.StatusOK; got != want {
		t.Errorf("status: %v; want %v", got, want)
	}
}

// TestH2H1AddForwardedForIP tests that server generates Forwarded header
// field with IP address in "for" parameters.
func TestH2H1AddForwardedForIP(t *testing.T) {
	opts := options{
		args: []string{
			"--add-forwarded=by,for,host,proto",
			"--forwarded-by=_alpha",
			"--forwarded-for=ip",
		},
		handler: func(_ http.ResponseWriter, r *http.Request) {
			want := fmt.Sprintf(`by=_alpha;for=127.0.0.1;host="127.0.0.1:%v";proto=http`, serverPort)
			if got := r.Header.Get("Forwarded"); got != want {
				t.Errorf("Forwarded = %v; want %v", got, want)
			}
		},
	}

	st := newServerTester(t, opts)
	defer st.Close()

	res, err := st.http2(requestParam{
		name: "TestH2H1AddForwardedForIP",
	})
	if err != nil {
		t.Fatalf("Error st.http2() = %v", err)
	}

	if got, want := res.status, http.StatusOK; got != want {
		t.Errorf("status: %v; want %v", got, want)
	}
}

// TestH2H1AddForwardedMerge tests that server generates Forwarded
// header field with IP address in "by" and "for" parameters.  The
// generated values must be appended to the existing value.
func TestH2H1AddForwardedMerge(t *testing.T) {
	opts := options{
		args: []string{"--add-forwarded=proto"},
		handler: func(_ http.ResponseWriter, r *http.Request) {
			if got, want := r.Header.Get("Forwarded"), `host=foo, proto=http`; got != want {
				t.Errorf("Forwarded = %v; want %v", got, want)
			}
		},
	}

	st := newServerTester(t, opts)
	defer st.Close()

	res, err := st.http2(requestParam{
		name: "TestH2H1AddForwardedMerge",
		header: []hpack.HeaderField{
			pair("forwarded", "host=foo"),
		},
	})
	if err != nil {
		t.Fatalf("Error st.http2() = %v", err)
	}

	if got, want := res.status, http.StatusOK; got != want {
		t.Errorf("status: %v; want %v", got, want)
	}
}

// TestH2H1AddForwardedStrip tests that server generates Forwarded
// header field with IP address in "by" and "for" parameters.  The
// generated values must not include the existing value.
func TestH2H1AddForwardedStrip(t *testing.T) {
	opts := options{
		args: []string{
			"--strip-incoming-forwarded",
			"--add-forwarded=proto",
		},
		handler: func(_ http.ResponseWriter, r *http.Request) {
			if got, want := r.Header.Get("Forwarded"), `proto=http`; got != want {
				t.Errorf("Forwarded = %v; want %v", got, want)
			}
		},
	}

	st := newServerTester(t, opts)
	defer st.Close()

	res, err := st.http2(requestParam{
		name: "TestH2H1AddForwardedStrip",
		header: []hpack.HeaderField{
			pair("forwarded", "host=foo"),
		},
	})
	if err != nil {
		t.Fatalf("Error st.http2() = %v", err)
	}

	if got, want := res.status, http.StatusOK; got != want {
		t.Errorf("status: %v; want %v", got, want)
	}
}

// TestH2H1StripForwarded tests that server strips incoming Forwarded
// header field.
func TestH2H1StripForwarded(t *testing.T) {
	opts := options{
		args: []string{"--strip-incoming-forwarded"},
		handler: func(_ http.ResponseWriter, r *http.Request) {
			if got, found := r.Header["Forwarded"]; found {
				t.Errorf("Forwarded = %v; want nothing", got)
			}
		},
	}

	st := newServerTester(t, opts)
	defer st.Close()

	res, err := st.http2(requestParam{
		name: "TestH2H1StripForwarded",
		header: []hpack.HeaderField{
			pair("forwarded", "host=foo"),
		},
	})
	if err != nil {
		t.Fatalf("Error st.http2() = %v", err)
	}

	if got, want := res.status, http.StatusOK; got != want {
		t.Errorf("status: %v; want %v", got, want)
	}
}

// TestH2H1AddForwardedStatic tests that server generates Forwarded
// header field with the given static obfuscated string for "by"
// parameter.
func TestH2H1AddForwardedStatic(t *testing.T) {
	opts := options{
		args: []string{
			"--add-forwarded=by,for",
			"--forwarded-by=_alpha",
		},
		handler: func(_ http.ResponseWriter, r *http.Request) {
			pattern := `by=_alpha;for=_[^;]+`
			validFwd := regexp.MustCompile(pattern)
			if got := r.Header.Get("Forwarded"); !validFwd.MatchString(got) {
				t.Errorf("Forwarded = %v; want pattern %v", got, pattern)
			}
		},
	}

	st := newServerTester(t, opts)
	defer st.Close()

	res, err := st.http2(requestParam{
		name: "TestH2H1AddForwardedStatic",
	})
	if err != nil {
		t.Fatalf("Error st.http2() = %v", err)
	}

	if got, want := res.status, http.StatusOK; got != want {
		t.Errorf("status: %v; want %v", got, want)
	}
}

// TestH2H1GenerateVia tests that server generates Via header field to and
// from backend server.
func TestH2H1GenerateVia(t *testing.T) {
	opts := options{
		handler: func(_ http.ResponseWriter, r *http.Request) {
			if got, want := r.Header.Get("Via"), "2 nghttpx"; got != want {
				t.Errorf("Via: %v; want %v", got, want)
			}
		},
	}

	st := newServerTester(t, opts)
	defer st.Close()

	res, err := st.http2(requestParam{
		name: "TestH2H1GenerateVia",
	})
	if err != nil {
		t.Fatalf("Error st.http2() = %v", err)
	}

	if got, want := res.header.Get("Via"), "1.1 nghttpx"; got != want {
		t.Errorf("Via: %v; want %v", got, want)
	}
}

// TestH2H1AppendVia tests that server adds value to existing Via
// header field to and from backend server.
func TestH2H1AppendVia(t *testing.T) {
	opts := options{
		handler: func(w http.ResponseWriter, r *http.Request) {
			if got, want := r.Header.Get("Via"), "foo, 2 nghttpx"; got != want {
				t.Errorf("Via: %v; want %v", got, want)
			}
			w.Header().Add("Via", "bar")
		},
	}

	st := newServerTester(t, opts)
	defer st.Close()

	res, err := st.http2(requestParam{
		name: "TestH2H1AppendVia",
		header: []hpack.HeaderField{
			pair("via", "foo"),
		},
	})
	if err != nil {
		t.Fatalf("Error st.http2() = %v", err)
	}

	if got, want := res.header.Get("Via"), "bar, 1.1 nghttpx"; got != want {
		t.Errorf("Via: %v; want %v", got, want)
	}
}

// TestH2H1NoVia tests that server does not add value to existing Via
// header field to and from backend server.
func TestH2H1NoVia(t *testing.T) {
	opts := options{
		args: []string{"--no-via"},
		handler: func(w http.ResponseWriter, r *http.Request) {
			if got, want := r.Header.Get("Via"), "foo"; got != want {
				t.Errorf("Via: %v; want %v", got, want)
			}
			w.Header().Add("Via", "bar")
		},
	}

	st := newServerTester(t, opts)
	defer st.Close()

	res, err := st.http2(requestParam{
		name: "TestH2H1NoVia",
		header: []hpack.HeaderField{
			pair("via", "foo"),
		},
	})
	if err != nil {
		t.Fatalf("Error st.http2() = %v", err)
	}

	if got, want := res.header.Get("Via"), "bar"; got != want {
		t.Errorf("Via: %v; want %v", got, want)
	}
}

// TestH2H1HostRewrite tests that server rewrites host header field
func TestH2H1HostRewrite(t *testing.T) {
	opts := options{
		args: []string{"--host-rewrite"},
		handler: func(w http.ResponseWriter, r *http.Request) {
			w.Header().Add("request-host", r.Host)
		},
	}

	st := newServerTester(t, opts)
	defer st.Close()

	res, err := st.http2(requestParam{
		name: "TestH2H1HostRewrite",
	})
	if err != nil {
		t.Fatalf("Error st.http2() = %v", err)
	}

	if got, want := res.status, http.StatusOK; got != want {
		t.Errorf("status: %v; want %v", got, want)
	}

	if got, want := res.header.Get("request-host"), st.backendHost; got != want {
		t.Errorf("request-host: %v; want %v", got, want)
	}
}

// TestH2H1NoHostRewrite tests that server does not rewrite host
// header field
func TestH2H1NoHostRewrite(t *testing.T) {
	opts := options{
		handler: func(w http.ResponseWriter, r *http.Request) {
			w.Header().Add("request-host", r.Host)
		},
	}

	st := newServerTester(t, opts)
	defer st.Close()

	res, err := st.http2(requestParam{
		name: "TestH2H1NoHostRewrite",
	})
	if err != nil {
		t.Fatalf("Error st.http2() = %v", err)
	}

	if got, want := res.status, http.StatusOK; got != want {
		t.Errorf("status: %v; want %v", got, want)
	}

	if got, want := res.header.Get("request-host"), st.frontendHost; got != want {
		t.Errorf("request-host: %v; want %v", got, want)
	}
}

// TestH2H1BadRequestCL tests that server rejects request whose
// content-length header field value does not match its request body
// size.
func TestH2H1BadRequestCL(t *testing.T) {
	st := newServerTester(t, options{})
	defer st.Close()

	// we set content-length: 1024, but the actual request body is
	// 3 bytes.
	res, err := st.http2(requestParam{
		name:   "TestH2H1BadRequestCL",
		method: "POST",
		header: []hpack.HeaderField{
			pair("content-length", "1024"),
		},
		body: []byte("foo"),
	})
	if err != nil {
		t.Fatalf("Error st.http2() = %v", err)
	}

	want := http2.ErrCodeProtocol
	if res.errCode != want {
		t.Errorf("res.errCode = %v; want %v", res.errCode, want)
	}
}

// TestH2H1BadResponseCL tests that server returns error when
// content-length response header field value does not match its
// response body size.
func TestH2H1BadResponseCL(t *testing.T) {
	opts := options{
		handler: func(w http.ResponseWriter, _ *http.Request) {
			// we set content-length: 1024, but only send 3 bytes.
			w.Header().Add("Content-Length", "1024")
			if _, err := w.Write([]byte("foo")); err != nil {
				t.Fatalf("Error w.Write() = %v", err)
			}
		},
	}

	st := newServerTester(t, opts)
	defer st.Close()

	res, err := st.http2(requestParam{
		name: "TestH2H1BadResponseCL",
	})
	if err != nil {
		t.Fatalf("Error st.http2() = %v", err)
	}

	want := http2.ErrCodeInternal
	if res.errCode != want {
		t.Errorf("res.errCode = %v; want %v", res.errCode, want)
	}
}

// TestH2H1LocationRewrite tests location header field rewriting
// works.
func TestH2H1LocationRewrite(t *testing.T) {
	opts := options{
		handler: func(w http.ResponseWriter, _ *http.Request) {
			// TODO we cannot get st.ts's port number
			// here.. 8443 is just a place holder.  We
			// ignore it on rewrite.
			w.Header().Add("Location", "http://127.0.0.1:8443/p/q?a=b#fragment")
		},
	}

	st := newServerTester(t, opts)
	defer st.Close()

	res, err := st.http2(requestParam{
		name: "TestH2H1LocationRewrite",
	})
	if err != nil {
		t.Fatalf("Error st.http2() = %v", err)
	}

	want := fmt.Sprintf("http://127.0.0.1:%v/p/q?a=b#fragment", serverPort)
	if got := res.header.Get("Location"); got != want {
		t.Errorf("Location: %v; want %v", got, want)
	}
}

// TestH2H1ChunkedRequestBody tests that chunked request body works.
func TestH2H1ChunkedRequestBody(t *testing.T) {
	opts := options{
		handler: func(_ http.ResponseWriter, r *http.Request) {
			want := "[chunked]"
			if got := fmt.Sprint(r.TransferEncoding); got != want {
				t.Errorf("Transfer-Encoding: %v; want %v", got, want)
			}
			body, err := io.ReadAll(r.Body)
			if err != nil {
				t.Fatalf("Error reading r.body: %v", err)
			}
			want = "foo"
			if got := string(body); got != want {
				t.Errorf("body: %v; want %v", got, want)
			}
		},
	}

	st := newServerTester(t, opts)
	defer st.Close()

	res, err := st.http2(requestParam{
		name:   "TestH2H1ChunkedRequestBody",
		method: "POST",
		body:   []byte("foo"),
	})
	if err != nil {
		t.Fatalf("Error st.http2() = %v", err)
	}

	if got, want := res.status, http.StatusOK; got != want {
		t.Errorf("status = %v; want %v", got, want)
	}
}

// TestH2H1MultipleRequestCL tests that server rejects request with
// multiple Content-Length request header fields.
func TestH2H1MultipleRequestCL(t *testing.T) {
	opts := options{
		handler: func(http.ResponseWriter, *http.Request) {
			t.Errorf("server should not forward bad request")
		},
	}

	st := newServerTester(t, opts)
	defer st.Close()

	res, err := st.http2(requestParam{
		name: "TestH2H1MultipleRequestCL",
		header: []hpack.HeaderField{
			pair("content-length", "1"),
			pair("content-length", "1"),
		},
	})
	if err != nil {
		t.Fatalf("Error st.http2() = %v", err)
	}

	if got, want := res.errCode, http2.ErrCodeProtocol; got != want {
		t.Errorf("res.errCode: %v; want %v", got, want)
	}
}

// TestH2H1InvalidRequestCL tests that server rejects request with
// Content-Length which cannot be parsed as a number.
func TestH2H1InvalidRequestCL(t *testing.T) {
	opts := options{
		handler: func(http.ResponseWriter, *http.Request) {
			t.Errorf("server should not forward bad request")
		},
	}

	st := newServerTester(t, opts)
	defer st.Close()

	res, err := st.http2(requestParam{
		name: "TestH2H1InvalidRequestCL",
		header: []hpack.HeaderField{
			pair("content-length", ""),
		},
	})
	if err != nil {
		t.Fatalf("Error st.http2() = %v", err)
	}

	if got, want := res.errCode, http2.ErrCodeProtocol; got != want {
		t.Errorf("res.errCode: %v; want %v", got, want)
	}
}

// // TestH2H1ConnectFailure tests that server handles the situation that
// // connection attempt to HTTP/1 backend failed.
// func TestH2H1ConnectFailure(t *testing.T) {
// 	st := newServerTester(t, options{})
// 	defer st.Close()

// 	// shutdown backend server to simulate backend connect failure
// 	st.ts.Close()

// 	res, err := st.http2(requestParam{
// 		name: "TestH2H1ConnectFailure",
// 	})
// 	if err != nil {
// 		t.Fatalf("Error st.http2() = %v", err)
// 	}
// 	want := 503
// 	if got := res.status; got != want {
// 		t.Errorf("status: %v; want %v", got, want)
// 	}
// }

// TestH2H1InvalidMethod tests that server rejects invalid method with
// 501.
func TestH2H1InvalidMethod(t *testing.T) {
	opts := options{
		handler: func(http.ResponseWriter, *http.Request) {
			t.Errorf("server should not forward this request")
		},
	}

	st := newServerTester(t, opts)
	defer st.Close()

	res, err := st.http2(requestParam{
		name:   "TestH2H1InvalidMethod",
		method: "get",
	})
	if err != nil {
		t.Fatalf("Error st.http2() = %v", err)
	}

	if got, want := res.status, http.StatusNotImplemented; got != want {
		t.Errorf("status: %v; want %v", got, want)
	}
}

// TestH2H1BadAuthority tests that server rejects request including
// bad characters in :authority header field.
func TestH2H1BadAuthority(t *testing.T) {
	opts := options{
		handler: func(http.ResponseWriter, *http.Request) {
			t.Errorf("server should not forward this request")
		},
	}

	st := newServerTester(t, opts)
	defer st.Close()

	res, err := st.http2(requestParam{
		name:      "TestH2H1BadAuthority",
		authority: `foo\bar`,
	})
	if err != nil {
		t.Fatalf("Error st.http2() = %v", err)
	}

	if got, want := res.errCode, http2.ErrCodeProtocol; got != want {
		t.Errorf("res.errCode: %v; want %v", got, want)
	}
}

// TestH2H1BadScheme tests that server rejects request including
// bad characters in :scheme header field.
func TestH2H1BadScheme(t *testing.T) {
	opts := options{
		handler: func(http.ResponseWriter, *http.Request) {
			t.Errorf("server should not forward this request")
		},
	}

	st := newServerTester(t, opts)
	defer st.Close()

	res, err := st.http2(requestParam{
		name:   "TestH2H1BadScheme",
		scheme: "http*",
	})
	if err != nil {
		t.Fatalf("Error st.http2() = %v", err)
	}

	if got, want := res.errCode, http2.ErrCodeProtocol; got != want {
		t.Errorf("res.errCode: %v; want %v", got, want)
	}
}

// TestH2H1AssembleCookies tests that crumbled cookies in HTTP/2
// request is assembled into 1 when forwarding to HTTP/1 backend link.
func TestH2H1AssembleCookies(t *testing.T) {
	opts := options{
		handler: func(_ http.ResponseWriter, r *http.Request) {
			if got, want := r.Header.Get("Cookie"), "alpha; bravo; charlie"; got != want {
				t.Errorf("Cookie: %v; want %v", got, want)
			}
		},
	}

	st := newServerTester(t, opts)
	defer st.Close()

	res, err := st.http2(requestParam{
		name: "TestH2H1AssembleCookies",
		header: []hpack.HeaderField{
			pair("cookie", "alpha"),
			pair("cookie", "bravo"),
			pair("cookie", "charlie"),
		},
	})
	if err != nil {
		t.Fatalf("Error st.http2() = %v", err)
	}

	if got, want := res.status, http.StatusOK; got != want {
		t.Errorf("status: %v; want %v", got, want)
	}
}

// TestH2H1TETrailers tests that server accepts TE request header
// field if it has trailers only.
func TestH2H1TETrailers(t *testing.T) {
	st := newServerTester(t, options{})
	defer st.Close()

	res, err := st.http2(requestParam{
		name: "TestH2H1TETrailers",
		header: []hpack.HeaderField{
			pair("te", "trailers"),
		},
	})
	if err != nil {
		t.Fatalf("Error st.http2() = %v", err)
	}

	if got, want := res.status, http.StatusOK; got != want {
		t.Errorf("status: %v; want %v", got, want)
	}
}

// TestH2H1TEGzip tests that server resets stream if TE request header
// field contains gzip.
func TestH2H1TEGzip(t *testing.T) {
	opts := options{
		handler: func(http.ResponseWriter, *http.Request) {
			t.Error("server should not forward bad request")
		},
	}

	st := newServerTester(t, opts)
	defer st.Close()

	res, err := st.http2(requestParam{
		name: "TestH2H1TEGzip",
		header: []hpack.HeaderField{
			pair("te", "gzip"),
		},
	})
	if err != nil {
		t.Fatalf("Error st.http2() = %v", err)
	}

	if got, want := res.errCode, http2.ErrCodeProtocol; got != want {
		t.Errorf("res.errCode = %v; want %v", res.errCode, want)
	}
}

// TestH2H1SNI tests server's TLS SNI extension feature.  It must
// choose appropriate certificate depending on the indicated
// server_name from client.
func TestH2H1SNI(t *testing.T) {
	opts := options{
		args: []string{"--subcert=" + testDir + "/alt-server.key:" + testDir + "/alt-server.crt"},
		tls:  true,
		tlsConfig: &tls.Config{
			ServerName: "alt-domain",
		},
	}

	st := newServerTester(t, opts)
	defer st.Close()

	tlsConn := st.conn.(*tls.Conn)
	connState := tlsConn.ConnectionState()
	cert := connState.PeerCertificates[0]

	if got, want := cert.Subject.CommonName, "alt-domain"; got != want {
		t.Errorf("CommonName: %v; want %v", got, want)
	}
}

// TestH2H1TLSXfp tests nghttpx sends x-forwarded-proto header field
// with http value since :scheme is http, even if the frontend
// connection is encrypted.
func TestH2H1TLSXfp(t *testing.T) {
	opts := options{
		handler: func(_ http.ResponseWriter, r *http.Request) {
			if got, want := r.Header.Get("x-forwarded-proto"), "http"; got != want {
				t.Errorf("x-forwarded-proto: want %v; got %v", want, got)
			}
		},
		tls: true,
	}

	st := newServerTester(t, opts)
	defer st.Close()

	res, err := st.http2(requestParam{
		name: "TestH2H1TLSXfp",
	})
	if err != nil {
		t.Fatalf("Error st.http2() = %v", err)
	}

	if got, want := res.status, http.StatusOK; got != want {
		t.Errorf("res.status: %v; want %v", got, want)
	}
}

// TestH2H1ServerPush tests server push using Link header field from
// backend server.
func TestH2H1ServerPush(t *testing.T) {
	opts := options{
		handler: func(w http.ResponseWriter, r *http.Request) {
			// only resources marked as rel=preload are pushed
			if !strings.HasPrefix(r.URL.Path, "/css/") {
				w.Header().Add("Link", "</css/main.css>; rel=preload, </foo>, </css/theme.css>; rel=preload")
			}
		},
	}

	st := newServerTester(t, opts)
	defer st.Close()

	res, err := st.http2(requestParam{
		name: "TestH2H1ServerPush",
	})
	if err != nil {
		t.Fatalf("Error st.http2() = %v", err)
	}

	if got, want := res.status, http.StatusOK; got != want {
		t.Errorf("res.status: %v; want %v", got, want)
	}

	if got, want := len(res.pushResponse), 2; got != want {
		t.Fatalf("len(res.pushResponse): %v; want %v", got, want)
	}

	mainCSS := res.pushResponse[0]
	if got, want := mainCSS.status, http.StatusOK; got != want {
		t.Errorf("mainCSS.status: %v; want %v", got, want)
	}

	themeCSS := res.pushResponse[1]
	if got, want := themeCSS.status, http.StatusOK; got != want {
		t.Errorf("themeCSS.status: %v; want %v", got, want)
	}
}

// TestH2H1RequestTrailer tests request trailer part is forwarded to
// backend.
func TestH2H1RequestTrailer(t *testing.T) {
	opts := options{
		handler: func(_ http.ResponseWriter, r *http.Request) {
			buf := make([]byte, 4096)
			for {
				_, err := r.Body.Read(buf)
				if err != nil {
					if errors.Is(err, io.EOF) {
						break
					}

					t.Fatalf("r.Body.Read() = %v", err)
				}
			}
			if got, want := r.Trailer.Get("foo"), "bar"; got != want {
				t.Errorf("r.Trailer.Get(foo): %v; want %v", got, want)
			}
		},
	}

	st := newServerTester(t, opts)
	defer st.Close()

	res, err := st.http2(requestParam{
		name: "TestH2H1RequestTrailer",
		body: []byte("1"),
		trailer: []hpack.HeaderField{
			pair("foo", "bar"),
		},
	})
	if err != nil {
		t.Fatalf("Error st.http2() = %v", err)
	}

	if got, want := res.status, http.StatusOK; got != want {
		t.Errorf("res.status: %v; want %v", got, want)
	}
}

// TestH2H1HeaderFieldBuffer tests that request with header fields
// larger than configured buffer size is rejected.
func TestH2H1HeaderFieldBuffer(t *testing.T) {
	opts := options{
		args: []string{"--request-header-field-buffer=10"},
		handler: func(http.ResponseWriter, *http.Request) {
			t.Fatal("execution path should not be here")
		},
	}

	st := newServerTester(t, opts)
	defer st.Close()

	res, err := st.http2(requestParam{
		name: "TestH2H1HeaderFieldBuffer",
	})
	if err != nil {
		t.Fatalf("Error st.http2() = %v", err)
	}

	if got, want := res.status, http.StatusRequestHeaderFieldsTooLarge; got != want {
		t.Errorf("status: %v; want %v", got, want)
	}
}

// TestH2H1HeaderFields tests that request with header fields more
// than configured number is rejected.
func TestH2H1HeaderFields(t *testing.T) {
	opts := options{
		args: []string{"--max-request-header-fields=1"},
		handler: func(http.ResponseWriter, *http.Request) {
			t.Fatal("execution path should not be here")
		},
	}

	st := newServerTester(t, opts)
	defer st.Close()

	res, err := st.http2(requestParam{
		name: "TestH2H1HeaderFields",
		// we have at least 4 pseudo-header fields sent, and
		// that ensures that buffer limit exceeds.
	})
	if err != nil {
		t.Fatalf("Error st.http2() = %v", err)
	}

	if got, want := res.status, http.StatusRequestHeaderFieldsTooLarge; got != want {
		t.Errorf("status: %v; want %v", got, want)
	}
}

// TestH2H1ReqPhaseSetHeader tests mruby request phase hook
// modifies request header fields.
func TestH2H1ReqPhaseSetHeader(t *testing.T) {
	opts := options{
		args: []string{"--mruby-file=" + testDir + "/req-set-header.rb"},
		handler: func(_ http.ResponseWriter, r *http.Request) {
			if got, want := r.Header.Get("User-Agent"), "mruby"; got != want {
				t.Errorf("User-Agent = %v; want %v", got, want)
			}
		},
	}

	st := newServerTester(t, opts)
	defer st.Close()

	res, err := st.http2(requestParam{
		name: "TestH2H1ReqPhaseSetHeader",
	})
	if err != nil {
		t.Fatalf("Error st.http2() = %v", err)
	}

	if got, want := res.status, http.StatusOK; got != want {
		t.Errorf("status = %v; want %v", got, want)
	}
}

// TestH2H1ReqPhaseReturn tests mruby request phase hook returns
// custom response.
func TestH2H1ReqPhaseReturn(t *testing.T) {
	opts := options{
		args: []string{"--mruby-file=" + testDir + "/req-return.rb"},
		handler: func(http.ResponseWriter, *http.Request) {
			t.Fatalf("request should not be forwarded")
		},
	}

	st := newServerTester(t, opts)
	defer st.Close()

	res, err := st.http2(requestParam{
		name: "TestH2H1ReqPhaseReturn",
	})
	if err != nil {
		t.Fatalf("Error st.http2() = %v", err)
	}

	if got, want := res.status, http.StatusNotFound; got != want {
		t.Errorf("status = %v; want %v", got, want)
	}

	hdtests := []struct {
		k, v string
	}{
		{"content-length", "20"},
		{"from", "mruby"},
	}

	for _, tt := range hdtests {
		if got, want := res.header.Get(tt.k), tt.v; got != want {
			t.Errorf("%v = %v; want %v", tt.k, got, want)
		}
	}

	if got, want := string(res.body), "Hello World from req"; got != want {
		t.Errorf("body = %v; want %v", got, want)
	}
}

// TestH2H1RespPhaseSetHeader tests mruby response phase hook modifies
// response header fields.
func TestH2H1RespPhaseSetHeader(t *testing.T) {
	opts := options{
		args: []string{"--mruby-file=" + testDir + "/resp-set-header.rb"},
	}

	st := newServerTester(t, opts)
	defer st.Close()

	res, err := st.http2(requestParam{
		name: "TestH2H1RespPhaseSetHeader",
	})
	if err != nil {
		t.Fatalf("Error st.http2() = %v", err)
	}

	if got, want := res.status, http.StatusOK; got != want {
		t.Errorf("status = %v; want %v", got, want)
	}

	if got, want := res.header.Get("alpha"), "bravo"; got != want {
		t.Errorf("alpha = %v; want %v", got, want)
	}
}

// TestH2H1RespPhaseReturn tests mruby response phase hook returns
// custom response.
func TestH2H1RespPhaseReturn(t *testing.T) {
	opts := options{
		args: []string{"--mruby-file=" + testDir + "/resp-return.rb"},
	}

	st := newServerTester(t, opts)
	defer st.Close()

	res, err := st.http2(requestParam{
		name: "TestH2H1RespPhaseReturn",
	})
	if err != nil {
		t.Fatalf("Error st.http2() = %v", err)
	}

	if got, want := res.status, http.StatusNotFound; got != want {
		t.Errorf("status = %v; want %v", got, want)
	}

	hdtests := []struct {
		k, v string
	}{
		{"content-length", "21"},
		{"from", "mruby"},
	}

	for _, tt := range hdtests {
		if got, want := res.header.Get(tt.k), tt.v; got != want {
			t.Errorf("%v = %v; want %v", tt.k, got, want)
		}
	}

	if got, want := string(res.body), "Hello World from resp"; got != want {
		t.Errorf("body = %v; want %v", got, want)
	}
}

// TestH2H1Upgrade tests HTTP Upgrade to HTTP/2
func TestH2H1Upgrade(t *testing.T) {
	st := newServerTester(t, options{})
	defer st.Close()

	res, err := st.http1(requestParam{
		name: "TestH2H1Upgrade",
		header: []hpack.HeaderField{
			pair("Connection", "Upgrade, HTTP2-Settings"),
			pair("Upgrade", "h2c"),
			pair("HTTP2-Settings", "AAMAAABkAAQAAP__"),
		},
	})

	if err != nil {
		t.Fatalf("Error st.http1() = %v", err)
	}

	if got, want := res.status, http.StatusSwitchingProtocols; got != want {
		t.Errorf("res.status: %v; want %v", got, want)
	}

	res, err = st.http2(requestParam{
		httpUpgrade: true,
	})
	if err != nil {
		t.Fatalf("Error st.http2() = %v", err)
	}

	if got, want := res.status, http.StatusOK; got != want {
		t.Errorf("res.status: %v; want %v", got, want)
	}
}

// TestH2H1ProxyProtocolV1ForwardedForObfuscated tests that Forwarded
// header field includes obfuscated address even if PROXY protocol
// version 1 containing TCP4 entry is accepted.
func TestH2H1ProxyProtocolV1ForwardedForObfuscated(t *testing.T) {
	pattern := `^for=_[^;]+$`
	validFwd := regexp.MustCompile(pattern)
	opts := options{
		args: []string{
			"--accept-proxy-protocol",
			"--add-x-forwarded-for",
			"--add-forwarded=for",
			"--forwarded-for=obfuscated",
		},
		handler: func(_ http.ResponseWriter, r *http.Request) {
			if got := r.Header.Get("Forwarded"); !validFwd.MatchString(got) {
				t.Errorf("Forwarded: %v; want pattern %v", got, pattern)
			}
		},
	}

	st := newServerTester(t, opts)
	defer st.Close()

	if _, err := st.conn.Write([]byte("PROXY TCP4 192.168.0.2 192.168.0.100 12345 8080\r\n")); err != nil {
		t.Fatalf("Error st.conn.Write() = %v", err)
	}

	res, err := st.http2(requestParam{
		name: "TestH2H1ProxyProtocolV1ForwardedForObfuscated",
	})
	if err != nil {
		t.Fatalf("Error st.http2() = %v", err)
	}

	if got, want := res.status, http.StatusOK; got != want {
		t.Errorf("res.status: %v; want %v", got, want)
	}
}

// TestH2H1ProxyProtocolV1TCP4 tests PROXY protocol version 1
// containing TCP4 entry is accepted and X-Forwarded-For contains
// advertised src address.
func TestH2H1ProxyProtocolV1TCP4(t *testing.T) {
	opts := options{
		args: []string{
			"--accept-proxy-protocol",
			"--add-x-forwarded-for",
			"--add-forwarded=for",
			"--forwarded-for=ip",
		},
		handler: func(_ http.ResponseWriter, r *http.Request) {
			if got, want := r.Header.Get("X-Forwarded-For"), "192.168.0.2"; got != want {
				t.Errorf("X-Forwarded-For: %v; want %v", got, want)
			}
			if got, want := r.Header.Get("Forwarded"), "for=192.168.0.2"; got != want {
				t.Errorf("Forwarded: %v; want %v", got, want)
			}
		},
	}

	st := newServerTester(t, opts)
	defer st.Close()

	if _, err := st.conn.Write([]byte("PROXY TCP4 192.168.0.2 192.168.0.100 12345 8080\r\n")); err != nil {
		t.Fatalf("Error st.conn.Write() = %v", err)
	}

	res, err := st.http2(requestParam{
		name: "TestH2H1ProxyProtocolV1TCP4",
	})
	if err != nil {
		t.Fatalf("Error st.http2() = %v", err)
	}

	if got, want := res.status, http.StatusOK; got != want {
		t.Errorf("res.status: %v; want %v", got, want)
	}
}

// TestH2H1ProxyProtocolV1TCP6 tests PROXY protocol version 1
// containing TCP6 entry is accepted and X-Forwarded-For contains
// advertised src address.
func TestH2H1ProxyProtocolV1TCP6(t *testing.T) {
	opts := options{
		args: []string{
			"--accept-proxy-protocol",
			"--add-x-forwarded-for",
			"--add-forwarded=for",
			"--forwarded-for=ip",
		},
		handler: func(_ http.ResponseWriter, r *http.Request) {
			if got, want := r.Header.Get("X-Forwarded-For"), "2001:0db8:85a3:0000:0000:8a2e:0370:7334"; got != want {
				t.Errorf("X-Forwarded-For: %v; want %v", got, want)
			}
			if got, want := r.Header.Get("Forwarded"), `for="[2001:0db8:85a3:0000:0000:8a2e:0370:7334]"`; got != want {
				t.Errorf("Forwarded: %v; want %v", got, want)
			}
		},
	}

	st := newServerTester(t, opts)
	defer st.Close()

	if _, err := st.conn.Write([]byte("PROXY TCP6 2001:0db8:85a3:0000:0000:8a2e:0370:7334 ::1 12345 8080\r\n")); err != nil {
		t.Fatalf("Error st.conn.Write() = %v", err)
	}

	res, err := st.http2(requestParam{
		name: "TestH2H1ProxyProtocolV1TCP6",
	})
	if err != nil {
		t.Fatalf("Error st.http2() = %v", err)
	}

	if got, want := res.status, http.StatusOK; got != want {
		t.Errorf("res.status: %v; want %v", got, want)
	}
}

// TestH2H1ProxyProtocolV1TCP4TLS tests PROXY protocol version 1 over
// TLS containing TCP4 entry is accepted and X-Forwarded-For contains
// advertised src address.
func TestH2H1ProxyProtocolV1TCP4TLS(t *testing.T) {
	opts := options{
		args: []string{
			"--accept-proxy-protocol",
			"--add-x-forwarded-for",
			"--add-forwarded=for",
			"--forwarded-for=ip",
		},
		handler: func(_ http.ResponseWriter, r *http.Request) {
			if got, want := r.Header.Get("X-Forwarded-For"), "192.168.0.2"; got != want {
				t.Errorf("X-Forwarded-For: %v; want %v", got, want)
			}
			if got, want := r.Header.Get("Forwarded"), "for=192.168.0.2"; got != want {
				t.Errorf("Forwarded: %v; want %v", got, want)
			}
		},
		tls:     true,
		tcpData: []byte("PROXY TCP4 192.168.0.2 192.168.0.100 12345 8080\r\n"),
	}

	st := newServerTester(t, opts)
	defer st.Close()

	res, err := st.http2(requestParam{
		name: "TestH2H1ProxyProtocolV1TCP4TLS",
	})
	if err != nil {
		t.Fatalf("Error st.http2() = %v", err)
	}

	if got, want := res.status, http.StatusOK; got != want {
		t.Errorf("res.status: %v; want %v", got, want)
	}
}

// TestH2H1ProxyProtocolV1TCP6TLS tests PROXY protocol version 1 over
// TLS containing TCP6 entry is accepted and X-Forwarded-For contains
// advertised src address.
func TestH2H1ProxyProtocolV1TCP6TLS(t *testing.T) {
	opts := options{
		args: []string{
			"--accept-proxy-protocol",
			"--add-x-forwarded-for",
			"--add-forwarded=for",
			"--forwarded-for=ip",
		},
		handler: func(_ http.ResponseWriter, r *http.Request) {
			if got, want := r.Header.Get("X-Forwarded-For"), "2001:0db8:85a3:0000:0000:8a2e:0370:7334"; got != want {
				t.Errorf("X-Forwarded-For: %v; want %v", got, want)
			}
			if got, want := r.Header.Get("Forwarded"), `for="[2001:0db8:85a3:0000:0000:8a2e:0370:7334]"`; got != want {
				t.Errorf("Forwarded: %v; want %v", got, want)
			}
		},
		tls:     true,
		tcpData: []byte("PROXY TCP6 2001:0db8:85a3:0000:0000:8a2e:0370:7334 ::1 12345 8080\r\n"),
	}

	st := newServerTester(t, opts)
	defer st.Close()

	res, err := st.http2(requestParam{
		name: "TestH2H1ProxyProtocolV1TCP6TLS",
	})
	if err != nil {
		t.Fatalf("Error st.http2() = %v", err)
	}

	if got, want := res.status, http.StatusOK; got != want {
		t.Errorf("res.status: %v; want %v", got, want)
	}
}

// TestH2H1ProxyProtocolV1Unknown tests PROXY protocol version 1
// containing UNKNOWN entry is accepted.
func TestH2H1ProxyProtocolV1Unknown(t *testing.T) {
	opts := options{
		args: []string{
			"--accept-proxy-protocol",
			"--add-x-forwarded-for",
			"--add-forwarded=for",
			"--forwarded-for=ip",
		},
		handler: func(_ http.ResponseWriter, r *http.Request) {
			if got, notWant := r.Header.Get("X-Forwarded-For"), "192.168.0.2"; got == notWant {
				t.Errorf("X-Forwarded-For: %v; want something else", got)
			}
			if got, notWant := r.Header.Get("Forwarded"), "for=192.168.0.2"; got == notWant {
				t.Errorf("Forwarded: %v; want something else", got)
			}
		},
	}

	st := newServerTester(t, opts)
	defer st.Close()

	if _, err := st.conn.Write([]byte("PROXY UNKNOWN 192.168.0.2 192.168.0.100 12345 8080\r\n")); err != nil {
		t.Fatalf("Error st.conn.Write() = %v", err)
	}

	res, err := st.http2(requestParam{
		name: "TestH2H1ProxyProtocolV1Unknown",
	})
	if err != nil {
		t.Fatalf("Error st.http2() = %v", err)
	}

	if got, want := res.status, http.StatusOK; got != want {
		t.Errorf("res.status: %v; want %v", got, want)
	}
}

// TestH2H1ProxyProtocolV1JustUnknown tests PROXY protocol version 1
// containing only "PROXY UNKNOWN" is accepted.
func TestH2H1ProxyProtocolV1JustUnknown(t *testing.T) {
	opts := options{
		args: []string{
			"--accept-proxy-protocol",
			"--add-x-forwarded-for",
		},
	}

	st := newServerTester(t, opts)
	defer st.Close()

	if _, err := st.conn.Write([]byte("PROXY UNKNOWN\r\n")); err != nil {
		t.Fatalf("Error st.conn.Write() = %v", err)
	}

	res, err := st.http2(requestParam{
		name: "TestH2H1ProxyProtocolV1JustUnknown",
	})
	if err != nil {
		t.Fatalf("Error st.http2() = %v", err)
	}

	if got, want := res.status, http.StatusOK; got != want {
		t.Errorf("res.status: %v; want %v", got, want)
	}
}

// TestH2H1ProxyProtocolV1TooLongLine tests PROXY protocol version 1
// line longer than 107 bytes must be rejected
func TestH2H1ProxyProtocolV1TooLongLine(t *testing.T) {
	opts := options{
		args: []string{
			"--accept-proxy-protocol",
			"--add-x-forwarded-for",
		},
	}

	st := newServerTester(t, opts)
	defer st.Close()

	if _, err := st.conn.Write([]byte("PROXY UNKNOWN ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff 65535 655350\r\n")); err != nil {
		t.Fatalf("Error st.conn.Write() = %v", err)
	}

	_, err := st.http2(requestParam{
		name: "TestH2H1ProxyProtocolV1TooLongLine",
	})
	if err == nil {
		t.Fatalf("connection was not terminated")
	}
}

// TestH2H1ProxyProtocolV1BadLineEnd tests that PROXY protocol version
// 1 line ending without \r\n should be rejected.
func TestH2H1ProxyProtocolV1BadLineEnd(t *testing.T) {
	opts := options{
		args: []string{"--accept-proxy-protocol"},
	}

	st := newServerTester(t, opts)
	defer st.Close()

	if _, err := st.conn.Write([]byte("PROXY TCP6 ::1 ::1 12345 8080\r \n")); err != nil {
		t.Fatalf("Error st.conn.Write() = %v", err)
	}

	_, err := st.http2(requestParam{
		name: "TestH2H1ProxyProtocolV1BadLineEnd",
	})
	if err == nil {
		t.Fatalf("connection was not terminated")
	}
}

// TestH2H1ProxyProtocolV1NoEnd tests that PROXY protocol version 1
// line containing no \r\n should be rejected.
func TestH2H1ProxyProtocolV1NoEnd(t *testing.T) {
	opts := options{
		args: []string{"--accept-proxy-protocol"},
	}

	st := newServerTester(t, opts)
	defer st.Close()

	if _, err := st.conn.Write([]byte("PROXY TCP6 ::1 ::1 12345 8080")); err != nil {
		t.Fatalf("Error st.conn.Write() = %v", err)
	}

	_, err := st.http2(requestParam{
		name: "TestH2H1ProxyProtocolV1NoEnd",
	})
	if err == nil {
		t.Fatalf("connection was not terminated")
	}
}

// TestH2H1ProxyProtocolV1EmbeddedNULL tests that PROXY protocol
// version 1 line containing NULL character should be rejected.
func TestH2H1ProxyProtocolV1EmbeddedNULL(t *testing.T) {
	opts := options{
		args: []string{"--accept-proxy-protocol"},
	}

	st := newServerTester(t, opts)
	defer st.Close()

	b := []byte("PROXY TCP6 ::1*foo ::1 12345 8080\r\n")
	b[14] = 0

	if _, err := st.conn.Write(b); err != nil {
		t.Fatalf("Error st.conn.Write() = %v", err)
	}

	_, err := st.http2(requestParam{
		name: "TestH2H1ProxyProtocolV1EmbeddedNULL",
	})
	if err == nil {
		t.Fatalf("connection was not terminated")
	}
}

// TestH2H1ProxyProtocolV1MissingSrcPort tests that PROXY protocol
// version 1 line without src port should be rejected.
func TestH2H1ProxyProtocolV1MissingSrcPort(t *testing.T) {
	opts := options{
		args: []string{"--accept-proxy-protocol"},
	}

	st := newServerTester(t, opts)
	defer st.Close()

	if _, err := st.conn.Write([]byte("PROXY TCP6 ::1 ::1  8080\r\n")); err != nil {
		t.Fatalf("Error st.conn.Write() = %v", err)
	}

	_, err := st.http2(requestParam{
		name: "TestH2H1ProxyProtocolV1MissingSrcPort",
	})
	if err == nil {
		t.Fatalf("connection was not terminated")
	}
}

// TestH2H1ProxyProtocolV1MissingDstPort tests that PROXY protocol
// version 1 line without dst port should be rejected.
func TestH2H1ProxyProtocolV1MissingDstPort(t *testing.T) {
	opts := options{
		args: []string{"--accept-proxy-protocol"},
	}

	st := newServerTester(t, opts)
	defer st.Close()

	if _, err := st.conn.Write([]byte("PROXY TCP6 ::1 ::1 12345 \r\n")); err != nil {
		t.Fatalf("Error st.conn.Write() = %v", err)
	}

	_, err := st.http2(requestParam{
		name: "TestH2H1ProxyProtocolV1MissingDstPort",
	})
	if err == nil {
		t.Fatalf("connection was not terminated")
	}
}

// TestH2H1ProxyProtocolV1InvalidSrcPort tests that PROXY protocol
// containing invalid src port should be rejected.
func TestH2H1ProxyProtocolV1InvalidSrcPort(t *testing.T) {
	opts := options{
		args: []string{"--accept-proxy-protocol"},
	}

	st := newServerTester(t, opts)
	defer st.Close()

	if _, err := st.conn.Write([]byte("PROXY TCP6 ::1 ::1 123x 8080\r\n")); err != nil {
		t.Fatalf("Error st.conn.Write() = %v", err)
	}

	_, err := st.http2(requestParam{
		name: "TestH2H1ProxyProtocolV1InvalidSrcPort",
	})
	if err == nil {
		t.Fatalf("connection was not terminated")
	}
}

// TestH2H1ProxyProtocolV1InvalidDstPort tests that PROXY protocol
// containing invalid dst port should be rejected.
func TestH2H1ProxyProtocolV1InvalidDstPort(t *testing.T) {
	opts := options{
		args: []string{"--accept-proxy-protocol"},
	}

	st := newServerTester(t, opts)
	defer st.Close()

	if _, err := st.conn.Write([]byte("PROXY TCP6 ::1 ::1 123456 80x\r\n")); err != nil {
		t.Fatalf("Error st.conn.Write() = %v", err)
	}

	_, err := st.http2(requestParam{
		name: "TestH2H1ProxyProtocolV1InvalidDstPort",
	})
	if err == nil {
		t.Fatalf("connection was not terminated")
	}
}

// TestH2H1ProxyProtocolV1LeadingZeroPort tests that PROXY protocol
// version 1 line with non zero port with leading zero should be
// rejected.
func TestH2H1ProxyProtocolV1LeadingZeroPort(t *testing.T) {
	opts := options{
		args: []string{"--accept-proxy-protocol"},
	}

	st := newServerTester(t, opts)
	defer st.Close()

	if _, err := st.conn.Write([]byte("PROXY TCP6 ::1 ::1 03000 8080\r\n")); err != nil {
		t.Fatalf("Error st.conn.Write() = %v", err)
	}

	_, err := st.http2(requestParam{
		name: "TestH2H1ProxyProtocolV1LeadingZeroPort",
	})
	if err == nil {
		t.Fatalf("connection was not terminated")
	}
}

// TestH2H1ProxyProtocolV1TooLargeSrcPort tests that PROXY protocol
// containing too large src port should be rejected.
func TestH2H1ProxyProtocolV1TooLargeSrcPort(t *testing.T) {
	opts := options{
		args: []string{"--accept-proxy-protocol"},
	}

	st := newServerTester(t, opts)
	defer st.Close()

	if _, err := st.conn.Write([]byte("PROXY TCP6 ::1 ::1 65536 8080\r\n")); err != nil {
		t.Fatalf("Error st.conn.Write() = %v", err)
	}

	_, err := st.http2(requestParam{
		name: "TestH2H1ProxyProtocolV1TooLargeSrcPort",
	})
	if err == nil {
		t.Fatalf("connection was not terminated")
	}
}

// TestH2H1ProxyProtocolV1TooLargeDstPort tests that PROXY protocol
// containing too large dst port should be rejected.
func TestH2H1ProxyProtocolV1TooLargeDstPort(t *testing.T) {
	opts := options{
		args: []string{"--accept-proxy-protocol"},
	}

	st := newServerTester(t, opts)
	defer st.Close()

	if _, err := st.conn.Write([]byte("PROXY TCP6 ::1 ::1 12345 65536\r\n")); err != nil {
		t.Fatalf("Error st.conn.Write() = %v", err)
	}

	_, err := st.http2(requestParam{
		name: "TestH2H1ProxyProtocolV1TooLargeDstPort",
	})
	if err == nil {
		t.Fatalf("connection was not terminated")
	}
}

// TestH2H1ProxyProtocolV1InvalidSrcAddr tests that PROXY protocol
// containing invalid src addr should be rejected.
func TestH2H1ProxyProtocolV1InvalidSrcAddr(t *testing.T) {
	opts := options{
		args: []string{"--accept-proxy-protocol"},
	}

	st := newServerTester(t, opts)
	defer st.Close()

	if _, err := st.conn.Write([]byte("PROXY TCP6 192.168.0.1 ::1 12345 8080\r\n")); err != nil {
		t.Fatalf("Error st.conn.Write() = %v", err)
	}

	_, err := st.http2(requestParam{
		name: "TestH2H1ProxyProtocolV1InvalidSrcAddr",
	})
	if err == nil {
		t.Fatalf("connection was not terminated")
	}
}

// TestH2H1ProxyProtocolV1InvalidDstAddr tests that PROXY protocol
// containing invalid dst addr should be rejected.
func TestH2H1ProxyProtocolV1InvalidDstAddr(t *testing.T) {
	opts := options{
		args: []string{"--accept-proxy-protocol"},
	}

	st := newServerTester(t, opts)
	defer st.Close()

	if _, err := st.conn.Write([]byte("PROXY TCP6 ::1 192.168.0.1 12345 8080\r\n")); err != nil {
		t.Fatalf("Error st.conn.Write() = %v", err)
	}

	_, err := st.http2(requestParam{
		name: "TestH2H1ProxyProtocolV1InvalidDstAddr",
	})
	if err == nil {
		t.Fatalf("connection was not terminated")
	}
}

// TestH2H1ProxyProtocolV1InvalidProtoFamily tests that PROXY protocol
// containing invalid protocol family should be rejected.
func TestH2H1ProxyProtocolV1InvalidProtoFamily(t *testing.T) {
	opts := options{
		args: []string{"--accept-proxy-protocol"},
	}

	st := newServerTester(t, opts)
	defer st.Close()

	if _, err := st.conn.Write([]byte("PROXY UNIX ::1 ::1 12345 8080\r\n")); err != nil {
		t.Fatalf("Error st.conn.Write() = %v", err)
	}

	_, err := st.http2(requestParam{
		name: "TestH2H1ProxyProtocolV1InvalidProtoFamily",
	})
	if err == nil {
		t.Fatalf("connection was not terminated")
	}
}

// TestH2H1ProxyProtocolV1InvalidID tests that PROXY protocol
// containing invalid PROXY protocol version 1 ID should be rejected.
func TestH2H1ProxyProtocolV1InvalidID(t *testing.T) {
	opts := options{
		args: []string{"--accept-proxy-protocol"},
	}

	st := newServerTester(t, opts)
	defer st.Close()

	if _, err := st.conn.Write([]byte("PR0XY TCP6 ::1 ::1 12345 8080\r\n")); err != nil {
		t.Fatalf("Error st.conn.Write() = %v", err)
	}

	_, err := st.http2(requestParam{
		name: "TestH2H1ProxyProtocolV1InvalidID",
	})
	if err == nil {
		t.Fatalf("connection was not terminated")
	}
}

// TestH2H1ProxyProtocolV2TCP4 tests PROXY protocol version 2
// containing AF_INET family is accepted and X-Forwarded-For contains
// advertised src address.
func TestH2H1ProxyProtocolV2TCP4(t *testing.T) {
	opts := options{
		args: []string{
			"--accept-proxy-protocol",
			"--add-x-forwarded-for",
			"--add-forwarded=for",
			"--forwarded-for=ip",
		},
		handler: func(_ http.ResponseWriter, r *http.Request) {
			if got, want := r.Header.Get("X-Forwarded-For"), "192.168.0.2"; got != want {
				t.Errorf("X-Forwarded-For: %v; want %v", got, want)
			}
			if got, want := r.Header.Get("Forwarded"), "for=192.168.0.2"; got != want {
				t.Errorf("Forwarded: %v; want %v", got, want)
			}
		},
	}

	st := newServerTester(t, opts)
	defer st.Close()

	var b bytes.Buffer
	if err := writeProxyProtocolV2(&b, proxyProtocolV2{
		command: proxyProtocolV2CommandProxy,
		sourceAddress: &net.TCPAddr{
			IP:   net.ParseIP("192.168.0.2").To4(),
			Port: 12345,
		},
		destinationAddress: &net.TCPAddr{
			IP:   net.ParseIP("192.168.0.100").To4(),
			Port: 8080,
		},
		additionalData: []byte("foobar"),
	}); err != nil {
		t.Fatalf("Error writeProxyProtocolV2() = %v", err)
	}

	if _, err := st.conn.Write(b.Bytes()); err != nil {
		t.Fatalf("Error st.conn.Write() = %v", err)
	}

	res, err := st.http2(requestParam{
		name: "TestH2H1ProxyProtocolV2TCP4",
	})
	if err != nil {
		t.Fatalf("Error st.http2() = %v", err)
	}

	if got, want := res.status, http.StatusOK; got != want {
		t.Errorf("res.status: %v; want %v", got, want)
	}
}

// TestH2H1ProxyProtocolV2TCP6 tests PROXY protocol version 2
// containing AF_INET6 family is accepted and X-Forwarded-For contains
// advertised src address.
func TestH2H1ProxyProtocolV2TCP6(t *testing.T) {
	opts := options{
		args: []string{
			"--accept-proxy-protocol",
			"--add-x-forwarded-for",
			"--add-forwarded=for",
			"--forwarded-for=ip",
		},
		handler: func(_ http.ResponseWriter, r *http.Request) {
			if got, want := r.Header.Get("X-Forwarded-For"), "2001:db8:85a3::8a2e:370:7334"; got != want {
				t.Errorf("X-Forwarded-For: %v; want %v", got, want)
			}
			if got, want := r.Header.Get("Forwarded"), `for="[2001:db8:85a3::8a2e:370:7334]"`; got != want {
				t.Errorf("Forwarded: %v; want %v", got, want)
			}
		},
	}

	st := newServerTester(t, opts)
	defer st.Close()

	var b bytes.Buffer
	if err := writeProxyProtocolV2(&b, proxyProtocolV2{
		command: proxyProtocolV2CommandProxy,
		sourceAddress: &net.TCPAddr{
			IP:   net.ParseIP("2001:0db8:85a3:0000:0000:8a2e:0370:7334"),
			Port: 12345,
		},
		destinationAddress: &net.TCPAddr{
			IP:   net.ParseIP("::1"),
			Port: 8080,
		},
		additionalData: []byte("foobar"),
	}); err != nil {
		t.Fatalf("Error writeProxyProtocolV2() = %v", err)
	}

	if _, err := st.conn.Write(b.Bytes()); err != nil {
		t.Fatalf("Error st.conn.Write() = %v", err)
	}

	res, err := st.http2(requestParam{
		name: "TestH2H1ProxyProtocolV2TCP6",
	})
	if err != nil {
		t.Fatalf("Error st.http2() = %v", err)
	}

	if got, want := res.status, http.StatusOK; got != want {
		t.Errorf("res.status: %v; want %v", got, want)
	}
}

// TestH2H1ProxyProtocolV2TCP4TLS tests PROXY protocol version 2 over
// TLS containing AF_INET family is accepted and X-Forwarded-For
// contains advertised src address.
func TestH2H1ProxyProtocolV2TCP4TLS(t *testing.T) {
	var v2Hdr bytes.Buffer
	if err := writeProxyProtocolV2(&v2Hdr, proxyProtocolV2{
		command: proxyProtocolV2CommandProxy,
		sourceAddress: &net.TCPAddr{
			IP:   net.ParseIP("192.168.0.2").To4(),
			Port: 12345,
		},
		destinationAddress: &net.TCPAddr{
			IP:   net.ParseIP("192.168.0.100").To4(),
			Port: 8080,
		},
		additionalData: []byte("foobar"),
	}); err != nil {
		t.Fatalf("Error writeProxyProtocolV2() = %v", err)
	}

	opts := options{
		args: []string{
			"--accept-proxy-protocol",
			"--add-x-forwarded-for",
			"--add-forwarded=for",
			"--forwarded-for=ip",
		},
		handler: func(_ http.ResponseWriter, r *http.Request) {
			if got, want := r.Header.Get("X-Forwarded-For"), "192.168.0.2"; got != want {
				t.Errorf("X-Forwarded-For: %v; want %v", got, want)
			}
			if got, want := r.Header.Get("Forwarded"), "for=192.168.0.2"; got != want {
				t.Errorf("Forwarded: %v; want %v", got, want)
			}
		},
		tls:     true,
		tcpData: v2Hdr.Bytes(),
	}

	st := newServerTester(t, opts)
	defer st.Close()

	res, err := st.http2(requestParam{
		name: "TestH2H1ProxyProtocolV2TCP4TLS",
	})
	if err != nil {
		t.Fatalf("Error st.http2() = %v", err)
	}

	if got, want := res.status, http.StatusOK; got != want {
		t.Errorf("res.status: %v; want %v", got, want)
	}
}

// TestH2H1ProxyProtocolV2TCP6TLS tests PROXY protocol version 2 over
// TLS containing AF_INET6 family is accepted and X-Forwarded-For
// contains advertised src address.
func TestH2H1ProxyProtocolV2TCP6TLS(t *testing.T) {
	var v2Hdr bytes.Buffer
	if err := writeProxyProtocolV2(&v2Hdr, proxyProtocolV2{
		command: proxyProtocolV2CommandProxy,
		sourceAddress: &net.TCPAddr{
			IP:   net.ParseIP("2001:0db8:85a3:0000:0000:8a2e:0370:7334"),
			Port: 12345,
		},
		destinationAddress: &net.TCPAddr{
			IP:   net.ParseIP("::1"),
			Port: 8080,
		},
		additionalData: []byte("foobar"),
	}); err != nil {
		t.Fatalf("Error writeProxyProtocolV2() = %v", err)
	}

	opts := options{
		args: []string{
			"--accept-proxy-protocol",
			"--add-x-forwarded-for",
			"--add-forwarded=for",
			"--forwarded-for=ip",
		},
		handler: func(_ http.ResponseWriter, r *http.Request) {
			if got, want := r.Header.Get("X-Forwarded-For"), "2001:db8:85a3::8a2e:370:7334"; got != want {
				t.Errorf("X-Forwarded-For: %v; want %v", got, want)
			}
			if got, want := r.Header.Get("Forwarded"), `for="[2001:db8:85a3::8a2e:370:7334]"`; got != want {
				t.Errorf("Forwarded: %v; want %v", got, want)
			}
		},
		tls:     true,
		tcpData: v2Hdr.Bytes(),
	}

	st := newServerTester(t, opts)
	defer st.Close()

	res, err := st.http2(requestParam{
		name: "TestH2H1ProxyProtocolV2TCP6TLS",
	})
	if err != nil {
		t.Fatalf("Error st.http2() = %v", err)
	}

	if got, want := res.status, http.StatusOK; got != want {
		t.Errorf("res.status: %v; want %v", got, want)
	}
}

// TestH2H1ProxyProtocolV2Local tests PROXY protocol version 2
// containing cmd == Local is ignored.
func TestH2H1ProxyProtocolV2Local(t *testing.T) {
	opts := options{
		args: []string{
			"--accept-proxy-protocol",
			"--add-x-forwarded-for",
			"--add-forwarded=for",
			"--forwarded-for=ip",
		},
		handler: func(_ http.ResponseWriter, r *http.Request) {
			if got, want := r.Header.Get("X-Forwarded-For"), "127.0.0.1"; got != want {
				t.Errorf("X-Forwarded-For: %v; want %v", got, want)
			}
			if got, want := r.Header.Get("Forwarded"), "for=127.0.0.1"; got != want {
				t.Errorf("Forwarded: %v; want %v", got, want)
			}
		},
	}

	st := newServerTester(t, opts)
	defer st.Close()

	var b bytes.Buffer
	if err := writeProxyProtocolV2(&b, proxyProtocolV2{
		command: proxyProtocolV2CommandLocal,
		sourceAddress: &net.TCPAddr{
			IP:   net.ParseIP("192.168.0.2").To4(),
			Port: 12345,
		},
		destinationAddress: &net.TCPAddr{
			IP:   net.ParseIP("192.168.0.100").To4(),
			Port: 8080,
		},
		additionalData: []byte("foobar"),
	}); err != nil {
		t.Fatalf("Error writeProxyProtocolV2() = %v", err)
	}

	if _, err := st.conn.Write(b.Bytes()); err != nil {
		t.Fatalf("Error st.conn.Write() = %v", err)
	}

	res, err := st.http2(requestParam{
		name: "TestH2H1ProxyProtocolV2Local",
	})
	if err != nil {
		t.Fatalf("Error st.http2() = %v", err)
	}

	if got, want := res.status, http.StatusOK; got != want {
		t.Errorf("res.status: %v; want %v", got, want)
	}
}

// TestH2H1ProxyProtocolV2UnknownCmd tests PROXY protocol version 2
// containing unknown cmd should be rejected.
func TestH2H1ProxyProtocolV2UnknownCmd(t *testing.T) {
	opts := options{
		args: []string{"--accept-proxy-protocol"},
	}

	st := newServerTester(t, opts)
	defer st.Close()

	var b bytes.Buffer
	if err := writeProxyProtocolV2(&b, proxyProtocolV2{
		command: 0xf,
		sourceAddress: &net.TCPAddr{
			IP:   net.ParseIP("192.168.0.2").To4(),
			Port: 12345,
		},
		destinationAddress: &net.TCPAddr{
			IP:   net.ParseIP("192.168.0.100").To4(),
			Port: 8080,
		},
		additionalData: []byte("foobar"),
	}); err != nil {
		t.Fatalf("Error writeProxyProtocolV2() = %v", err)
	}

	if _, err := st.conn.Write(b.Bytes()); err != nil {
		t.Fatalf("Error st.conn.Write() = %v", err)
	}

	_, err := st.http2(requestParam{
		name: "TestH2H1ProxyProtocolV2UnknownCmd",
	})
	if err == nil {
		t.Fatalf("connection was not terminated")
	}
}

// TestH2H1ProxyProtocolV2Unix tests PROXY protocol version 2
// containing AF_UNIX family is ignored.
func TestH2H1ProxyProtocolV2Unix(t *testing.T) {
	opts := options{
		args: []string{
			"--accept-proxy-protocol",
			"--add-x-forwarded-for",
			"--add-forwarded=for",
			"--forwarded-for=ip",
		},
		handler: func(_ http.ResponseWriter, r *http.Request) {
			if got, want := r.Header.Get("X-Forwarded-For"), "127.0.0.1"; got != want {
				t.Errorf("X-Forwarded-For: %v; want %v", got, want)
			}
			if got, want := r.Header.Get("Forwarded"), "for=127.0.0.1"; got != want {
				t.Errorf("Forwarded: %v; want %v", got, want)
			}
		},
	}

	st := newServerTester(t, opts)
	defer st.Close()

	var b bytes.Buffer
	if err := writeProxyProtocolV2(&b, proxyProtocolV2{
		command: proxyProtocolV2CommandProxy,
		sourceAddress: &net.UnixAddr{
			Name: "/foo",
			Net:  "unix",
		},
		destinationAddress: &net.UnixAddr{
			Name: "/bar",
			Net:  "unix",
		},
		additionalData: []byte("foobar"),
	}); err != nil {
		t.Fatalf("Error writeProxyProtocolV2() = %v", err)
	}

	if _, err := st.conn.Write(b.Bytes()); err != nil {
		t.Fatalf("Error st.conn.Write() = %v", err)
	}

	res, err := st.http2(requestParam{
		name: "TestH2H1ProxyProtocolV2Unix",
	})
	if err != nil {
		t.Fatalf("Error st.http2() = %v", err)
	}

	if got, want := res.status, http.StatusOK; got != want {
		t.Errorf("res.status: %v; want %v", got, want)
	}
}

// TestH2H1ProxyProtocolV2Unspec tests PROXY protocol version 2
// containing AF_UNSPEC family is ignored.
func TestH2H1ProxyProtocolV2Unspec(t *testing.T) {
	opts := options{
		args: []string{
			"--accept-proxy-protocol",
			"--add-x-forwarded-for",
			"--add-forwarded=for",
			"--forwarded-for=ip",
		},
		handler: func(_ http.ResponseWriter, r *http.Request) {
			if got, want := r.Header.Get("X-Forwarded-For"), "127.0.0.1"; got != want {
				t.Errorf("X-Forwarded-For: %v; want %v", got, want)
			}
			if got, want := r.Header.Get("Forwarded"), "for=127.0.0.1"; got != want {
				t.Errorf("Forwarded: %v; want %v", got, want)
			}
		},
	}

	st := newServerTester(t, opts)
	defer st.Close()

	var b bytes.Buffer
	if err := writeProxyProtocolV2(&b, proxyProtocolV2{
		command:        proxyProtocolV2CommandProxy,
		additionalData: []byte("foobar"),
	}); err != nil {
		t.Fatalf("Error writeProxyProtocolV2() = %v", err)
	}

	if _, err := st.conn.Write(b.Bytes()); err != nil {
		t.Fatalf("Error st.conn.Write() = %v", err)
	}

	res, err := st.http2(requestParam{
		name: "TestH2H1ProxyProtocolV2Unspec",
	})
	if err != nil {
		t.Fatalf("Error st.http2() = %v", err)
	}

	if got, want := res.status, http.StatusOK; got != want {
		t.Errorf("res.status: %v; want %v", got, want)
	}
}

// TestH2H1ExternalDNS tests that DNS resolution using external DNS
// with HTTP/1 backend works.
func TestH2H1ExternalDNS(t *testing.T) {
	opts := options{
		args: []string{"--external-dns"},
	}

	st := newServerTester(t, opts)
	defer st.Close()

	res, err := st.http2(requestParam{
		name: "TestH2H1ExternalDNS",
	})
	if err != nil {
		t.Fatalf("Error st.http2() = %v", err)
	}

	if got, want := res.status, http.StatusOK; got != want {
		t.Errorf("status = %v; want %v", got, want)
	}
}

// TestH2H1DNS tests that DNS resolution without external DNS with
// HTTP/1 backend works.
func TestH2H1DNS(t *testing.T) {
	opts := options{
		args: []string{"--dns"},
	}

	st := newServerTester(t, opts)
	defer st.Close()

	res, err := st.http2(requestParam{
		name: "TestH2H1DNS",
	})
	if err != nil {
		t.Fatalf("Error st.http2() = %v", err)
	}

	if got, want := res.status, http.StatusOK; got != want {
		t.Errorf("status = %v; want %v", got, want)
	}
}

// TestH2H1HTTPSRedirect tests that the request to the backend which
// requires TLS is redirected to https URI.
func TestH2H1HTTPSRedirect(t *testing.T) {
	opts := options{
		args: []string{"--redirect-if-not-tls"},
	}

	st := newServerTester(t, opts)
	defer st.Close()

	res, err := st.http2(requestParam{
		name: "TestH2H1HTTPSRedirect",
	})
	if err != nil {
		t.Fatalf("Error st.http2() = %v", err)
	}

	if got, want := res.status, http.StatusPermanentRedirect; got != want {
		t.Errorf("status = %v; want %v", got, want)
	}

	if got, want := res.header.Get("location"), "https://127.0.0.1/"; got != want {
		t.Errorf("location: %v; want %v", got, want)
	}
}

// TestH2H1HTTPSRedirectPort tests that the request to the backend
// which requires TLS is redirected to https URI with given port.
func TestH2H1HTTPSRedirectPort(t *testing.T) {
	opts := options{
		args: []string{
			"--redirect-if-not-tls",
			"--redirect-https-port=8443",
		},
	}

	st := newServerTester(t, opts)
	defer st.Close()

	res, err := st.http2(requestParam{
		path: "/foo?bar",
		name: "TestH2H1HTTPSRedirectPort",
	})
	if err != nil {
		t.Fatalf("Error st.http2() = %v", err)
	}

	if got, want := res.status, http.StatusPermanentRedirect; got != want {
		t.Errorf("status = %v; want %v", got, want)
	}

	if got, want := res.header.Get("location"), "https://127.0.0.1:8443/foo?bar"; got != want {
		t.Errorf("location: %v; want %v", got, want)
	}
}

// TestH2H1Code204 tests that 204 response without content-length, and
// transfer-encoding is valid.
func TestH2H1Code204(t *testing.T) {
	opts := options{
		handler: func(w http.ResponseWriter, _ *http.Request) {
			w.WriteHeader(http.StatusNoContent)
		},
	}

	st := newServerTester(t, opts)
	defer st.Close()

	res, err := st.http2(requestParam{
		name: "TestH2H1Code204",
	})
	if err != nil {
		t.Fatalf("Error st.http2() = %v", err)
	}

	if got, want := res.status, http.StatusNoContent; got != want {
		t.Errorf("status = %v; want %v", got, want)
	}
}

// TestH2H1Code204CL0 tests that 204 response with content-length: 0
// is allowed.
func TestH2H1Code204CL0(t *testing.T) {
	opts := options{
		handler: func(w http.ResponseWriter, _ *http.Request) {
			hj, ok := w.(http.Hijacker)
			if !ok {
				http.Error(w, "Could not hijack the connection", http.StatusInternalServerError)
				return
			}
			conn, bufrw, err := hj.Hijack()
			if err != nil {
				http.Error(w, err.Error(), http.StatusInternalServerError)
				return
			}
			defer conn.Close()
			if _, err := bufrw.WriteString("HTTP/1.1 204\r\nContent-Length: 0\r\n\r\n"); err != nil {
				t.Fatalf("Error bufrw.WriteString() = %v", err)
			}
			bufrw.Flush()
		},
	}

	st := newServerTester(t, opts)
	defer st.Close()

	res, err := st.http2(requestParam{
		name: "TestH2H1Code204CL0",
	})
	if err != nil {
		t.Fatalf("Error st.http2() = %v", err)
	}

	if got, want := res.status, http.StatusNoContent; got != want {
		t.Errorf("status = %v; want %v", got, want)
	}

	if got, found := res.header["Content-Length"]; found {
		t.Errorf("Content-Length = %v, want nothing", got)
	}
}

// TestH2H1Code204CLNonzero tests that 204 response with nonzero
// content-length is not allowed.
func TestH2H1Code204CLNonzero(t *testing.T) {
	opts := options{
		handler: func(w http.ResponseWriter, _ *http.Request) {
			hj, ok := w.(http.Hijacker)
			if !ok {
				http.Error(w, "Could not hijack the connection", http.StatusInternalServerError)
				return
			}
			conn, bufrw, err := hj.Hijack()
			if err != nil {
				http.Error(w, err.Error(), http.StatusInternalServerError)
				return
			}
			defer conn.Close()
			if _, err := bufrw.WriteString("HTTP/1.1 204\r\nContent-Length: 1\r\n\r\n"); err != nil {
				t.Fatalf("Error bufrw.WriteString() = %v", err)
			}
			bufrw.Flush()
		},
	}

	st := newServerTester(t, opts)
	defer st.Close()

	res, err := st.http2(requestParam{
		name: "TestH2H1Code204CLNonzero",
	})
	if err != nil {
		t.Fatalf("Error st.http2() = %v", err)
	}

	if got, want := res.status, http.StatusBadGateway; got != want {
		t.Errorf("status = %v; want %v", got, want)
	}
}

// TestH2H1Code204TE tests that 204 response with transfer-encoding is
// not allowed.
func TestH2H1Code204TE(t *testing.T) {
	opts := options{
		handler: func(w http.ResponseWriter, _ *http.Request) {
			hj, ok := w.(http.Hijacker)
			if !ok {
				http.Error(w, "Could not hijack the connection", http.StatusInternalServerError)
				return
			}
			conn, bufrw, err := hj.Hijack()
			if err != nil {
				http.Error(w, err.Error(), http.StatusInternalServerError)
				return
			}
			defer conn.Close()
			if _, err := bufrw.WriteString("HTTP/1.1 204\r\nTransfer-Encoding: chunked\r\n\r\n"); err != nil {
				t.Fatalf("Error bufrw.WriteString() = %v", err)
			}
			bufrw.Flush()
		},
	}

	st := newServerTester(t, opts)
	defer st.Close()

	res, err := st.http2(requestParam{
		name: "TestH2H1Code204TE",
	})
	if err != nil {
		t.Fatalf("Error st.http2() = %v", err)
	}

	if got, want := res.status, http.StatusBadGateway; got != want {
		t.Errorf("status = %v; want %v", got, want)
	}
}

// TestH2H1AffinityCookie tests that affinity cookie is sent back in
// cleartext http.
func TestH2H1AffinityCookie(t *testing.T) {
	opts := options{
		args: []string{"--affinity-cookie"},
	}

	st := newServerTester(t, opts)
	defer st.Close()

	res, err := st.http2(requestParam{
		name: "TestH2H1AffinityCookie",
	})
	if err != nil {
		t.Fatalf("Error st.http2() = %v", err)
	}

	if got, want := res.status, http.StatusOK; got != want {
		t.Errorf("status = %v; want %v", got, want)
	}

	const pattern = `affinity=[0-9a-f]{8}; Path=/foo/bar`
	validCookie := regexp.MustCompile(pattern)

	if got := res.header.Get("Set-Cookie"); !validCookie.MatchString(got) {
		t.Errorf("Set-Cookie: %v; want pattern %v", got, pattern)
	}
}

// TestH2H1AffinityCookieTLS tests that affinity cookie is sent back
// in https.
func TestH2H1AffinityCookieTLS(t *testing.T) {
	opts := options{
		args: []string{"--affinity-cookie"},
		tls:  true,
	}

	st := newServerTester(t, opts)
	defer st.Close()

	res, err := st.http2(requestParam{
		name:   "TestH2H1AffinityCookieTLS",
		scheme: "https",
	})
	if err != nil {
		t.Fatalf("Error st.http2() = %v", err)
	}

	if got, want := res.status, http.StatusOK; got != want {
		t.Errorf("status = %v; want %v", got, want)
	}

	const pattern = `affinity=[0-9a-f]{8}; Path=/foo/bar; Secure`
	validCookie := regexp.MustCompile(pattern)

	if got := res.header.Get("Set-Cookie"); !validCookie.MatchString(got) {
		t.Errorf("Set-Cookie: %v; want pattern %v", got, pattern)
	}
}

// TestH2H1GracefulShutdown tests graceful shutdown.
func TestH2H1GracefulShutdown(t *testing.T) {
	st := newServerTester(t, options{})
	defer st.Close()

	fmt.Fprint(st.conn, "PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n")

	if err := st.fr.WriteSettings(); err != nil {
		t.Fatalf("st.fr.WriteSettings(): %v", err)
	}

	header := []hpack.HeaderField{
		pair(":method", "GET"),
		pair(":scheme", "http"),
		pair(":authority", st.authority),
		pair(":path", "/"),
	}

	for _, h := range header {
		_ = st.enc.WriteField(h)
	}

	if err := st.fr.WriteHeaders(http2.HeadersFrameParam{
		StreamID:      1,
		EndStream:     false,
		EndHeaders:    true,
		BlockFragment: st.headerBlkBuf.Bytes(),
	}); err != nil {
		t.Fatalf("st.fr.WriteHeaders(): %v", err)
	}

	// send SIGQUIT signal to nghttpx to perform graceful shutdown
	if err := st.cmd.Process.Signal(syscall.SIGQUIT); err != nil {
		t.Fatalf("Error st.cmd.Process.Signal() = %v", err)
	}

	time.Sleep(150 * time.Millisecond)

	// after signal, finish request body
	if err := st.fr.WriteData(1, true, nil); err != nil {
		t.Fatalf("st.fr.WriteData(): %v", err)
	}

	numGoAway := 0

	for {
		fr, err := st.readFrame()
		if err != nil {
			if errors.Is(err, io.EOF) {
				want := 2
				if got := numGoAway; got != want {
					t.Fatalf("numGoAway: %v; want %v", got, want)
				}

				return
			}

			t.Fatalf("st.readFrame(): %v", err)
		}

		switch f := fr.(type) {
		case *http2.GoAwayFrame:
			numGoAway++

			want := http2.ErrCodeNo
			if got := f.ErrCode; got != want {
				t.Fatalf("f.ErrCode(%v): %v; want %v", numGoAway, got, want)
			}

			switch numGoAway {
			case 1:
				want := (uint32(1) << 31) - 1
				if got := f.LastStreamID; got != want {
					t.Fatalf("f.LastStreamID(%v): %v; want %v", numGoAway, got, want)
				}
			case 2:
				want := uint32(1)
				if got := f.LastStreamID; got != want {
					t.Fatalf("f.LastStreamID(%v): %v; want %v", numGoAway, got, want)
				}
			case 3:
				t.Fatalf("too many GOAWAYs received")
			}
		}
	}
}

// TestH2H2MultipleResponseCL tests that server returns error if
// multiple Content-Length response header fields are received.
func TestH2H2MultipleResponseCL(t *testing.T) {
	opts := options{
		args: []string{"--http2-bridge"},
		handler: func(w http.ResponseWriter, _ *http.Request) {
			w.Header().Add("content-length", "1")
			w.Header().Add("content-length", "1")
		},
	}

	st := newServerTester(t, opts)
	defer st.Close()

	res, err := st.http2(requestParam{
		name: "TestH2H2MultipleResponseCL",
	})
	if err != nil {
		t.Fatalf("Error st.http2() = %v", err)
	}

	if got, want := res.errCode, http2.ErrCodeInternal; got != want {
		t.Errorf("res.errCode: %v; want %v", got, want)
	}
}

// TestH2H2InvalidResponseCL tests that server returns error if
// Content-Length response header field value cannot be parsed as a
// number.
func TestH2H2InvalidResponseCL(t *testing.T) {
	opts := options{
		args: []string{"--http2-bridge"},
		handler: func(w http.ResponseWriter, _ *http.Request) {
			w.Header().Add("content-length", "")
		},
	}

	st := newServerTester(t, opts)
	defer st.Close()

	res, err := st.http2(requestParam{
		name: "TestH2H2InvalidResponseCL",
	})
	if err != nil {
		t.Fatalf("Error st.http2() = %v", err)
	}

	if got, want := res.errCode, http2.ErrCodeInternal; got != want {
		t.Errorf("res.errCode: %v; want %v", got, want)
	}
}

// // TestH2H2ConnectFailure tests that server handles the situation that
// // connection attempt to HTTP/2 backend failed.
// func TestH2H2ConnectFailure(t *testing.T) {
// 	opts := options{
// 		args: []string{"--http2-bridge"},
// 	}
// 	st := newServerTester(t, opts)
// 	defer st.Close()

// 	// simulate backend connect attempt failure
// 	st.ts.Close()

// 	res, err := st.http2(requestParam{
// 		name: "TestH2H2ConnectFailure",
// 	})
// 	if err != nil {
// 		t.Fatalf("Error st.http2() = %v", err)
// 	}
// 	want := 503
// 	if got := res.status; got != want {
// 		t.Errorf("status: %v; want %v", got, want)
// 	}
// }

// TestH2H2HostRewrite tests that server rewrites host header field
func TestH2H2HostRewrite(t *testing.T) {
	opts := options{
		args: []string{"--http2-bridge", "--host-rewrite"},
		handler: func(w http.ResponseWriter, r *http.Request) {
			w.Header().Add("request-host", r.Host)
		},
	}

	st := newServerTester(t, opts)
	defer st.Close()

	res, err := st.http2(requestParam{
		name: "TestH2H2HostRewrite",
	})
	if err != nil {
		t.Fatalf("Error st.http2() = %v", err)
	}

	if got, want := res.status, http.StatusOK; got != want {
		t.Errorf("status: %v; want %v", got, want)
	}

	if got, want := res.header.Get("request-host"), st.backendHost; got != want {
		t.Errorf("request-host: %v; want %v", got, want)
	}
}

// TestH2H2NoHostRewrite tests that server does not rewrite host
// header field
func TestH2H2NoHostRewrite(t *testing.T) {
	opts := options{
		args: []string{"--http2-bridge"},
		handler: func(w http.ResponseWriter, r *http.Request) {
			w.Header().Add("request-host", r.Host)
		},
	}

	st := newServerTester(t, opts)
	defer st.Close()

	res, err := st.http2(requestParam{
		name: "TestH2H2NoHostRewrite",
	})
	if err != nil {
		t.Fatalf("Error st.http2() = %v", err)
	}

	if got, want := res.status, http.StatusOK; got != want {
		t.Errorf("status: %v; want %v", got, want)
	}

	if got, want := res.header.Get("request-host"), st.frontendHost; got != want {
		t.Errorf("request-host: %v; want %v", got, want)
	}
}

// TestH2H2TLSXfp tests nghttpx sends x-forwarded-proto header field
// with http value since :scheme is http, even if the frontend
// connection is encrypted.
func TestH2H2TLSXfp(t *testing.T) {
	opts := options{
		args: []string{"--http2-bridge"},
		handler: func(_ http.ResponseWriter, r *http.Request) {
			if got, want := r.Header.Get("x-forwarded-proto"), "http"; got != want {
				t.Errorf("x-forwarded-proto: want %v; got %v", want, got)
			}
		},
		tls: true,
	}

	st := newServerTester(t, opts)
	defer st.Close()

	res, err := st.http2(requestParam{
		name: "TestH2H2TLSXfp",
	})
	if err != nil {
		t.Fatalf("Error st.http2() = %v", err)
	}

	if got, want := res.status, http.StatusOK; got != want {
		t.Errorf("res.status: %v; want %v", got, want)
	}
}

// TestH2H2AddXfp tests that server appends :scheme to the existing
// x-forwarded-proto header field.
func TestH2H2AddXfp(t *testing.T) {
	opts := options{
		args: []string{
			"--http2-bridge",
			"--no-strip-incoming-x-forwarded-proto",
		},
		handler: func(_ http.ResponseWriter, r *http.Request) {
			xfp := r.Header.Get("X-Forwarded-Proto")
			if got, want := xfp, "foo, http"; got != want {
				t.Errorf("X-Forwarded-Proto = %q; want %q", got, want)
			}
		},
		tls: true,
	}

	st := newServerTester(t, opts)
	defer st.Close()

	res, err := st.http2(requestParam{
		name: "TestH2H2AddXfp",
		header: []hpack.HeaderField{
			pair("x-forwarded-proto", "foo"),
		},
	})
	if err != nil {
		t.Fatalf("Error st.http2() = %v", err)
	}

	if got, want := res.status, http.StatusOK; got != want {
		t.Errorf("status = %v; want %v", got, want)
	}
}

// TestH2H2NoAddXfp tests that server does not append :scheme to the
// existing x-forwarded-proto header field.
func TestH2H2NoAddXfp(t *testing.T) {
	opts := options{
		args: []string{
			"--http2-bridge",
			"--no-add-x-forwarded-proto",
			"--no-strip-incoming-x-forwarded-proto",
		},
		handler: func(_ http.ResponseWriter, r *http.Request) {
			xfp := r.Header.Get("X-Forwarded-Proto")
			if got, want := xfp, "foo"; got != want {
				t.Errorf("X-Forwarded-Proto = %q; want %q", got, want)
			}
		},
		tls: true,
	}

	st := newServerTester(t, opts)
	defer st.Close()

	res, err := st.http2(requestParam{
		name: "TestH2H2NoAddXfp",
		header: []hpack.HeaderField{
			pair("x-forwarded-proto", "foo"),
		},
	})
	if err != nil {
		t.Fatalf("Error st.http2() = %v", err)
	}

	if got, want := res.status, http.StatusOK; got != want {
		t.Errorf("status = %v; want %v", got, want)
	}
}

// TestH2H2StripXfp tests that server strips incoming
// x-forwarded-proto header field.
func TestH2H2StripXfp(t *testing.T) {
	opts := options{
		args: []string{"--http2-bridge"},
		handler: func(_ http.ResponseWriter, r *http.Request) {
			xfp := r.Header.Get("X-Forwarded-Proto")
			if got, want := xfp, "http"; got != want {
				t.Errorf("X-Forwarded-Proto = %q; want %q", got, want)
			}
		},
		tls: true,
	}

	st := newServerTester(t, opts)
	defer st.Close()

	res, err := st.http2(requestParam{
		name: "TestH2H2StripXfp",
		header: []hpack.HeaderField{
			pair("x-forwarded-proto", "foo"),
		},
	})
	if err != nil {
		t.Fatalf("Error st.http2() = %v", err)
	}

	if got, want := res.status, http.StatusOK; got != want {
		t.Errorf("status = %v; want %v", got, want)
	}
}

// TestH2H2StripNoAddXfp tests that server strips incoming
// x-forwarded-proto header field, and does not add another.
func TestH2H2StripNoAddXfp(t *testing.T) {
	opts := options{
		args: []string{"--http2-bridge", "--no-add-x-forwarded-proto"},
		handler: func(_ http.ResponseWriter, r *http.Request) {
			if got, found := r.Header["X-Forwarded-Proto"]; found {
				t.Errorf("X-Forwarded-Proto = %q; want nothing", got)
			}
		},
		tls: true,
	}

	st := newServerTester(t, opts)
	defer st.Close()

	res, err := st.http2(requestParam{
		name: "TestH2H2StripNoAddXfp",
		header: []hpack.HeaderField{
			pair("x-forwarded-proto", "foo"),
		},
	})
	if err != nil {
		t.Fatalf("Error st.http2() = %v", err)
	}

	if got, want := res.status, http.StatusOK; got != want {
		t.Errorf("status = %v; want %v", got, want)
	}
}

// TestH2H2AddXff tests that server generates X-Forwarded-For header
// field when forwarding request to backend.
func TestH2H2AddXff(t *testing.T) {
	opts := options{
		args: []string{"--http2-bridge", "--add-x-forwarded-for"},
		handler: func(_ http.ResponseWriter, r *http.Request) {
			xff := r.Header.Get("X-Forwarded-For")
			want := "127.0.0.1"
			if xff != want {
				t.Errorf("X-Forwarded-For = %v; want %v", xff, want)
			}
		},
		tls: true,
	}

	st := newServerTester(t, opts)
	defer st.Close()

	res, err := st.http2(requestParam{
		name: "TestH2H2AddXff",
	})
	if err != nil {
		t.Fatalf("Error st.http2() = %v", err)
	}

	if got, want := res.status, http.StatusOK; got != want {
		t.Errorf("status = %v; want %v", got, want)
	}
}

// TestH2H2AddXff2 tests that server appends X-Forwarded-For header
// field to existing one when forwarding request to backend.
func TestH2H2AddXff2(t *testing.T) {
	opts := options{
		args: []string{"--http2-bridge", "--add-x-forwarded-for"},
		handler: func(_ http.ResponseWriter, r *http.Request) {
			xff := r.Header.Get("X-Forwarded-For")
			want := "host, 127.0.0.1"
			if xff != want {
				t.Errorf("X-Forwarded-For = %v; want %v", xff, want)
			}
		},
		tls: true,
	}

	st := newServerTester(t, opts)
	defer st.Close()

	res, err := st.http2(requestParam{
		name: "TestH2H2AddXff2",
		header: []hpack.HeaderField{
			pair("x-forwarded-for", "host"),
		},
	})
	if err != nil {
		t.Fatalf("Error st.http2() = %v", err)
	}

	if got, want := res.status, http.StatusOK; got != want {
		t.Errorf("status = %v; want %v", got, want)
	}
}

// TestH2H2StripXff tests that --strip-incoming-x-forwarded-for
// option.
func TestH2H2StripXff(t *testing.T) {
	opts := options{
		args: []string{
			"--http2-bridge",
			"--strip-incoming-x-forwarded-for",
		},
		handler: func(_ http.ResponseWriter, r *http.Request) {
			if xff, found := r.Header["X-Forwarded-For"]; found {
				t.Errorf("X-Forwarded-For = %v; want nothing", xff)
			}
		},
		tls: true,
	}

	st := newServerTester(t, opts)
	defer st.Close()

	res, err := st.http2(requestParam{
		name: "TestH2H2StripXff",
		header: []hpack.HeaderField{
			pair("x-forwarded-for", "host"),
		},
	})
	if err != nil {
		t.Fatalf("Error st.http2() = %v", err)
	}

	if got, want := res.status, http.StatusOK; got != want {
		t.Errorf("status = %v; want %v", got, want)
	}
}

// TestH2H2StripAddXff tests that --strip-incoming-x-forwarded-for and
// --add-x-forwarded-for options.
func TestH2H2StripAddXff(t *testing.T) {
	opts := options{
		args: []string{
			"--http2-bridge",
			"--strip-incoming-x-forwarded-for",
			"--add-x-forwarded-for",
		},
		handler: func(_ http.ResponseWriter, r *http.Request) {
			xff := r.Header.Get("X-Forwarded-For")
			want := "127.0.0.1"
			if xff != want {
				t.Errorf("X-Forwarded-For = %v; want %v", xff, want)
			}
		},
		tls: true,
	}

	st := newServerTester(t, opts)
	defer st.Close()

	res, err := st.http2(requestParam{
		name: "TestH2H2StripAddXff",
		header: []hpack.HeaderField{
			pair("x-forwarded-for", "host"),
		},
	})
	if err != nil {
		t.Fatalf("Error st.http2() = %v", err)
	}

	if got, want := res.status, http.StatusOK; got != want {
		t.Errorf("status = %v; want %v", got, want)
	}
}

// TestH2H2AddForwarded tests that server generates Forwarded header
// field using static obfuscated "by" parameter.
func TestH2H2AddForwarded(t *testing.T) {
	opts := options{
		args: []string{
			"--http2-bridge",
			"--add-forwarded=by,for,host,proto",
			"--forwarded-by=_alpha",
		},
		handler: func(_ http.ResponseWriter, r *http.Request) {
			pattern := fmt.Sprintf(`by=_alpha;for=_[^;]+;host="127\.0\.0\.1:%v";proto=https`, serverPort)
			validFwd := regexp.MustCompile(pattern)
			if got := r.Header.Get("Forwarded"); !validFwd.MatchString(got) {
				t.Errorf("Forwarded = %v; want pattern %v", got, pattern)
			}
		},
		tls: true,
	}

	st := newServerTester(t, opts)
	defer st.Close()

	res, err := st.http2(requestParam{
		name:   "TestH2H2AddForwarded",
		scheme: "https",
	})
	if err != nil {
		t.Fatalf("Error st.http2() = %v", err)
	}

	if got, want := res.status, http.StatusOK; got != want {
		t.Errorf("status: %v; want %v", got, want)
	}
}

// TestH2H2AddForwardedMerge tests that server generates Forwarded
// header field using static obfuscated "by" parameter, and
// existing Forwarded header field.
func TestH2H2AddForwardedMerge(t *testing.T) {
	opts := options{
		args: []string{
			"--http2-bridge",
			"--add-forwarded=by,host,proto",
			"--forwarded-by=_alpha",
		},
		handler: func(_ http.ResponseWriter, r *http.Request) {
			want := fmt.Sprintf(`host=foo, by=_alpha;host="127.0.0.1:%v";proto=https`, serverPort)
			if got := r.Header.Get("Forwarded"); got != want {
				t.Errorf("Forwarded = %v; want %v", got, want)
			}
		},
		tls: true,
	}

	st := newServerTester(t, opts)
	defer st.Close()

	res, err := st.http2(requestParam{
		name:   "TestH2H2AddForwardedMerge",
		scheme: "https",
		header: []hpack.HeaderField{
			pair("forwarded", "host=foo"),
		},
	})
	if err != nil {
		t.Fatalf("Error st.http2() = %v", err)
	}

	if got, want := res.status, http.StatusOK; got != want {
		t.Errorf("status: %v; want %v", got, want)
	}
}

// TestH2H2AddForwardedStrip tests that server generates Forwarded
// header field using static obfuscated "by" parameter, and
// existing Forwarded header field stripped.
func TestH2H2AddForwardedStrip(t *testing.T) {
	opts := options{
		args: []string{
			"--http2-bridge",
			"--strip-incoming-forwarded",
			"--add-forwarded=by,host,proto",
			"--forwarded-by=_alpha",
		},
		handler: func(_ http.ResponseWriter, r *http.Request) {
			want := fmt.Sprintf(`by=_alpha;host="127.0.0.1:%v";proto=https`, serverPort)
			if got := r.Header.Get("Forwarded"); got != want {
				t.Errorf("Forwarded = %v; want %v", got, want)
			}
		},
		tls: true,
	}

	st := newServerTester(t, opts)
	defer st.Close()

	res, err := st.http2(requestParam{
		name:   "TestH2H2AddForwardedStrip",
		scheme: "https",
		header: []hpack.HeaderField{
			pair("forwarded", "host=foo"),
		},
	})
	if err != nil {
		t.Fatalf("Error st.http2() = %v", err)
	}

	if got, want := res.status, http.StatusOK; got != want {
		t.Errorf("status: %v; want %v", got, want)
	}
}

// TestH2H2StripForwarded tests that server strips incoming Forwarded
// header field.
func TestH2H2StripForwarded(t *testing.T) {
	opts := options{
		args: []string{"--http2-bridge", "--strip-incoming-forwarded"},
		handler: func(_ http.ResponseWriter, r *http.Request) {
			if got, found := r.Header["Forwarded"]; found {
				t.Errorf("Forwarded = %v; want nothing", got)
			}
		},
		tls: true,
	}

	st := newServerTester(t, opts)
	defer st.Close()

	res, err := st.http2(requestParam{
		name:   "TestH2H2StripForwarded",
		scheme: "https",
		header: []hpack.HeaderField{
			pair("forwarded", "host=foo"),
		},
	})
	if err != nil {
		t.Fatalf("Error st.http2() = %v", err)
	}

	if got, want := res.status, http.StatusOK; got != want {
		t.Errorf("status: %v; want %v", got, want)
	}
}

// TestH2H2ReqPhaseReturn tests mruby request phase hook returns
// custom response.
func TestH2H2ReqPhaseReturn(t *testing.T) {
	opts := options{
		args: []string{
			"--http2-bridge",
			"--mruby-file=" + testDir + "/req-return.rb",
		},
		handler: func(http.ResponseWriter, *http.Request) {
			t.Fatalf("request should not be forwarded")
		},
	}

	st := newServerTester(t, opts)
	defer st.Close()

	res, err := st.http2(requestParam{
		name: "TestH2H2ReqPhaseReturn",
	})
	if err != nil {
		t.Fatalf("Error st.http2() = %v", err)
	}

	if got, want := res.status, http.StatusNotFound; got != want {
		t.Errorf("status = %v; want %v", got, want)
	}

	hdtests := []struct {
		k, v string
	}{
		{"content-length", "20"},
		{"from", "mruby"},
	}

	for _, tt := range hdtests {
		if got, want := res.header.Get(tt.k), tt.v; got != want {
			t.Errorf("%v = %v; want %v", tt.k, got, want)
		}
	}

	if got, want := string(res.body), "Hello World from req"; got != want {
		t.Errorf("body = %v; want %v", got, want)
	}
}

// TestH2H2RespPhaseReturn tests mruby response phase hook returns
// custom response.
func TestH2H2RespPhaseReturn(t *testing.T) {
	opts := options{
		args: []string{
			"--http2-bridge",
			"--mruby-file=" + testDir + "/resp-return.rb",
		},
	}

	st := newServerTester(t, opts)
	defer st.Close()

	res, err := st.http2(requestParam{
		name: "TestH2H2RespPhaseReturn",
	})
	if err != nil {
		t.Fatalf("Error st.http2() = %v", err)
	}

	if got, want := res.status, http.StatusNotFound; got != want {
		t.Errorf("status = %v; want %v", got, want)
	}

	hdtests := []struct {
		k, v string
	}{
		{"content-length", "21"},
		{"from", "mruby"},
	}

	for _, tt := range hdtests {
		if got, want := res.header.Get(tt.k), tt.v; got != want {
			t.Errorf("%v = %v; want %v", tt.k, got, want)
		}
	}

	if got, want := string(res.body), "Hello World from resp"; got != want {
		t.Errorf("body = %v; want %v", got, want)
	}
}

// TestH2H2ExternalDNS tests that DNS resolution using external DNS
// with HTTP/2 backend works.
func TestH2H2ExternalDNS(t *testing.T) {
	opts := options{
		args: []string{"--http2-bridge", "--external-dns"},
	}

	st := newServerTester(t, opts)
	defer st.Close()

	res, err := st.http2(requestParam{
		name: "TestH2H2ExternalDNS",
	})
	if err != nil {
		t.Fatalf("Error st.http2() = %v", err)
	}

	if got, want := res.status, http.StatusOK; got != want {
		t.Errorf("status = %v; want %v", got, want)
	}
}

// TestH2H2DNS tests that DNS resolution without external DNS with
// HTTP/2 backend works.
func TestH2H2DNS(t *testing.T) {
	opts := options{
		args: []string{"--http2-bridge", "--dns"},
	}

	st := newServerTester(t, opts)
	defer st.Close()

	res, err := st.http2(requestParam{
		name: "TestH2H2DNS",
	})
	if err != nil {
		t.Fatalf("Error st.http2() = %v", err)
	}

	if got, want := res.status, http.StatusOK; got != want {
		t.Errorf("status = %v; want %v", got, want)
	}
}

// TestH2H2Code204 tests that 204 response without content-length, and
// transfer-encoding is valid.
func TestH2H2Code204(t *testing.T) {
	opts := options{
		args: []string{"--http2-bridge"},
		handler: func(w http.ResponseWriter, _ *http.Request) {
			w.WriteHeader(http.StatusNoContent)
		},
	}

	st := newServerTester(t, opts)
	defer st.Close()

	res, err := st.http2(requestParam{
		name: "TestH2H2Code204",
	})
	if err != nil {
		t.Fatalf("Error st.http2() = %v", err)
	}

	if got, want := res.status, http.StatusNoContent; got != want {
		t.Errorf("status = %v; want %v", got, want)
	}
}

// TestH2APIBackendconfig exercise backendconfig API endpoint routine
// for successful case.
func TestH2APIBackendconfig(t *testing.T) {
	opts := options{
		args: []string{"-f127.0.0.1,3010;api;no-tls"},
		handler: func(http.ResponseWriter, *http.Request) {
			t.Fatalf("request should not be forwarded")
		},
		connectPort: 3010,
	}

	st := newServerTester(t, opts)
	defer st.Close()

	res, err := st.http2(requestParam{
		name:   "TestH2APIBackendconfig",
		path:   "/api/v1beta1/backendconfig",
		method: "PUT",
		body: []byte(`# comment
backend=127.0.0.1,3011

`),
	})
	if err != nil {
		t.Fatalf("Error st.http2() = %v", err)
	}

	if got, want := res.status, http.StatusOK; got != want {
		t.Errorf("res.status: %v; want %v", got, want)
	}

	var apiResp APIResponse

	if err := json.Unmarshal(res.body, &apiResp); err != nil {
		t.Fatalf("Error unmarshaling API response: %v", err)
	}

	if got, want := apiResp.Status, "Success"; got != want {
		t.Errorf("apiResp.Status: %v; want %v", got, want)
	}

	if got, want := apiResp.Code, 200; got != want {
		t.Errorf("apiResp.Status: %v; want %v", got, want)
	}
}

// TestH2APIBackendconfigQuery exercise backendconfig API endpoint
// routine with query.
func TestH2APIBackendconfigQuery(t *testing.T) {
	opts := options{
		args: []string{"-f127.0.0.1,3010;api;no-tls"},
		handler: func(http.ResponseWriter, *http.Request) {
			t.Fatalf("request should not be forwarded")
		},
		connectPort: 3010,
	}

	st := newServerTester(t, opts)
	defer st.Close()

	res, err := st.http2(requestParam{
		name:   "TestH2APIBackendconfigQuery",
		path:   "/api/v1beta1/backendconfig?foo=bar",
		method: "PUT",
		body: []byte(`# comment
backend=127.0.0.1,3011

`),
	})
	if err != nil {
		t.Fatalf("Error st.http2() = %v", err)
	}

	if got, want := res.status, http.StatusOK; got != want {
		t.Errorf("res.status: %v; want %v", got, want)
	}

	var apiResp APIResponse

	if err := json.Unmarshal(res.body, &apiResp); err != nil {
		t.Fatalf("Error unmarshaling API response: %v", err)
	}

	if got, want := apiResp.Status, "Success"; got != want {
		t.Errorf("apiResp.Status: %v; want %v", got, want)
	}

	if got, want := apiResp.Code, 200; got != want {
		t.Errorf("apiResp.Status: %v; want %v", got, want)
	}
}

// TestH2APIBackendconfigBadMethod exercise backendconfig API endpoint
// routine with bad method.
func TestH2APIBackendconfigBadMethod(t *testing.T) {
	opts := options{
		args: []string{"-f127.0.0.1,3010;api;no-tls"},
		handler: func(http.ResponseWriter, *http.Request) {
			t.Fatalf("request should not be forwarded")
		},
		connectPort: 3010,
	}

	st := newServerTester(t, opts)
	defer st.Close()

	res, err := st.http2(requestParam{
		name:   "TestH2APIBackendconfigBadMethod",
		path:   "/api/v1beta1/backendconfig",
		method: "GET",
		body: []byte(`# comment
backend=127.0.0.1,3011

`),
	})
	if err != nil {
		t.Fatalf("Error st.http2() = %v", err)
	}

	if got, want := res.status, http.StatusMethodNotAllowed; got != want {
		t.Errorf("res.status: %v; want %v", got, want)
	}

	var apiResp APIResponse

	if err := json.Unmarshal(res.body, &apiResp); err != nil {
		t.Fatalf("Error unmarshaling API response: %v", err)
	}

	if got, want := apiResp.Status, "Failure"; got != want {
		t.Errorf("apiResp.Status: %v; want %v", got, want)
	}

	if got, want := apiResp.Code, 405; got != want {
		t.Errorf("apiResp.Status: %v; want %v", got, want)
	}
}

// TestH2APIConfigrevision tests configrevision API.
func TestH2APIConfigrevision(t *testing.T) {
	opts := options{
		args: []string{"-f127.0.0.1,3010;api;no-tls"},
		handler: func(http.ResponseWriter, *http.Request) {
			t.Fatalf("request should not be forwarded")
		},
		connectPort: 3010,
	}

	st := newServerTester(t, opts)
	defer st.Close()

	res, err := st.http2(requestParam{
		name:   "TestH2APIConfigrevision",
		path:   "/api/v1beta1/configrevision",
		method: "GET",
	})
	if err != nil {
		t.Fatalf("Error st.http2() = %v", err)
	}

	if got, want := res.status, http.StatusOK; got != want {
		t.Errorf("res.status: %v; want = %v", got, want)
	}

	var apiResp APIResponse

	d := json.NewDecoder(bytes.NewBuffer(res.body))
	d.UseNumber()

	if err := d.Decode(&apiResp); err != nil {
		t.Fatalf("Error unmarshalling API response: %v", err)
	}

	if got, want := apiResp.Status, "Success"; got != want {
		t.Errorf("apiResp.Status: %v; want %v", got, want)
	}

	if got, want := apiResp.Code, 200; got != want {
		t.Errorf("apiResp.Status: %v; want %v", got, want)
	}

	if got, want := apiResp.Data["configRevision"], json.Number("0"); got != want {
		t.Errorf(`apiResp.Data["configRevision"]: %v %t; want %v`, got, got, want)
	}
}

// TestH2APINotFound exercise backendconfig API endpoint routine when
// API endpoint is not found.
func TestH2APINotFound(t *testing.T) {
	opts := options{
		args: []string{"-f127.0.0.1,3010;api;no-tls"},
		handler: func(http.ResponseWriter, *http.Request) {
			t.Fatalf("request should not be forwarded")
		},
		connectPort: 3010,
	}

	st := newServerTester(t, opts)
	defer st.Close()

	res, err := st.http2(requestParam{
		name:   "TestH2APINotFound",
		path:   "/api/notfound",
		method: "GET",
		body: []byte(`# comment
backend=127.0.0.1,3011

`),
	})
	if err != nil {
		t.Fatalf("Error st.http2() = %v", err)
	}

	if got, want := res.status, http.StatusNotFound; got != want {
		t.Errorf("res.status: %v; want %v", got, want)
	}

	var apiResp APIResponse

	if err := json.Unmarshal(res.body, &apiResp); err != nil {
		t.Fatalf("Error unmarshaling API response: %v", err)
	}

	if got, want := apiResp.Status, "Failure"; got != want {
		t.Errorf("apiResp.Status: %v; want %v", got, want)
	}

	if got, want := apiResp.Code, 404; got != want {
		t.Errorf("apiResp.Status: %v; want %v", got, want)
	}
}

// TestH2Healthmon tests health monitor endpoint.
func TestH2Healthmon(t *testing.T) {
	opts := options{
		args: []string{"-f127.0.0.1,3011;healthmon;no-tls"},
		handler: func(http.ResponseWriter, *http.Request) {
			t.Fatalf("request should not be forwarded")
		},
		connectPort: 3011,
	}

	st := newServerTester(t, opts)
	defer st.Close()

	res, err := st.http2(requestParam{
		name: "TestH2Healthmon",
		path: "/alpha/bravo",
	})
	if err != nil {
		t.Fatalf("Error st.http2() = %v", err)
	}

	if got, want := res.status, http.StatusOK; got != want {
		t.Errorf("res.status: %v; want %v", got, want)
	}
}

// TestH2ResponseBeforeRequestEnd tests the situation where response
// ends before request body finishes.
func TestH2ResponseBeforeRequestEnd(t *testing.T) {
	opts := options{
		args: []string{"--mruby-file=" + testDir + "/req-return.rb"},
		handler: func(http.ResponseWriter, *http.Request) {
			t.Fatal("request should not be forwarded")
		},
	}

	st := newServerTester(t, opts)
	defer st.Close()

	res, err := st.http2(requestParam{
		name:        "TestH2ResponseBeforeRequestEnd",
		noEndStream: true,
	})
	if err != nil {
		t.Fatalf("Error st.http2() = %v", err)
	}

	if got, want := res.status, http.StatusNotFound; got != want {
		t.Errorf("res.status: %v; want %v", got, want)
	}
}

// TestH2H1ChunkedEndsPrematurely tests that a stream is reset if the
// backend chunked encoded response ends prematurely.
func TestH2H1ChunkedEndsPrematurely(t *testing.T) {
	opts := options{
		handler: func(w http.ResponseWriter, _ *http.Request) {
			hj, ok := w.(http.Hijacker)
			if !ok {
				http.Error(w, "Could not hijack the connection", http.StatusInternalServerError)
				return
			}

			conn, bufrw, err := hj.Hijack()
			if err != nil {
				http.Error(w, err.Error(), http.StatusInternalServerError)
				return
			}

			defer conn.Close()

			if _, err := bufrw.WriteString("HTTP/1.1 200\r\nTransfer-Encoding: chunked\r\n\r\n"); err != nil {
				t.Fatalf("Error bufrw.WriteString() = %v", err)
			}

			bufrw.Flush()
		},
	}

	st := newServerTester(t, opts)
	defer st.Close()

	res, err := st.http2(requestParam{
		name: "TestH2H1ChunkedEndsPrematurely",
	})
	if err != nil {
		t.Fatalf("Error st.http2() = %v", err)
	}

	if got, want := res.errCode, http2.ErrCodeInternal; got != want {
		t.Errorf("res.errCode = %v; want %v", got, want)
	}
}

// TestH2H1RequireHTTPSchemeHTTPSWithoutEncryption verifies that https
// scheme in non-encrypted connection is treated as error.
func TestH2H1RequireHTTPSchemeHTTPSWithoutEncryption(t *testing.T) {
	opts := options{
		args: []string{"--require-http-scheme"},
		handler: func(http.ResponseWriter, *http.Request) {
			t.Errorf("server should not forward this request")
		},
	}

	st := newServerTester(t, opts)
	defer st.Close()

	res, err := st.http2(requestParam{
		name:   "TestH2H1RequireHTTPSchemeHTTPSWithoutEncryption",
		scheme: "https",
	})
	if err != nil {
		t.Fatalf("Error st.http2() = %v", err)
	}

	if got, want := res.status, http.StatusBadRequest; got != want {
		t.Errorf("status = %v; want %v", got, want)
	}
}

// TestH2H1RequireHTTPSchemeHTTPWithEncryption verifies that http
// scheme in encrypted connection is treated as error.
func TestH2H1RequireHTTPSchemeHTTPWithEncryption(t *testing.T) {
	opts := options{
		args: []string{"--require-http-scheme"},
		handler: func(http.ResponseWriter, *http.Request) {
			t.Errorf("server should not forward this request")
		},
		tls: true,
	}

	st := newServerTester(t, opts)
	defer st.Close()

	res, err := st.http2(requestParam{
		name:   "TestH2H1RequireHTTPSchemeHTTPWithEncryption",
		scheme: "http",
	})
	if err != nil {
		t.Fatalf("Error st.http2() = %v", err)
	}

	if got, want := res.status, http.StatusBadRequest; got != want {
		t.Errorf("status = %v; want %v", got, want)
	}
}

// TestH2H1RequireHTTPSchemeUnknownSchemeWithoutEncryption verifies
// that unknown scheme in non-encrypted connection is treated as
// error.
func TestH2H1RequireHTTPSchemeUnknownSchemeWithoutEncryption(t *testing.T) {
	opts := options{
		args: []string{"--require-http-scheme"},
		handler: func(http.ResponseWriter, *http.Request) {
			t.Errorf("server should not forward this request")
		},
	}

	st := newServerTester(t, opts)
	defer st.Close()

	res, err := st.http2(requestParam{
		name:   "TestH2H1RequireHTTPSchemeUnknownSchemeWithoutEncryption",
		scheme: "unknown",
	})
	if err != nil {
		t.Fatalf("Error st.http2() = %v", err)
	}

	if got, want := res.status, http.StatusBadRequest; got != want {
		t.Errorf("status = %v; want %v", got, want)
	}
}

// TestH2H1RequireHTTPSchemeUnknownSchemeWithEncryption verifies that
// unknown scheme in encrypted connection is treated as error.
func TestH2H1RequireHTTPSchemeUnknownSchemeWithEncryption(t *testing.T) {
	opts := options{
		args: []string{"--require-http-scheme"},
		handler: func(http.ResponseWriter, *http.Request) {
			t.Errorf("server should not forward this request")
		},
		tls: true,
	}

	st := newServerTester(t, opts)
	defer st.Close()

	res, err := st.http2(requestParam{
		name:   "TestH2H1RequireHTTPSchemeUnknownSchemeWithEncryption",
		scheme: "unknown",
	})
	if err != nil {
		t.Fatalf("Error st.http2() = %v", err)
	}

	if got, want := res.status, http.StatusBadRequest; got != want {
		t.Errorf("status = %v; want %v", got, want)
	}
}
