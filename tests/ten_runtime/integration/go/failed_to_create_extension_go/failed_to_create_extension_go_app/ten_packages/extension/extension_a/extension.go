//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

package default_extension_go

import (
	ten "ten_framework/ten_runtime"
)

type baseExtension struct {
	ten.DefaultExtension
}

func (ext *baseExtension) OnStart(tenEnv ten.TenEnv) {
	panic("Should not happen.")
}

func (ext *baseExtension) OnStop(tenEnv ten.TenEnv) {
	panic("Should not happen.")
}

type aExtension struct {
	baseExtension
}

func newAExtension(name string) ten.Extension {
	// This line is important. Return nil to simulate failing to create an
	// extension.
	return nil
}

func (p *aExtension) OnCmd(
	tenEnv ten.TenEnv,
	cmd ten.Cmd,
) {
	panic("Should not happen.")
}

func init() {
	// Register addon
	err := ten.RegisterAddonAsExtension(
		"extension_a",
		ten.NewDefaultExtensionAddon(newAExtension),
	)
	if err != nil {
		panic("Failed to register addon.")
	}
}
