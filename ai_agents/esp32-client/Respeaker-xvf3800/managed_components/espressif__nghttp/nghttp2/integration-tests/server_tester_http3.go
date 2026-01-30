//go:build quic

package nghttp2

import (
	"bytes"
	"context"
	"crypto/tls"
	"io"
	"net/http"
	"net/url"
	"time"

	"github.com/quic-go/quic-go/http3"
)

func (st *serverTester) http3(rp requestParam) (*serverResponse, error) {
	rt := &http3.RoundTripper{
		TLSClientConfig: &tls.Config{
			InsecureSkipVerify: true,
		},
	}

	defer rt.Close()

	c := &http.Client{
		Transport: rt,
	}

	method := "GET"
	if rp.method != "" {
		method = rp.method
	}

	var body io.Reader

	if rp.body != nil {
		body = bytes.NewBuffer(rp.body)
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

	// TODO http3 package does not support trailer at the time of
	// this writing.

	resp, err := c.Do(req)
	if err != nil {
		return nil, err
	}

	defer resp.Body.Close()

	respBody, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, err
	}

	res := &serverResponse{
		status:    resp.StatusCode,
		header:    resp.Header,
		body:      respBody,
		connClose: resp.Close,
	}

	return res, nil
}
