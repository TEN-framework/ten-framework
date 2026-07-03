//
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0.
// See the LICENSE file for more information.
//
// Note that this is just an example extension written in the GO programming
// language, so the package name does not equal to the containing directory
// name. However, it is not common in Go.
//

package http

import (
	"fmt"
	"ten_packages/extension/simple_http_server_go/endpoint"

	ten "ten_framework/ten_runtime"
)

type httpExtension struct {
	ten.DefaultExtension
	server *endpoint.Endpoint
}

func newHttpExtension(name string) ten.Extension {
	return &httpExtension{}
}

func (p *httpExtension) OnStart(tenEnv ten.TenEnv) {
	p.server = endpoint.NewEndpoint(tenEnv)
	p.server.Start()
}

func (p *httpExtension) OnStop(tenEnv ten.TenEnv) {
	if p.server != nil {
		p.server.Stop()
	}

	tenEnv.OnStopDone()
}

func (p *httpExtension) OnCmd(
	tenEnv ten.TenEnv,
	cmd ten.Cmd,
) {
	fmt.Println("httpExtension OnCmd")

	cmdResult, _ := ten.NewCmdResult(ten.StatusCodeOk, cmd)
	cmdResult.SetPropertyString("detail", "This is default go extension.")
	tenEnv.ReturnResult(cmdResult, nil)
}

func init() {
	fmt.Println("httpExtension init")

	// Register addon
	ten.RegisterAddonAsExtension(
		"simple_http_server_go",
		ten.NewDefaultExtensionAddon(newHttpExtension),
	)
}
