package nghttp2

import (
	"bufio"
	"bytes"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"net/http"
	"regexp"
	"syscall"
	"testing"
	"time"

	"golang.org/x/net/http2/hpack"
	"golang.org/x/net/websocket"
)

// TestH1H1PlainGET tests whether simple HTTP/1 GET request works.
func TestH1H1PlainGET(t *testing.T) {
	st := newServerTester(t, options{})
	defer st.Close()

	res, err := st.http1(requestParam{
		name: "TestH1H1PlainGET",
	})
	if err != nil {
		t.Fatalf("Error st.http1() = %v", err)
	}

	if got, want := res.status, http.StatusOK; got != want {
		t.Errorf("status = %v; want %v", got, want)
	}
}

// TestH1H1PlainGETClose tests whether simple HTTP/1 GET request with
// Connection: close request header field works.
func TestH1H1PlainGETClose(t *testing.T) {
	st := newServerTester(t, options{})
	defer st.Close()

	res, err := st.http1(requestParam{
		name: "TestH1H1PlainGETClose",
		header: []hpack.HeaderField{
			pair("Connection", "close"),
		},
	})
	if err != nil {
		t.Fatalf("Error st.http1() = %v", err)
	}

	if got, want := res.status, http.StatusOK; got != want {
		t.Errorf("status = %v; want %v", got, want)
	}
}

// TestH1H1InvalidMethod tests that server rejects invalid method with
// 501 status code
func TestH1H1InvalidMethod(t *testing.T) {
	opts := options{
		handler: func(http.ResponseWriter, *http.Request) {
			t.Errorf("server should not forward this request")
		},
	}

	st := newServerTester(t, opts)
	defer st.Close()

	res, err := st.http1(requestParam{
		name:   "TestH1H1InvalidMethod",
		method: "get",
	})
	if err != nil {
		t.Fatalf("Error st.http1() = %v", err)
	}

	if got, want := res.status, http.StatusNotImplemented; got != want {
		t.Errorf("status = %v; want %v", got, want)
	}
}

// TestH1H1MultipleRequestCL tests that server rejects request which
// contains multiple Content-Length header fields.
func TestH1H1MultipleRequestCL(t *testing.T) {
	opts := options{
		handler: func(http.ResponseWriter, *http.Request) {
			t.Errorf("server should not forward bad request")
		},
	}

	st := newServerTester(t, opts)
	defer st.Close()

	if _, err := io.WriteString(st.conn, fmt.Sprintf("GET / HTTP/1.1\r\nHost: %v\r\nTest-Case: TestH1H1MultipleRequestCL\r\nContent-Length: 0\r\nContent-Length: 0\r\n\r\n",
		st.authority)); err != nil {
		t.Fatalf("Error io.WriteString() = %v", err)
	}

	resp, err := http.ReadResponse(bufio.NewReader(st.conn), nil)
	if err != nil {
		t.Fatalf("Error http.ReadResponse() = %v", err)
	}

	defer resp.Body.Close()

	if got, want := resp.StatusCode, http.StatusBadRequest; got != want {
		t.Errorf("status: %v; want %v", got, want)
	}
}

// // TestH1H1ConnectFailure tests that server handles the situation that
// // connection attempt to HTTP/1 backend failed.
// func TestH1H1ConnectFailure(t *testing.T) {
// 	st := newServerTester(t, options{})
// 	defer st.Close()

// 	// shutdown backend server to simulate backend connect failure
// 	st.ts.Close()

// 	res, err := st.http1(requestParam{
// 		name: "TestH1H1ConnectFailure",
// 	})
// 	if err != nil {
// 		t.Fatalf("Error st.http1() = %v", err)
// 	}
// 	want := 503
// 	if got := res.status; got != want {
// 		t.Errorf("status: %v; want %v", got, want)
// 	}
// }

// TestH1H1AffinityCookie tests that affinity cookie is sent back in
// cleartext http.
func TestH1H1AffinityCookie(t *testing.T) {
	opts := options{
		args: []string{"--affinity-cookie"},
	}

	st := newServerTester(t, opts)
	defer st.Close()

	res, err := st.http1(requestParam{
		name: "TestH1H1AffinityCookie",
	})
	if err != nil {
		t.Fatalf("Error st.http1() = %v", err)
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

// TestH1H1AffinityCookieTLS tests that affinity cookie is sent back
// in https.
func TestH1H1AffinityCookieTLS(t *testing.T) {
	opts := options{
		args: []string{"--alpn-h1", "--affinity-cookie"},
		tls:  true,
	}

	st := newServerTester(t, opts)
	defer st.Close()

	res, err := st.http1(requestParam{
		name: "TestH1H1AffinityCookieTLS",
	})
	if err != nil {
		t.Fatalf("Error st.http1() = %v", err)
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

// TestH1H1GracefulShutdown tests graceful shutdown.
func TestH1H1GracefulShutdown(t *testing.T) {
	st := newServerTester(t, options{})
	defer st.Close()

	res, err := st.http1(requestParam{
		name: "TestH1H1GracefulShutdown-1",
	})
	if err != nil {
		t.Fatalf("Error st.http1() = %v", err)
	}

	if got, want := res.status, http.StatusOK; got != want {
		t.Errorf("status: %v; want %v", got, want)
	}

	if err := st.cmd.Process.Signal(syscall.SIGQUIT); err != nil {
		t.Fatalf("Error st.cmd.Process.Signal() = %v", err)
	}

	time.Sleep(150 * time.Millisecond)

	res, err = st.http1(requestParam{
		name: "TestH1H1GracefulShutdown-2",
	})
	if err != nil {
		t.Fatalf("Error st.http1() = %v", err)
	}

	if got, want := res.status, http.StatusOK; got != want {
		t.Errorf("status: %v; want %v", got, want)
	}

	if got, want := res.connClose, true; got != want {
		t.Errorf("res.connClose: %v; want %v", got, want)
	}

	want := io.EOF
	b := make([]byte, 256)

	if _, err := st.conn.Read(b); !errors.Is(err, want) {
		t.Errorf("st.conn.Read(): %v; want %v", err, want)
	}
}

// TestH1H1HostRewrite tests that server rewrites Host header field
func TestH1H1HostRewrite(t *testing.T) {
	opts := options{
		args: []string{"--host-rewrite"},
		handler: func(w http.ResponseWriter, r *http.Request) {
			w.Header().Add("request-host", r.Host)
		},
	}

	st := newServerTester(t, opts)
	defer st.Close()

	res, err := st.http1(requestParam{
		name: "TestH1H1HostRewrite",
	})
	if err != nil {
		t.Fatalf("Error st.http1() = %v", err)
	}

	if got, want := res.status, http.StatusOK; got != want {
		t.Errorf("status: %v; want %v", got, want)
	}

	if got, want := res.header.Get("request-host"), st.backendHost; got != want {
		t.Errorf("request-host: %v; want %v", got, want)
	}
}

// TestH1H1BadHost tests that server rejects request including bad
// characters in host header field.
func TestH1H1BadHost(t *testing.T) {
	opts := options{
		handler: func(http.ResponseWriter, *http.Request) {
			t.Errorf("server should not forward this request")
		},
	}

	st := newServerTester(t, opts)
	defer st.Close()

	if _, err := io.WriteString(st.conn, "GET / HTTP/1.1\r\nTest-Case: TestH1H1HBadHost\r\nHost: foo\"bar\r\n\r\n"); err != nil {
		t.Fatalf("Error io.WriteString() = %v", err)
	}

	resp, err := http.ReadResponse(bufio.NewReader(st.conn), nil)
	if err != nil {
		t.Fatalf("Error http.ReadResponse() = %v", err)
	}

	defer resp.Body.Close()

	if got, want := resp.StatusCode, http.StatusBadRequest; got != want {
		t.Errorf("status: %v; want %v", got, want)
	}
}

// TestH1H1BadAuthority tests that server rejects request including
// bad characters in authority component of requset URI.
func TestH1H1BadAuthority(t *testing.T) {
	opts := options{
		handler: func(http.ResponseWriter, *http.Request) {
			t.Errorf("server should not forward this request")
		},
	}

	st := newServerTester(t, opts)
	defer st.Close()

	if _, err := io.WriteString(st.conn, "GET http://foo\"bar/ HTTP/1.1\r\nTest-Case: TestH1H1HBadAuthority\r\nHost: foobar\r\n\r\n"); err != nil {
		t.Fatalf("Error io.WriteString() = %v", err)
	}

	resp, err := http.ReadResponse(bufio.NewReader(st.conn), nil)
	if err != nil {
		t.Fatalf("Error http.ReadResponse() = %v", err)
	}

	defer resp.Body.Close()

	if got, want := resp.StatusCode, http.StatusBadRequest; got != want {
		t.Errorf("status: %v; want %v", got, want)
	}
}

// TestH1H1BadScheme tests that server rejects request including
// bad characters in scheme component of requset URI.
func TestH1H1BadScheme(t *testing.T) {
	opts := options{
		handler: func(http.ResponseWriter, *http.Request) {
			t.Errorf("server should not forward this request")
		},
	}

	st := newServerTester(t, opts)
	defer st.Close()

	if _, err := io.WriteString(st.conn, "GET http*://example.com/ HTTP/1.1\r\nTest-Case: TestH1H1HBadScheme\r\nHost: example.com\r\n\r\n"); err != nil {
		t.Fatalf("Error io.WriteString() = %v", err)
	}

	resp, err := http.ReadResponse(bufio.NewReader(st.conn), nil)
	if err != nil {
		t.Fatalf("Error http.ReadResponse() = %v", err)
	}

	defer resp.Body.Close()

	if got, want := resp.StatusCode, http.StatusBadRequest; got != want {
		t.Errorf("status: %v; want %v", got, want)
	}
}

// TestH1H1HTTP10 tests that server can accept HTTP/1.0 request
// without Host header field
func TestH1H1HTTP10(t *testing.T) {
	opts := options{
		handler: func(w http.ResponseWriter, r *http.Request) {
			w.Header().Add("request-host", r.Host)
		},
	}

	st := newServerTester(t, opts)
	defer st.Close()

	if _, err := io.WriteString(st.conn, "GET / HTTP/1.0\r\nTest-Case: TestH1H1HTTP10\r\n\r\n"); err != nil {
		t.Fatalf("Error io.WriteString() = %v", err)
	}

	resp, err := http.ReadResponse(bufio.NewReader(st.conn), nil)
	if err != nil {
		t.Fatalf("Error http.ReadResponse() = %v", err)
	}

	defer resp.Body.Close()

	if got, want := resp.StatusCode, http.StatusOK; got != want {
		t.Errorf("status: %v; want %v", got, want)
	}

	if got, want := resp.Header.Get("request-host"), st.backendHost; got != want {
		t.Errorf("request-host: %v; want %v", got, want)
	}
}

// TestH1H1HTTP10NoHostRewrite tests that server generates host header
// field using actual backend server even if --no-http-rewrite is
// used.
func TestH1H1HTTP10NoHostRewrite(t *testing.T) {
	opts := options{
		handler: func(w http.ResponseWriter, r *http.Request) {
			w.Header().Add("request-host", r.Host)
		},
	}

	st := newServerTester(t, opts)
	defer st.Close()

	if _, err := io.WriteString(st.conn, "GET / HTTP/1.0\r\nTest-Case: TestH1H1HTTP10NoHostRewrite\r\n\r\n"); err != nil {
		t.Fatalf("Error io.WriteString() = %v", err)
	}

	resp, err := http.ReadResponse(bufio.NewReader(st.conn), nil)
	if err != nil {
		t.Fatalf("Error http.ReadResponse() = %v", err)
	}

	defer resp.Body.Close()

	if got, want := resp.StatusCode, http.StatusOK; got != want {
		t.Errorf("status: %v; want %v", got, want)
	}

	if got, want := resp.Header.Get("request-host"), st.backendHost; got != want {
		t.Errorf("request-host: %v; want %v", got, want)
	}
}

// TestH1H1RequestTrailer tests request trailer part is forwarded to
// backend.
func TestH1H1RequestTrailer(t *testing.T) {
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

	res, err := st.http1(requestParam{
		name: "TestH1H1RequestTrailer",
		body: []byte("1"),
		trailer: []hpack.HeaderField{
			pair("foo", "bar"),
		},
	})
	if err != nil {
		t.Fatalf("Error st.http1() = %v", err)
	}

	if got, want := res.status, http.StatusOK; got != want {
		t.Errorf("res.status: %v; want %v", got, want)
	}
}

// TestH1H1HeaderFieldBufferPath tests that request with request path
// larger than configured buffer size is rejected.
func TestH1H1HeaderFieldBufferPath(t *testing.T) {
	// The value 100 is chosen so that sum of header fields bytes
	// does not exceed it.  We use > 100 bytes URI to exceed this
	// limit.
	opts := options{
		args: []string{"--request-header-field-buffer=100"},
		handler: func(http.ResponseWriter, *http.Request) {
			t.Fatal("execution path should not be here")
		},
	}

	st := newServerTester(t, opts)
	defer st.Close()

	res, err := st.http1(requestParam{
		name: "TestH1H1HeaderFieldBufferPath",
		path: "/aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaabbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb",
	})
	if err != nil {
		t.Fatalf("Error st.http1() = %v", err)
	}

	if got, want := res.status, http.StatusRequestHeaderFieldsTooLarge; got != want {
		t.Errorf("status: %v; want %v", got, want)
	}
}

// TestH1H1HeaderFieldBuffer tests that request with header fields
// larger than configured buffer size is rejected.
func TestH1H1HeaderFieldBuffer(t *testing.T) {
	opts := options{
		args: []string{"--request-header-field-buffer=10"},
		handler: func(http.ResponseWriter, *http.Request) {
			t.Fatal("execution path should not be here")
		},
	}

	st := newServerTester(t, opts)
	defer st.Close()

	res, err := st.http1(requestParam{
		name: "TestH1H1HeaderFieldBuffer",
	})
	if err != nil {
		t.Fatalf("Error st.http1() = %v", err)
	}

	if got, want := res.status, http.StatusRequestHeaderFieldsTooLarge; got != want {
		t.Errorf("status: %v; want %v", got, want)
	}
}

// TestH1H1HeaderFields tests that request with header fields more
// than configured number is rejected.
func TestH1H1HeaderFields(t *testing.T) {
	opts := options{
		args: []string{"--max-request-header-fields=1"},
		handler: func(http.ResponseWriter, *http.Request) {
			t.Fatal("execution path should not be here")
		},
	}

	st := newServerTester(t, opts)
	defer st.Close()

	res, err := st.http1(requestParam{
		name: "TestH1H1HeaderFields",
		header: []hpack.HeaderField{
			// Add extra header field to ensure that
			// header field limit exceeds
			pair("Connection", "close"),
		},
	})
	if err != nil {
		t.Fatalf("Error st.http1() = %v", err)
	}

	if got, want := res.status, http.StatusRequestHeaderFieldsTooLarge; got != want {
		t.Errorf("status: %v; want %v", got, want)
	}
}

// TestH1H1Websocket tests that HTTP Upgrade to WebSocket works.
func TestH1H1Websocket(t *testing.T) {
	opts := options{
		handler: websocket.Handler(func(ws *websocket.Conn) {
			if _, err := io.Copy(ws, ws); err != nil {
				t.Fatalf("Error io.Copy() = %v", err)
			}
		}).ServeHTTP,
	}

	st := newServerTester(t, opts)
	defer st.Close()

	content := []byte("hello world")
	res := st.websocket(requestParam{
		name: "TestH1H1Websocket",
		body: content,
	})

	if got, want := res.body, content; !bytes.Equal(got, want) {
		t.Errorf("echo: %q; want %q", got, want)
	}
}

// TestH1H1ReqPhaseSetHeader tests mruby request phase hook
// modifies request header fields.
func TestH1H1ReqPhaseSetHeader(t *testing.T) {
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

	res, err := st.http1(requestParam{
		name: "TestH1H1ReqPhaseSetHeader",
	})
	if err != nil {
		t.Fatalf("Error st.http1() = %v", err)
	}

	if got, want := res.status, http.StatusOK; got != want {
		t.Errorf("status = %v; want %v", got, want)
	}
}

// TestH1H1ReqPhaseReturn tests mruby request phase hook returns
// custom response.
func TestH1H1ReqPhaseReturn(t *testing.T) {
	opts := options{
		args: []string{"--mruby-file=" + testDir + "/req-return.rb"},
		handler: func(http.ResponseWriter, *http.Request) {
			t.Fatalf("request should not be forwarded")
		},
	}

	st := newServerTester(t, opts)
	defer st.Close()

	res, err := st.http1(requestParam{
		name: "TestH1H1ReqPhaseReturn",
	})
	if err != nil {
		t.Fatalf("Error st.http1() = %v", err)
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

// TestH1H1ReqPhaseReturnCONNECTMethod tests that mruby request phase
// hook resets llhttp HPE_PAUSED_UPGRADE.
func TestH1H1ReqPhaseReturnCONNECTMethod(t *testing.T) {
	opts := options{
		args: []string{"--mruby-file=" + testDir + "/req-return.rb"},
		handler: func(http.ResponseWriter, *http.Request) {
			t.Fatalf("request should not be forwarded")
		},
	}

	st := newServerTester(t, opts)
	defer st.Close()

	if _, err := io.WriteString(st.conn, "CONNECT 127.0.0.1:443 HTTP/1.1\r\nTest-Case: TestH1H1ReqPhaseReturnCONNECTMethod\r\nHost: 127.0.0.1:443\r\n\r\n"); err != nil {
		t.Fatalf("Error io.WriteString() = %v", err)
	}

	resp, err := http.ReadResponse(bufio.NewReader(st.conn), nil)
	if err != nil {
		t.Fatalf("Error http.ReadResponse() = %v", err)
	}

	defer resp.Body.Close()

	if got, want := resp.StatusCode, http.StatusNotFound; got != want {
		t.Errorf("status: %v; want %v", got, want)
	}

	hdCheck := func() {
		hdtests := []struct {
			k, v string
		}{
			{"content-length", "20"},
			{"from", "mruby"},
		}

		for _, tt := range hdtests {
			if got, want := resp.Header.Get(tt.k), tt.v; got != want {
				t.Errorf("%v = %v; want %v", tt.k, got, want)
			}
		}

		if _, err := io.ReadAll(resp.Body); err != nil {
			t.Fatalf("Error io.ReadAll() = %v", err)
		}
	}

	hdCheck()

	if _, err := io.WriteString(st.conn, "CONNECT 127.0.0.1:443 HTTP/1.1\r\nTest-Case: TestH1H1ReqPhaseReturnCONNECTMethod\r\nHost: 127.0.0.1:443\r\n\r\n"); err != nil {
		t.Fatalf("Error io.WriteString() = %v", err)
	}

	resp, err = http.ReadResponse(bufio.NewReader(st.conn), nil)
	if err != nil {
		t.Fatalf("Error http.ReadResponse() = %v", err)
	}

	defer resp.Body.Close()

	if got, want := resp.StatusCode, http.StatusNotFound; got != want {
		t.Errorf("status: %v; want %v", got, want)
	}

	hdCheck()

	if _, err := io.ReadAll(resp.Body); err != nil {
		t.Fatalf("Error io.ReadAll() = %v", err)
	}
}

// TestH1H1RespPhaseSetHeader tests mruby response phase hook modifies
// response header fields.
func TestH1H1RespPhaseSetHeader(t *testing.T) {
	opts := options{
		args: []string{"--mruby-file=" + testDir + "/resp-set-header.rb"},
	}

	st := newServerTester(t, opts)
	defer st.Close()

	res, err := st.http1(requestParam{
		name: "TestH1H1RespPhaseSetHeader",
	})
	if err != nil {
		t.Fatalf("Error st.http1() = %v", err)
	}

	if got, want := res.status, http.StatusOK; got != want {
		t.Errorf("status = %v; want %v", got, want)
	}

	if got, want := res.header.Get("alpha"), "bravo"; got != want {
		t.Errorf("alpha = %v; want %v", got, want)
	}
}

// TestH1H1RespPhaseReturn tests mruby response phase hook returns
// custom response.
func TestH1H1RespPhaseReturn(t *testing.T) {
	opts := options{
		args: []string{"--mruby-file=" + testDir + "/resp-return.rb"},
	}

	st := newServerTester(t, opts)
	defer st.Close()

	res, err := st.http1(requestParam{
		name: "TestH1H1RespPhaseReturn",
	})
	if err != nil {
		t.Fatalf("Error st.http1() = %v", err)
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

// TestH1H1HTTPSRedirect tests that the request to the backend which
// requires TLS is redirected to https URI.
func TestH1H1HTTPSRedirect(t *testing.T) {
	opts := options{
		args: []string{"--redirect-if-not-tls"},
	}

	st := newServerTester(t, opts)
	defer st.Close()

	res, err := st.http1(requestParam{
		name: "TestH1H1HTTPSRedirect",
	})
	if err != nil {
		t.Fatalf("Error st.http1() = %v", err)
	}

	if got, want := res.status, http.StatusPermanentRedirect; got != want {
		t.Errorf("status = %v; want %v", got, want)
	}

	if got, want := res.header.Get("location"), "https://127.0.0.1/"; got != want {
		t.Errorf("location: %v; want %v", got, want)
	}
}

// TestH1H1HTTPSRedirectPort tests that the request to the backend
// which requires TLS is redirected to https URI with given port.
func TestH1H1HTTPSRedirectPort(t *testing.T) {
	opts := options{
		args: []string{
			"--redirect-if-not-tls",
			"--redirect-https-port=8443",
		},
	}

	st := newServerTester(t, opts)
	defer st.Close()

	res, err := st.http1(requestParam{
		path: "/foo?bar",
		name: "TestH1H1HTTPSRedirectPort",
	})
	if err != nil {
		t.Fatalf("Error st.http1() = %v", err)
	}

	if got, want := res.status, http.StatusPermanentRedirect; got != want {
		t.Errorf("status = %v; want %v", got, want)
	}

	if got, want := res.header.Get("location"), "https://127.0.0.1:8443/foo?bar"; got != want {
		t.Errorf("location: %v; want %v", got, want)
	}
}

// TestH1H1POSTRequests tests that server can handle 2 requests with
// request body.
func TestH1H1POSTRequests(t *testing.T) {
	st := newServerTester(t, options{})
	defer st.Close()

	res, err := st.http1(requestParam{
		name: "TestH1H1POSTRequestsNo1",
		body: make([]byte, 1),
	})
	if err != nil {
		t.Fatalf("Error st.http1() = %v", err)
	}

	if got, want := res.status, http.StatusOK; got != want {
		t.Errorf("res.status: %v; want %v", got, want)
	}

	res, err = st.http1(requestParam{
		name: "TestH1H1POSTRequestsNo2",
		body: make([]byte, 65536),
	})
	if err != nil {
		t.Fatalf("Error st.http1() = %v", err)
	}

	if got, want := res.status, http.StatusOK; got != want {
		t.Errorf("res.status: %v; want %v", got, want)
	}
}

// TestH1H1CONNECTMethodFailure tests that CONNECT method failure
// resets llhttp HPE_PAUSED_UPGRADE.
func TestH1H1CONNECTMethodFailure(t *testing.T) {
	opts := options{
		handler: func(w http.ResponseWriter, r *http.Request) {
			if r.Header.Get("required-header") == "" {
				w.WriteHeader(http.StatusNotFound)
			}
		},
	}

	st := newServerTester(t, opts)
	defer st.Close()

	if _, err := io.WriteString(st.conn, "CONNECT 127.0.0.1:443 HTTP/1.1\r\nTest-Case: TestH1H1CONNECTMethodFailure\r\nHost: 127.0.0.1:443\r\n\r\n"); err != nil {
		t.Fatalf("Error io.WriteString() = %v", err)
	}

	resp, err := http.ReadResponse(bufio.NewReader(st.conn), nil)
	if err != nil {
		t.Fatalf("Error http.ReadResponse() = %v", err)
	}

	defer resp.Body.Close()

	if got, want := resp.StatusCode, http.StatusNotFound; got != want {
		t.Errorf("status: %v; want %v", got, want)
	}

	if _, err := io.ReadAll(resp.Body); err != nil {
		t.Fatalf("Error io.ReadAll() = %v", err)
	}

	if _, err := io.WriteString(st.conn, "CONNECT 127.0.0.1:443 HTTP/1.1\r\nTest-Case: TestH1H1CONNECTMethodFailure\r\nHost: 127.0.0.1:443\r\nrequired-header: foo\r\n\r\n"); err != nil {
		t.Fatalf("Error io.WriteString() = %v", err)
	}

	resp, err = http.ReadResponse(bufio.NewReader(st.conn), nil)
	if err != nil {
		t.Fatalf("Error http.ReadResponse() = %v", err)
	}

	defer resp.Body.Close()

	if got, want := resp.StatusCode, http.StatusOK; got != want {
		t.Errorf("status: %v; want %v", got, want)
	}
}

// // TestH1H2ConnectFailure tests that server handles the situation that
// // connection attempt to HTTP/2 backend failed.
// func TestH1H2ConnectFailure(t *testing.T) {
// 	opts := options{
// 		args: []string{"--http2-bridge"},
// 	}
// 	st := newServerTester(t, opts)
// 	defer st.Close()

// 	// simulate backend connect attempt failure
// 	st.ts.Close()

// 	res, err := st.http1(requestParam{
// 		name: "TestH1H2ConnectFailure",
// 	})
// 	if err != nil {
// 		t.Fatalf("Error st.http1() = %v", err)
// 	}
// 	want := 503
// 	if got := res.status; got != want {
// 		t.Errorf("status: %v; want %v", got, want)
// 	}
// }

// TestH1H2NoHost tests that server rejects request without Host
// header field for HTTP/2 backend.
func TestH1H2NoHost(t *testing.T) {
	opts := options{
		args: []string{"--http2-bridge"},
		handler: func(http.ResponseWriter, *http.Request) {
			t.Errorf("server should not forward bad request")
		},
	}

	st := newServerTester(t, opts)
	defer st.Close()

	// without Host header field, we expect 400 response
	if _, err := io.WriteString(st.conn, "GET / HTTP/1.1\r\nTest-Case: TestH1H2NoHost\r\n\r\n"); err != nil {
		t.Fatalf("Error io.WriteString() = %v", err)
	}

	resp, err := http.ReadResponse(bufio.NewReader(st.conn), nil)
	if err != nil {
		t.Fatalf("Error http.ReadResponse() = %v", err)
	}

	defer resp.Body.Close()

	if got, want := resp.StatusCode, http.StatusBadRequest; got != want {
		t.Errorf("status: %v; want %v", got, want)
	}
}

// TestH1H2HTTP10 tests that server can accept HTTP/1.0 request
// without Host header field
func TestH1H2HTTP10(t *testing.T) {
	opts := options{
		args: []string{"--http2-bridge"},
		handler: func(w http.ResponseWriter, r *http.Request) {
			w.Header().Add("request-host", r.Host)
		},
	}

	st := newServerTester(t, opts)
	defer st.Close()

	if _, err := io.WriteString(st.conn, "GET / HTTP/1.0\r\nTest-Case: TestH1H2HTTP10\r\n\r\n"); err != nil {
		t.Fatalf("Error io.WriteString() = %v", err)
	}

	resp, err := http.ReadResponse(bufio.NewReader(st.conn), nil)
	if err != nil {
		t.Fatalf("Error http.ReadResponse() = %v", err)
	}

	defer resp.Body.Close()

	if got, want := resp.StatusCode, http.StatusOK; got != want {
		t.Errorf("status: %v; want %v", got, want)
	}

	if got, want := resp.Header.Get("request-host"), st.backendHost; got != want {
		t.Errorf("request-host: %v; want %v", got, want)
	}
}

// TestH1H2HTTP10NoHostRewrite tests that server generates host header
// field using actual backend server even if --no-http-rewrite is
// used.
func TestH1H2HTTP10NoHostRewrite(t *testing.T) {
	opts := options{
		args: []string{"--http2-bridge"},
		handler: func(w http.ResponseWriter, r *http.Request) {
			w.Header().Add("request-host", r.Host)
		},
	}

	st := newServerTester(t, opts)
	defer st.Close()

	if _, err := io.WriteString(st.conn, "GET / HTTP/1.0\r\nTest-Case: TestH1H2HTTP10NoHostRewrite\r\n\r\n"); err != nil {
		t.Fatalf("Error io.WriteString() = %v", err)
	}

	resp, err := http.ReadResponse(bufio.NewReader(st.conn), nil)
	if err != nil {
		t.Fatalf("Error http.ReadResponse() = %v", err)
	}

	defer resp.Body.Close()

	if got, want := resp.StatusCode, http.StatusOK; got != want {
		t.Errorf("status: %v; want %v", got, want)
	}

	if got, want := resp.Header.Get("request-host"), st.backendHost; got != want {
		t.Errorf("request-host: %v; want %v", got, want)
	}
}

// TestH1H2CrumbleCookie tests that Cookies are crumbled and assembled
// when forwarding to HTTP/2 backend link.  go-nghttp2 server
// concatenates crumbled Cookies automatically, so this test is not
// much effective now.
func TestH1H2CrumbleCookie(t *testing.T) {
	opts := options{
		args: []string{"--http2-bridge"},
		handler: func(_ http.ResponseWriter, r *http.Request) {
			if got, want := r.Header.Get("Cookie"), "alpha; bravo; charlie"; got != want {
				t.Errorf("Cookie: %v; want %v", got, want)
			}
		},
	}

	st := newServerTester(t, opts)
	defer st.Close()

	res, err := st.http1(requestParam{
		name: "TestH1H2CrumbleCookie",
		header: []hpack.HeaderField{
			pair("Cookie", "alpha; bravo; charlie"),
		},
	})
	if err != nil {
		t.Fatalf("Error st.http1() = %v", err)
	}

	if got, want := res.status, http.StatusOK; got != want {
		t.Errorf("status: %v; want %v", got, want)
	}
}

// TestH1H2GenerateVia tests that server generates Via header field to and
// from backend server.
func TestH1H2GenerateVia(t *testing.T) {
	opts := options{
		args: []string{"--http2-bridge"},
		handler: func(_ http.ResponseWriter, r *http.Request) {
			if got, want := r.Header.Get("Via"), "1.1 nghttpx"; got != want {
				t.Errorf("Via: %v; want %v", got, want)
			}
		},
	}

	st := newServerTester(t, opts)
	defer st.Close()

	res, err := st.http1(requestParam{
		name: "TestH1H2GenerateVia",
	})
	if err != nil {
		t.Fatalf("Error st.http1() = %v", err)
	}

	if got, want := res.header.Get("Via"), "2 nghttpx"; got != want {
		t.Errorf("Via: %v; want %v", got, want)
	}
}

// TestH1H2AppendVia tests that server adds value to existing Via
// header field to and from backend server.
func TestH1H2AppendVia(t *testing.T) {
	opts := options{
		args: []string{"--http2-bridge"},
		handler: func(w http.ResponseWriter, r *http.Request) {
			if got, want := r.Header.Get("Via"), "foo, 1.1 nghttpx"; got != want {
				t.Errorf("Via: %v; want %v", got, want)
			}
			w.Header().Add("Via", "bar")
		},
	}

	st := newServerTester(t, opts)
	defer st.Close()

	res, err := st.http1(requestParam{
		name: "TestH1H2AppendVia",
		header: []hpack.HeaderField{
			pair("via", "foo"),
		},
	})
	if err != nil {
		t.Fatalf("Error st.http1() = %v", err)
	}

	if got, want := res.header.Get("Via"), "bar, 2 nghttpx"; got != want {
		t.Errorf("Via: %v; want %v", got, want)
	}
}

// TestH1H2NoVia tests that server does not add value to existing Via
// header field to and from backend server.
func TestH1H2NoVia(t *testing.T) {
	opts := options{
		args: []string{"--http2-bridge", "--no-via"},
		handler: func(w http.ResponseWriter, r *http.Request) {
			if got, want := r.Header.Get("Via"), "foo"; got != want {
				t.Errorf("Via: %v; want %v", got, want)
			}
			w.Header().Add("Via", "bar")
		},
	}

	st := newServerTester(t, opts)
	defer st.Close()

	res, err := st.http1(requestParam{
		name: "TestH1H2NoVia",
		header: []hpack.HeaderField{
			pair("via", "foo"),
		},
	})
	if err != nil {
		t.Fatalf("Error st.http1() = %v", err)
	}

	if got, want := res.header.Get("Via"), "bar"; got != want {
		t.Errorf("Via: %v; want %v", got, want)
	}
}

// TestH1H2ReqPhaseReturn tests mruby request phase hook returns
// custom response.
func TestH1H2ReqPhaseReturn(t *testing.T) {
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

	res, err := st.http1(requestParam{
		name: "TestH1H2ReqPhaseReturn",
	})
	if err != nil {
		t.Fatalf("Error st.http1() = %v", err)
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

// TestH1H2RespPhaseReturn tests mruby response phase hook returns
// custom response.
func TestH1H2RespPhaseReturn(t *testing.T) {
	opts := options{
		args: []string{
			"--http2-bridge",
			"--mruby-file=" + testDir + "/resp-return.rb",
		},
	}

	st := newServerTester(t, opts)
	defer st.Close()

	res, err := st.http1(requestParam{
		name: "TestH1H2RespPhaseReturn",
	})
	if err != nil {
		t.Fatalf("Error st.http1() = %v", err)
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

// TestH1H2TE tests that "te: trailers" header is forwarded to HTTP/2
// backend server by stripping other encodings.
func TestH1H2TE(t *testing.T) {
	opts := options{
		args: []string{"--http2-bridge"},
		handler: func(_ http.ResponseWriter, r *http.Request) {
			if got, want := r.Header.Get("te"), "trailers"; got != want {
				t.Errorf("te: %v; want %v", got, want)
			}
		},
	}

	st := newServerTester(t, opts)
	defer st.Close()

	res, err := st.http1(requestParam{
		name: "TestH1H2TE",
		header: []hpack.HeaderField{
			pair("te", "foo,trailers,bar"),
		},
	})
	if err != nil {
		t.Fatalf("Error st.http1() = %v", err)
	}

	if got, want := res.status, http.StatusOK; got != want {
		t.Errorf("status: %v; want %v", got, want)
	}
}

// TestH1APIBackendconfig exercise backendconfig API endpoint routine
// for successful case.
func TestH1APIBackendconfig(t *testing.T) {
	opts := options{
		args: []string{"-f127.0.0.1,3010;api;no-tls"},
		handler: func(http.ResponseWriter, *http.Request) {
			t.Fatalf("request should not be forwarded")
		},
		connectPort: 3010,
	}

	st := newServerTester(t, opts)
	defer st.Close()

	res, err := st.http1(requestParam{
		name:   "TestH1APIBackendconfig",
		path:   "/api/v1beta1/backendconfig",
		method: "PUT",
		body: []byte(`# comment
backend=127.0.0.1,3011

`),
	})
	if err != nil {
		t.Fatalf("Error st.http1() = %v", err)
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

// TestH1APIBackendconfigQuery exercise backendconfig API endpoint
// routine with query.
func TestH1APIBackendconfigQuery(t *testing.T) {
	opts := options{
		args: []string{"-f127.0.0.1,3010;api;no-tls"},
		handler: func(http.ResponseWriter, *http.Request) {
			t.Fatalf("request should not be forwarded")
		},
		connectPort: 3010,
	}

	st := newServerTester(t, opts)
	defer st.Close()

	res, err := st.http1(requestParam{
		name:   "TestH1APIBackendconfigQuery",
		path:   "/api/v1beta1/backendconfig?foo=bar",
		method: "PUT",
		body: []byte(`# comment
backend=127.0.0.1,3011

`),
	})
	if err != nil {
		t.Fatalf("Error st.http1() = %v", err)
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

// TestH1APIBackendconfigBadMethod exercise backendconfig API endpoint
// routine with bad method.
func TestH1APIBackendconfigBadMethod(t *testing.T) {
	opts := options{
		args: []string{"-f127.0.0.1,3010;api;no-tls"},
		handler: func(http.ResponseWriter, *http.Request) {
			t.Fatalf("request should not be forwarded")
		},
		connectPort: 3010,
	}

	st := newServerTester(t, opts)
	defer st.Close()

	res, err := st.http1(requestParam{
		name:   "TestH1APIBackendconfigBadMethod",
		path:   "/api/v1beta1/backendconfig",
		method: "GET",
		body: []byte(`# comment
backend=127.0.0.1,3011

`),
	})
	if err != nil {
		t.Fatalf("Error st.http1() = %v", err)
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

// TestH1APIConfigrevision tests configrevision API.
func TestH1APIConfigrevision(t *testing.T) {
	opts := options{
		args: []string{"-f127.0.0.1,3010;api;no-tls"},
		handler: func(http.ResponseWriter, *http.Request) {
			t.Fatalf("request should not be forwarded")
		},
		connectPort: 3010,
	}

	st := newServerTester(t, opts)
	defer st.Close()

	res, err := st.http1(requestParam{
		name:   "TestH1APIConfigrevision",
		path:   "/api/v1beta1/configrevision",
		method: "GET",
	})
	if err != nil {
		t.Fatalf("Error st.http1() = %v", err)
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

// TestH1APINotFound exercise backendconfig API endpoint routine when
// API endpoint is not found.
func TestH1APINotFound(t *testing.T) {
	opts := options{
		args: []string{"-f127.0.0.1,3010;api;no-tls"},
		handler: func(http.ResponseWriter, *http.Request) {
			t.Fatalf("request should not be forwarded")
		},
		connectPort: 3010,
	}

	st := newServerTester(t, opts)
	defer st.Close()

	res, err := st.http1(requestParam{
		name:   "TestH1APINotFound",
		path:   "/api/notfound",
		method: "GET",
		body: []byte(`# comment
backend=127.0.0.1,3011

`),
	})
	if err != nil {
		t.Fatalf("Error st.http1() = %v", err)
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

// TestH1Healthmon tests health monitor endpoint.
func TestH1Healthmon(t *testing.T) {
	opts := options{
		args: []string{"-f127.0.0.1,3011;healthmon;no-tls"},
		handler: func(http.ResponseWriter, *http.Request) {
			t.Fatalf("request should not be forwarded")
		},
		connectPort: 3011,
	}

	st := newServerTester(t, opts)
	defer st.Close()

	res, err := st.http1(requestParam{
		name: "TestH1Healthmon",
		path: "/alpha/bravo",
	})
	if err != nil {
		t.Fatalf("Error st.http1() = %v", err)
	}

	if got, want := res.status, http.StatusOK; got != want {
		t.Errorf("res.status: %v; want %v", got, want)
	}
}

// TestH1ResponseBeforeRequestEnd tests the situation where response
// ends before request body finishes.
func TestH1ResponseBeforeRequestEnd(t *testing.T) {
	opts := options{
		args: []string{"--mruby-file=" + testDir + "/req-return.rb"},
		handler: func(http.ResponseWriter, *http.Request) {
			t.Fatal("request should not be forwarded")
		},
	}

	st := newServerTester(t, opts)
	defer st.Close()

	if _, err := io.WriteString(st.conn, fmt.Sprintf("POST / HTTP/1.1\r\nHost: %v\r\nTest-Case: TestH1ResponseBeforeRequestEnd\r\nContent-Length: 1000000\r\n\r\n",
		st.authority)); err != nil {
		t.Fatalf("Error io.WriteString() = %v", err)
	}

	resp, err := http.ReadResponse(bufio.NewReader(st.conn), nil)
	if err != nil {
		t.Fatalf("Error http.ReadResponse() = %v", err)
	}

	defer resp.Body.Close()

	if got, want := resp.StatusCode, http.StatusNotFound; got != want {
		t.Errorf("status: %v; want %v", got, want)
	}
}

// TestH1H1ChunkedEndsPrematurely tests that an HTTP/1.1 request fails
// if the backend chunked encoded response ends prematurely.
func TestH1H1ChunkedEndsPrematurely(t *testing.T) {
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

	_, err := st.http1(requestParam{
		name: "TestH1H1ChunkedEndsPrematurely",
	})
	if err == nil {
		t.Fatal("st.http1() should fail")
	}
}

// TestH1H1RequestMalformedTransferEncoding tests that server rejects
// request which contains malformed transfer-encoding.
func TestH1H1RequestMalformedTransferEncoding(t *testing.T) {
	opts := options{
		handler: func(http.ResponseWriter, *http.Request) {
			t.Errorf("server should not forward bad request")
		},
	}

	st := newServerTester(t, opts)
	defer st.Close()

	if _, err := io.WriteString(st.conn, fmt.Sprintf("GET / HTTP/1.1\r\nHost: %v\r\nTest-Case: TestH1H1RequestMalformedTransferEncoding\r\nTransfer-Encoding: ,chunked\r\n\r\n",
		st.authority)); err != nil {
		t.Fatalf("Error io.WriteString() = %v", err)
	}

	resp, err := http.ReadResponse(bufio.NewReader(st.conn), nil)
	if err != nil {
		t.Fatalf("Error http.ReadResponse() = %v", err)
	}

	defer resp.Body.Close()

	if got, want := resp.StatusCode, http.StatusBadRequest; got != want {
		t.Errorf("status: %v; want %v", got, want)
	}
}

// TestH1H1ResponseMalformedTransferEncoding tests a request fails if
// its response contains malformed transfer-encoding.
func TestH1H1ResponseMalformedTransferEncoding(t *testing.T) {
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
			if _, err := bufrw.WriteString("HTTP/1.1 200\r\nTransfer-Encoding: ,chunked\r\n\r\n"); err != nil {
				t.Fatalf("Error bufrw.WriteString() = %v", err)
			}
			bufrw.Flush()
		},
	}

	st := newServerTester(t, opts)
	defer st.Close()

	res, err := st.http1(requestParam{
		name: "TestH1H1ResponseMalformedTransferEncoding",
	})
	if err != nil {
		t.Fatalf("Error st.http1() = %v", err)
	}

	if got, want := res.status, http.StatusBadGateway; got != want {
		t.Errorf("res.status: %v; want %v", got, want)
	}
}

// TestH1H1ResponseUnknownTransferEncoding tests a request succeeds if
// its response contains unknown transfer-encoding.
func TestH1H1ResponseUnknownTransferEncoding(t *testing.T) {
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
			if _, err := bufrw.WriteString("HTTP/1.1 200\r\nTransfer-Encoding: foo\r\n\r\n"); err != nil {
				t.Fatalf("Error bufrw.WriteString() = %v", err)
			}
			bufrw.Flush()
		},
	}

	st := newServerTester(t, opts)
	defer st.Close()

	if _, err := io.WriteString(st.conn, fmt.Sprintf("GET / HTTP/1.1\r\nHost: %v\r\nTest-Case: TestH1H1ResponseUnknownTransferEncoding\r\n\r\n",
		st.authority)); err != nil {
		t.Fatalf("Error: io.WriteString() = %v", err)
	}

	r := bufio.NewReader(st.conn)

	resp := make([]byte, 4096)

	resplen, err := r.Read(resp)
	if err != nil {
		t.Fatalf("Error: r.Read() = %v", err)
	}

	resp = resp[:resplen]

	const expect = "HTTP/1.1 200 OK\r\nTransfer-Encoding: foo\r\nConnection: close\r\nServer: nghttpx\r\nVia: 1.1 nghttpx\r\n\r\n"

	if got, want := string(resp), expect; got != want {
		t.Errorf("resp = %v, want %v", got, want)
	}
}

// TestH1H1RequestHTTP10TransferEncoding tests that server rejects
// HTTP/1.0 request which contains transfer-encoding.
func TestH1H1RequestHTTP10TransferEncoding(t *testing.T) {
	opts := options{
		handler: func(http.ResponseWriter, *http.Request) {
			t.Errorf("server should not forward bad request")
		},
	}

	st := newServerTester(t, opts)
	defer st.Close()

	if _, err := io.WriteString(st.conn, "GET / HTTP/1.0\r\nTest-Case: TestH1H1RequestHTTP10TransferEncoding\r\nTransfer-Encoding: chunked\r\n\r\n"); err != nil {
		t.Fatalf("Error io.WriteString() = %v", err)
	}

	resp, err := http.ReadResponse(bufio.NewReader(st.conn), nil)
	if err != nil {
		t.Fatalf("Error http.ReadResponse() = %v", err)
	}

	defer resp.Body.Close()

	if got, want := resp.StatusCode, http.StatusBadRequest; got != want {
		t.Errorf("status: %v; want %v", got, want)
	}
}
