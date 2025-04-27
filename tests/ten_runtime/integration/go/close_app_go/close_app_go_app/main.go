//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

package main

import (
	"fmt"
	"time"

	ten "ten_framework/ten_runtime"
)

type defaultApp struct {
	ten.DefaultApp
}

func (p *defaultApp) OnDeinit(tenEnv ten.TenEnv) {
	fmt.Println("DefaultApp onDeinit")

	tenEnv.OnDeinitDone()
}

func appRoutine(app ten.App, stopped chan<- struct{}) {
	app.Run(false)
	stopped <- struct{}{}
}

func appTimeout(app ten.App, timeout time.Duration) {
	time.Sleep(timeout)
	app.Close()
}

func main() {
	// test app
	app, err := ten.NewApp(&defaultApp{})
	if err != nil {
		fmt.Println("Failed to create app.")
	}

	stopped := make(chan struct{}, 1)
	go appRoutine(app, stopped)
	go appTimeout(app, time.Duration(5*time.Second))
	<-stopped
}
