//
// nghttp2 - HTTP/2 C Library
//
// Copyright (c) 2015 Tatsuhiro Tsujikawa
//
// Permission is hereby granted, free of charge, to any person obtaining
// a copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to
// permit persons to whom the Software is furnished to do so, subject to
// the following conditions:
//
// The above copyright notice and this permission notice shall be
// included in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
// LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
// OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
// WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
//
package main

import (
	"bytes"
	"crypto/rand"
	"encoding/binary"
	"flag"
	"fmt"
	"log"
	"time"

	"github.com/bradfitz/gomemcache/memcache"
)

func makeKey(len int) []byte {
	b := make([]byte, len)
	if _, err := rand.Read(b); err != nil {
		log.Fatalf("rand.Read: %v", err)
	}
	return b
}

func main() {
	var host = flag.String("host", "127.0.0.1", "memcached host")
	var port = flag.Int("port", 11211, "memcached port")
	var cipher = flag.String("cipher", "aes-128-cbc", "cipher for TLS ticket encryption")
	var interval = flag.Int("interval", 3600, "interval to update TLS ticket keys")

	flag.Parse()

	var keylen int
	switch *cipher {
	case "aes-128-cbc":
		keylen = 48
	case "aes-256-cbc":
		keylen = 80
	default:
		log.Fatalf("cipher: unknown cipher %v", cipher)
	}

	mc := memcache.New(fmt.Sprintf("%v:%v", *host, *port))

	keys := [][]byte{
		makeKey(keylen), // current encryption key
		makeKey(keylen), // next encryption key; now decryption only
	}

	for {
		buf := new(bytes.Buffer)
		if err := binary.Write(buf, binary.BigEndian, uint32(1)); err != nil {
			log.Fatalf("failed to write version: %v", err)
		}

		for _, key := range keys {
			if err := binary.Write(buf, binary.BigEndian, uint16(keylen)); err != nil {
				log.Fatalf("failed to write length: %v", err)
			}
			if _, err := buf.Write(key); err != nil {
				log.Fatalf("buf.Write: %v", err)
			}
		}

		mc.Set(&memcache.Item{
			Key:        "nghttpx:tls-ticket-key",
			Value:      buf.Bytes(),
			Expiration: int32((*interval) + 300),
		})

		<-time.After(time.Duration(*interval) * time.Second)

		// rotate keys.  the last key is now encryption key.
		// generate new key and append it to the last, so that
		// we can at least decrypt TLS ticket encrypted by new
		// key on the host which does not get new key yet.
		// keep at most past 11 keys as decryption only key
		n := len(keys) + 1
		if n > 13 {
			n = 13
		}
		newKeys := make([][]byte, n)
		newKeys[0] = keys[len(keys)-1]
		copy(newKeys[1:], keys[0:n-2])
		newKeys[n-1] = makeKey(keylen)

		keys = newKeys
	}

}
