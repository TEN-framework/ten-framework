//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

package default_extension_go

import (
	"encoding/json"
	"fmt"
	"io"
	"os"

	ten "ten_framework/ten_runtime"
)

type defaultExtension struct {
	ten.DefaultExtension
}

type data struct {
	Extension string `json:"extension"`
}

func newDefaultExtension(name string) ten.Extension {
	return &defaultExtension{}
}

func (p *defaultExtension) OnDeinit(tenEnv ten.TenEnv) {
	fmt.Println("defaultExtension OnStop")

	// Write something to file, so the test case can check if this function is
	// always called when app exits.

	d := &data{
		Extension: "exit_signal",
	}

	dest := "exit_signal.json"
	if _, err := os.Stat(dest); err == nil {
		os.Remove(dest)
	}

	dest_file, err := os.Create(dest)
	if err != nil {
		panic(err)
	}

	defer dest_file.Close()

	bytes, _ := json.MarshalIndent(d, "", " ")
	_, err = io.WriteString(dest_file, string(bytes))
	if err != nil {
		panic(err)
	}

	tenEnv.OnDeinitDone()
}

func init() {
	fmt.Println("defaultExtension init")

	// Register addon
	ten.RegisterAddonAsExtension(
		"default_extension_go",
		ten.NewDefaultExtensionAddon(newDefaultExtension),
	)
}
