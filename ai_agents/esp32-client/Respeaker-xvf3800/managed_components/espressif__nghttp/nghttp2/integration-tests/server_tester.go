package nghttp2

import (
	"bufio"
	"bytes"
	"cmp"
	"context"
	"crypto/tls"
	"encoding/binary"
	"errors"
	"fmt"
	"io"
	"net"
	"net/http"
	"net/http/httptest"
	"net/url"
	"os"
	"os/exec"
	"slices"
	"strconv"
	"strings"
	"syscall"
	"testing"
	"time"

	"github.com/tatsuhiro-t/go-nghttp2"
	"golang.org/x/net/http2"
	"golang.org/x/net/http2/hpack"
	"golang.org/x/net/websocket"
)

const (
	serverBin  = buildDir + "/src/nghttpx"
	serverPort = 3009
	testDir    = sourceDir + "/integration-tests"
	logDir     = buildDir + "/integration-tests"
)

func pair(name, value string) hpack.HeaderField {
	return hpack.HeaderField{
		Name:  name,
		Value: value,
	}
}

type serverTester struct {
	cmd           *exec.Cmd // test frontend server process, which is test subject
	url           string    // test frontend server URL
	t             *testing.T
	ts            *httptest.Server // backend server
	frontendHost  string           // frontend server host
	backendHost   string           // backend server host
	conn          net.Conn         // connection to frontend server
	h2PrefaceSent bool             // HTTP/2 preface was sent in conn
	nextStreamID  uint32           // next stream ID
	fr            *http2.Framer    // HTTP/2 framer
	headerBlkBuf  bytes.Buffer     // buffer to store encoded header block
	enc           *hpack.Encoder   // HTTP/2 HPACK encoder
	header        http.Header      // received header fields
	dec           *hpack.Decoder   // HTTP/2 HPACK decoder
	authority     string           // server's host:port
	frCh          chan http2.Frame // used for incoming HTTP/2 frame
	errCh         chan error
}

type options struct {
	// args is the additional arguments to nghttpx.
	args []string
	// handler is the handler to handle the request.  It defaults
	// to noopHandler.
	handler http.HandlerFunc
	// connectPort is the server side port where client connection
	// is made.  It defaults to serverPort.
	connectPort int
	// tls, if set to true, sets up TLS frontend connection.
	tls bool
	// tlsConfig is the client side TLS configuration that is used
	// when tls is true.
	tlsConfig *tls.Config
	// tcpData is additional data that are written to connection
	// before TLS handshake starts.  This field is ignored if tls
	// is false.
	tcpData []byte
	// quic, if set to true, sets up QUIC frontend connection.
	// quic implies tls = true.
	quic bool
}

// newServerTester creates test context.
func newServerTester(t *testing.T, opts options) *serverTester {
	if opts.quic {
		opts.tls = true
	}

	if opts.handler == nil {
		opts.handler = noopHandler
	}

	if opts.connectPort == 0 {
		opts.connectPort = serverPort
	}

	ts := httptest.NewUnstartedServer(opts.handler)

	var (
		args                                                                                        []string
		backendTLS, dns, externalDNS, acceptProxyProtocol, redirectIfNotTLS, affinityCookie, alpnH1 bool
	)

	for _, k := range opts.args {
		switch k {
		case "--http2-bridge":
			backendTLS = true
		case "--dns":
			dns = true
		case "--external-dns":
			dns = true
			externalDNS = true
		case "--accept-proxy-protocol":
			acceptProxyProtocol = true
		case "--redirect-if-not-tls":
			redirectIfNotTLS = true
		case "--affinity-cookie":
			affinityCookie = true
		case "--alpn-h1":
			alpnH1 = true
		default:
			args = append(args, k)
		}
	}

	if backendTLS {
		nghttp2.ConfigureServer(ts.Config, &nghttp2.Server{})
		// According to httptest/server.go, we have to set
		// NextProtos separately for ts.TLS.  NextProtos set
		// in nghttp2.ConfigureServer is effectively ignored.
		ts.TLS = new(tls.Config)
		ts.TLS.NextProtos = append(ts.TLS.NextProtos, "h2")
		ts.StartTLS()

		args = append(args, "-k")
	} else {
		ts.Start()
	}

	scheme := "http"

	if opts.tls {
		scheme = "https"

		args = append(args, testDir+"/server.key", testDir+"/server.crt")
	}

	backendURL, err := url.Parse(ts.URL)
	if err != nil {
		t.Fatalf("Error parsing URL from httptest.Server: %v", err)
	}

	// URL.Host looks like "127.0.0.1:8080", but we want
	// "127.0.0.1,8080"
	b := "-b"

	if !externalDNS {
		b += fmt.Sprintf("%v;", strings.Replace(backendURL.Host, ":", ",", -1))
	} else {
		sep := strings.LastIndex(backendURL.Host, ":")
		if sep == -1 {
			t.Fatalf("backendURL.Host %v does not contain separator ':'", backendURL.Host)
		}

		// We use awesome service nip.io.
		b += fmt.Sprintf("%v.nip.io,%v;", backendURL.Host[:sep], backendURL.Host[sep+1:])
	}

	if backendTLS {
		b += ";proto=h2;tls"
	}

	if dns {
		b += ";dns"
	}

	if redirectIfNotTLS {
		b += ";redirect-if-not-tls"
	}

	if affinityCookie {
		b += ";affinity=cookie;affinity-cookie-name=affinity;affinity-cookie-path=/foo/bar"
	}

	noTLS := ";no-tls"

	if opts.tls {
		noTLS = ""
	}

	var proxyProto string

	if acceptProxyProtocol {
		proxyProto = ";proxyproto"
	}

	args = append(args, fmt.Sprintf("-f127.0.0.1,%v%v%v", serverPort, noTLS, proxyProto), b,
		"--errorlog-file="+logDir+"/log.txt", "-LINFO")

	if opts.quic {
		args = append(args,
			fmt.Sprintf("-f127.0.0.1,%v;quic", serverPort),
			"--no-quic-bpf")
	}

	authority := fmt.Sprintf("127.0.0.1:%v", opts.connectPort)

	st := &serverTester{
		cmd:          exec.Command(serverBin, args...),
		t:            t,
		ts:           ts,
		url:          fmt.Sprintf("%v://%v", scheme, authority),
		frontendHost: fmt.Sprintf("127.0.0.1:%v", serverPort),
		backendHost:  backendURL.Host,
		nextStreamID: 1,
		authority:    authority,
		frCh:         make(chan http2.Frame),
		errCh:        make(chan error),
	}

	st.cmd.Stdout = os.Stdout
	st.cmd.Stderr = os.Stderr

	if err := st.cmd.Start(); err != nil {
		st.t.Fatalf("Error starting %v: %v", serverBin, err)
	}

	retry := 0

	for {
		time.Sleep(50 * time.Millisecond)

		conn, err := net.Dial("tcp", authority)
		if err == nil && opts.tls {
			if len(opts.tcpData) > 0 {
				if _, err := conn.Write(opts.tcpData); err != nil {
					st.Close()
					st.t.Fatal("Error writing TCP data")
				}
			}

			var tlsConfig *tls.Config

			if opts.tlsConfig == nil {
				tlsConfig = new(tls.Config)
			} else {
				tlsConfig = opts.tlsConfig.Clone()
			}

			tlsConfig.InsecureSkipVerify = true

			if alpnH1 {
				tlsConfig.NextProtos = []string{"http/1.1"}
			} else {
				tlsConfig.NextProtos = []string{"h2"}
			}

			tlsConn := tls.Client(conn, tlsConfig)

			err = tlsConn.Handshake()
			if err == nil {
				conn = tlsConn
			}
		}

		if err != nil {
			retry++
			if retry >= 100 {
				st.Close()
				st.t.Fatalf("Error server is not responding too long; server command-line arguments may be invalid")
			}

			continue
		}

		st.conn = conn

		break
	}

	st.fr = http2.NewFramer(st.conn, st.conn)
	st.enc = hpack.NewEncoder(&st.headerBlkBuf)
	st.dec = hpack.NewDecoder(4096, func(f hpack.HeaderField) {
		st.header.Add(f.Name, f.Value)
	})

	return st
}

func (st *serverTester) Close() {
	if st.conn != nil {
		st.conn.Close()
	}

	if st.cmd != nil {
		done := make(chan struct{})

		go func() {
			if err := st.cmd.Wait(); err != nil {
				st.t.Errorf("Error st.cmd.Wait() = %v", err)
			}

			close(done)
		}()

		if err := st.cmd.Process.Signal(syscall.SIGQUIT); err != nil {
			st.t.Errorf("Error st.cmd.Process.Signal() = %v", err)
		}

		select {
		case <-done:
		case <-time.After(10 * time.Second):
			if err := st.cmd.Process.Kill(); err != nil {
				st.t.Errorf("Error st.cmd.Process.Kill() = %v", err)
			}

			<-done
		}
	}

	if st.ts != nil {
		st.ts.Close()
	}
}

func (st *serverTester) readFrame() (http2.Frame, error) {
	go func() {
		f, err := st.fr.ReadFrame()
		if err != nil {
			st.errCh <- err
			return
		}

		st.frCh <- f
	}()

	select {
	case f := <-st.frCh:
		return f, nil
	case err := <-st.errCh:
		return nil, err
	case <-time.After(5 * time.Second):
		return nil, errors.New("timeout waiting for frame")
	}
}

type requestParam struct {
	name        string              // name for this request to identify the request in log easily
	streamID    uint32              // stream ID, automatically assigned if 0
	method      string              // method, defaults to GET
	scheme      string              // scheme, defaults to http
	authority   string              // authority, defaults to backend server address
	path        string              // path, defaults to /
	header      []hpack.HeaderField // additional request header fields
	body        []byte              // request body
	trailer     []hpack.HeaderField // trailer part
	httpUpgrade bool                // true if upgraded to HTTP/2 through HTTP Upgrade
	noEndStream bool                // true if END_STREAM should not be sent
}

// wrapper for request body to set trailer part
type chunkedBodyReader struct {
	trailer        []hpack.HeaderField
	trailerWritten bool
	body           io.Reader
	req            *http.Request
}

func (cbr *chunkedBodyReader) Read(p []byte) (n int, err error) {
	// document says that we have to set http.Request.Trailer
	// after request was sent and before body returns EOF.
	if !cbr.trailerWritten {
		cbr.trailerWritten = true

		for _, h := range cbr.trailer {
			cbr.req.Trailer.Set(h.Name, h.Value)
		}
	}

	return cbr.body.Read(p)
}

func (st *serverTester) websocket(rp requestParam) *serverResponse {
	urlstring := st.url + "/echo"

	config, err := websocket.NewConfig(urlstring, st.url)
	if err != nil {
		st.t.Fatalf("websocket.NewConfig(%q, %q) returned error: %v", urlstring, st.url, err)
	}

	config.Header.Add("Test-Case", rp.name)

	for _, h := range rp.header {
		config.Header.Add(h.Name, h.Value)
	}

	ws, err := websocket.NewClient(config, st.conn)
	if err != nil {
		st.t.Fatalf("Error creating websocket client: %v", err)
	}

	if _, err := ws.Write(rp.body); err != nil {
		st.t.Fatalf("ws.Write() returned error: %v", err)
	}

	msg := make([]byte, 1024)

	var n int

	if n, err = ws.Read(msg); err != nil {
		st.t.Fatalf("ws.Read() returned error: %v", err)
	}

	res := &serverResponse{
		body: msg[:n],
	}

	return res
}

func (st *serverTester) http1(rp requestParam) (*serverResponse, error) {
	method := "GET"

	if rp.method != "" {
		method = rp.method
	}

	var (
		body io.Reader
		cbr  *chunkedBodyReader
	)

	if rp.body != nil {
		body = bytes.NewBuffer(rp.body)

		if len(rp.trailer) != 0 {
			cbr = &chunkedBodyReader{
				trailer: rp.trailer,
				body:    body,
			}

			body = cbr
		}
	}

	reqURL := st.url

	if rp.path != "" {
		u, err := url.Parse(st.url)
		if err != nil {
			st.t.Fatalf("Error parsing URL from st.url %v: %v", st.url, err)
		}

		u.Path = ""
		u.RawQuery = ""
		reqURL = u.String() + rp.path
	}

	ctx, cancel := context.WithTimeout(context.Background(), 30*time.Second)
	defer cancel()

	req, err := http.NewRequestWithContext(ctx, method, reqURL, body)
	if err != nil {
		return nil, err
	}

	for _, h := range rp.header {
		req.Header.Add(h.Name, h.Value)
	}

	req.Header.Add("Test-Case", rp.name)

	if cbr != nil {
		cbr.req = req
		// this makes request use chunked encoding
		req.ContentLength = -1
		req.Trailer = make(http.Header)

		for _, h := range cbr.trailer {
			req.Trailer.Set(h.Name, "")
		}
	}

	if err := req.Write(st.conn); err != nil {
		return nil, err
	}

	resp, err := http.ReadResponse(bufio.NewReader(st.conn), req)
	if err != nil {
		return nil, err
	}

	respBody, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, err
	}

	resp.Body.Close()

	res := &serverResponse{
		status:    resp.StatusCode,
		header:    resp.Header,
		body:      respBody,
		connClose: resp.Close,
	}

	return res, nil
}

func (st *serverTester) http2(rp requestParam) (*serverResponse, error) {
	st.headerBlkBuf.Reset()
	st.header = make(http.Header)

	var id uint32

	if rp.streamID != 0 {
		id = rp.streamID
		if id >= st.nextStreamID && id%2 == 1 {
			st.nextStreamID = id + 2
		}
	} else {
		id = st.nextStreamID
		st.nextStreamID += 2
	}

	if !st.h2PrefaceSent {
		st.h2PrefaceSent = true
		fmt.Fprint(st.conn, "PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n")

		if err := st.fr.WriteSettings(); err != nil {
			return nil, err
		}
	}

	res := &serverResponse{
		streamID: id,
	}

	streams := make(map[uint32]*serverResponse)
	streams[id] = res

	if !rp.httpUpgrade {
		method := "GET"
		if rp.method != "" {
			method = rp.method
		}

		_ = st.enc.WriteField(pair(":method", method))

		scheme := "http"

		if rp.scheme != "" {
			scheme = rp.scheme
		}

		_ = st.enc.WriteField(pair(":scheme", scheme))

		authority := st.authority

		if rp.authority != "" {
			authority = rp.authority
		}

		_ = st.enc.WriteField(pair(":authority", authority))

		path := "/"

		if rp.path != "" {
			path = rp.path
		}

		_ = st.enc.WriteField(pair(":path", path))
		_ = st.enc.WriteField(pair("test-case", rp.name))

		for _, h := range rp.header {
			_ = st.enc.WriteField(h)
		}

		err := st.fr.WriteHeaders(http2.HeadersFrameParam{
			StreamID:      id,
			EndStream:     len(rp.body) == 0 && len(rp.trailer) == 0 && !rp.noEndStream,
			EndHeaders:    true,
			BlockFragment: st.headerBlkBuf.Bytes(),
		})
		if err != nil {
			return nil, err
		}

		if len(rp.body) != 0 {
			// TODO we assume rp.body fits in 1 frame
			if err := st.fr.WriteData(id, len(rp.trailer) == 0 && !rp.noEndStream, rp.body); err != nil {
				return nil, err
			}
		}

		if len(rp.trailer) != 0 {
			st.headerBlkBuf.Reset()

			for _, h := range rp.trailer {
				_ = st.enc.WriteField(h)
			}

			err := st.fr.WriteHeaders(http2.HeadersFrameParam{
				StreamID:      id,
				EndStream:     true,
				EndHeaders:    true,
				BlockFragment: st.headerBlkBuf.Bytes(),
			})
			if err != nil {
				return nil, err
			}
		}
	}
loop:
	for {
		fr, err := st.readFrame()
		if err != nil {
			return res, err
		}

		switch f := fr.(type) {
		case *http2.HeadersFrame:
			_, err := st.dec.Write(f.HeaderBlockFragment())
			if err != nil {
				return res, err
			}

			sr, ok := streams[f.FrameHeader.StreamID]
			if !ok {
				st.header = make(http.Header)
				break
			}

			sr.header = cloneHeader(st.header)

			var status int

			status, err = strconv.Atoi(sr.header.Get(":status"))
			if err != nil {
				return res, fmt.Errorf("Error parsing status code: %w", err)
			}

			sr.status = status

			if f.StreamEnded() && streamEnded(res, streams, sr) {
				break loop
			}
		case *http2.PushPromiseFrame:
			_, err := st.dec.Write(f.HeaderBlockFragment())
			if err != nil {
				return res, err
			}

			sr := &serverResponse{
				streamID:  f.PromiseID,
				reqHeader: cloneHeader(st.header),
			}

			streams[sr.streamID] = sr
		case *http2.DataFrame:
			sr, ok := streams[f.FrameHeader.StreamID]
			if !ok {
				break
			}

			sr.body = append(sr.body, f.Data()...)

			if f.StreamEnded() && streamEnded(res, streams, sr) {
				break loop
			}
		case *http2.RSTStreamFrame:
			sr, ok := streams[f.FrameHeader.StreamID]
			if !ok {
				break
			}

			sr.errCode = f.ErrCode

			if streamEnded(res, streams, sr) {
				break loop
			}
		case *http2.GoAwayFrame:
			if f.ErrCode == http2.ErrCodeNo {
				break
			}

			res.errCode = f.ErrCode
			res.connErr = true

			break loop
		case *http2.SettingsFrame:
			if f.IsAck() {
				break
			}

			if err := st.fr.WriteSettingsAck(); err != nil {
				return res, err
			}
		}
	}

	slices.SortFunc(res.pushResponse, func(a, b *serverResponse) int {
		return cmp.Compare(a.streamID, b.streamID)
	})

	return res, nil
}

func streamEnded(mainSr *serverResponse, streams map[uint32]*serverResponse, sr *serverResponse) bool {
	delete(streams, sr.streamID)

	if mainSr.streamID != sr.streamID {
		mainSr.pushResponse = append(mainSr.pushResponse, sr)
	}

	return len(streams) == 0
}

type serverResponse struct {
	status       int               // HTTP status code
	header       http.Header       // response header fields
	body         []byte            // response body
	streamID     uint32            // stream ID in HTTP/2
	errCode      http2.ErrCode     // error code received in HTTP/2 RST_STREAM or GOAWAY
	connErr      bool              // true if HTTP/2 connection error
	connClose    bool              // Connection: close is included in response header in HTTP/1 test
	reqHeader    http.Header       // http request header, currently only stores pushed request header
	pushResponse []*serverResponse // pushed response
}

func cloneHeader(h http.Header) http.Header {
	h2 := make(http.Header, len(h))

	for k, vv := range h {
		vv2 := make([]string, len(vv))
		copy(vv2, vv)
		h2[k] = vv2
	}

	return h2
}

func noopHandler(w http.ResponseWriter, r *http.Request) {
	if _, err := io.ReadAll(r.Body); err != nil {
		http.Error(w, fmt.Sprintf("Error io.ReadAll() = %v", err), http.StatusInternalServerError)
	}
}

type APIResponse struct {
	Status string                 `json:"status,omitempty"`
	Code   int                    `json:"code,omitempty"`
	Data   map[string]interface{} `json:"data,omitempty"`
}

type proxyProtocolV2 struct {
	command            proxyProtocolV2Command
	sourceAddress      net.Addr
	destinationAddress net.Addr
	additionalData     []byte
}

type proxyProtocolV2Command int

const (
	proxyProtocolV2CommandLocal proxyProtocolV2Command = 0x0
	proxyProtocolV2CommandProxy proxyProtocolV2Command = 0x1
)

type proxyProtocolV2Family int

const (
	proxyProtocolV2FamilyUnspec proxyProtocolV2Family = 0x0
	proxyProtocolV2FamilyInet   proxyProtocolV2Family = 0x1
	proxyProtocolV2FamilyInet6  proxyProtocolV2Family = 0x2
	proxyProtocolV2FamilyUnix   proxyProtocolV2Family = 0x3
)

type proxyProtocolV2Protocol int

const (
	proxyProtocolV2ProtocolUnspec proxyProtocolV2Protocol = 0x0
	proxyProtocolV2ProtocolStream proxyProtocolV2Protocol = 0x1
	proxyProtocolV2ProtocolDgram  proxyProtocolV2Protocol = 0x2
)

func writeProxyProtocolV2(w io.Writer, hdr proxyProtocolV2) error {
	if _, err := w.Write([]byte{0x0D, 0x0A, 0x0D, 0x0A, 0x00, 0x0D, 0x0A, 0x51, 0x55, 0x49, 0x54, 0x0A}); err != nil {
		return err
	}

	if _, err := w.Write([]byte{byte(0x20 | hdr.command)}); err != nil {
		return err
	}

	switch srcAddr := hdr.sourceAddress.(type) {
	case *net.TCPAddr:
		dstAddr := hdr.destinationAddress.(*net.TCPAddr)

		if len(srcAddr.IP) != len(dstAddr.IP) {
			panic("len(srcAddr.IP) != len(dstAddr.IP)")
		}

		var fam byte

		if len(srcAddr.IP) == 4 {
			fam = byte(proxyProtocolV2FamilyInet << 4)
		} else {
			fam = byte(proxyProtocolV2FamilyInet6 << 4)
		}

		fam |= byte(proxyProtocolV2ProtocolStream)

		if _, err := w.Write([]byte{fam}); err != nil {
			return err
		}

		length := uint16(len(srcAddr.IP)*2 + 4 + len(hdr.additionalData))

		if err := binary.Write(w, binary.BigEndian, length); err != nil {
			return err
		}

		if _, err := w.Write(srcAddr.IP); err != nil {
			return err
		}

		if _, err := w.Write(dstAddr.IP); err != nil {
			return err
		}

		if err := binary.Write(w, binary.BigEndian, uint16(srcAddr.Port)); err != nil {
			return err
		}

		if err := binary.Write(w, binary.BigEndian, uint16(dstAddr.Port)); err != nil {
			return err
		}
	case *net.UnixAddr:
		dstAddr := hdr.destinationAddress.(*net.UnixAddr)

		if len(srcAddr.Name) > 108 {
			panic("too long Unix source address")
		}

		if len(dstAddr.Name) > 108 {
			panic("too long Unix destination address")
		}

		fam := byte(proxyProtocolV2FamilyUnix << 4)

		switch srcAddr.Net {
		case "unix":
			fam |= byte(proxyProtocolV2ProtocolStream)
		case "unixdgram":
			fam |= byte(proxyProtocolV2ProtocolDgram)
		default:
			fam |= byte(proxyProtocolV2ProtocolUnspec)
		}

		if _, err := w.Write([]byte{fam}); err != nil {
			return err
		}

		length := uint16(216 + len(hdr.additionalData))

		if err := binary.Write(w, binary.BigEndian, length); err != nil {
			return err
		}

		zeros := make([]byte, 108)

		if _, err := w.Write([]byte(srcAddr.Name)); err != nil {
			return err
		}

		if _, err := w.Write(zeros[:108-len(srcAddr.Name)]); err != nil {
			return err
		}

		if _, err := w.Write([]byte(dstAddr.Name)); err != nil {
			return err
		}

		if _, err := w.Write(zeros[:108-len(dstAddr.Name)]); err != nil {
			return err
		}
	default:
		fam := byte(proxyProtocolV2FamilyUnspec<<4) | byte(proxyProtocolV2ProtocolUnspec)

		if _, err := w.Write([]byte{fam}); err != nil {
			return err
		}

		length := uint16(len(hdr.additionalData))

		if err := binary.Write(w, binary.BigEndian, length); err != nil {
			return err
		}
	}

	if _, err := w.Write(hdr.additionalData); err != nil {
		return err
	}

	return nil
}
