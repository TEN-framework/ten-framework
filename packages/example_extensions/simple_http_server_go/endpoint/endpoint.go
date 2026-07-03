//
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0.
// See the LICENSE file for more information.
//

package endpoint

import (
	"context"
	"fmt"
	"net/http"
	"time"

	ten "ten_framework/ten_runtime"
)

type Endpoint struct {
	server *http.Server
	tenEnv ten.TenEnv // The communication bridge of this extension.
}

func NewEndpoint(tenEnv ten.TenEnv) *Endpoint {
	return &Endpoint{
		tenEnv: tenEnv,
	}
}

func (s *Endpoint) defaultHandler(
	writer http.ResponseWriter,
	request *http.Request,
) {
	switch request.URL.Path {
	case "/health":
		writer.WriteHeader(http.StatusOK)
	default:
		cmdResultChan := make(chan ten.CmdResult, 1)
		cmd, _ := ten.NewCmd("demo")

		s.tenEnv.SendCmd(
			cmd,
			func(tenEnv ten.TenEnv, cmdResult ten.CmdResult, e error) {
				cmdResultChan <- cmdResult
			},
		)

		select {
		case cmdResult := <-cmdResultChan:
			writer.WriteHeader(http.StatusOK)

			detail, _ := cmdResult.GetPropertyString("detail")
			writer.Write([]byte(detail))
		}
	}
}

func (e *Endpoint) Start() {
	mux := http.NewServeMux()
	mux.HandleFunc("/", e.defaultHandler)

	e.server = &http.Server{
		Addr:    ":8001",
		Handler: mux,
	}

	go func() {
		if err := e.server.ListenAndServe(); err != nil {
			if err != http.ErrServerClosed {
				panic(err)
			}
		}
	}()

	go func() {
		// Check if the server is ready.
		for {
			resp, err := http.Get("http://127.0.0.1:8001/health")
			if err != nil {
				continue
			}

			defer resp.Body.Close()

			if resp.StatusCode == 200 {
				break
			}

			time.Sleep(50 * time.Millisecond)
		}

		fmt.Println("http server starts.")

		e.tenEnv.OnStartDone()
	}()
}

func (s *Endpoint) Stop() {
	if s.server != nil {
		s.server.Shutdown(context.Background())
	}
}
