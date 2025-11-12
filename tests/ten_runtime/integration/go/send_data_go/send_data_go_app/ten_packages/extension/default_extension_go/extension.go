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

type defaultExtension struct {
	ten.DefaultExtension

	counter int
}

func (ext *defaultExtension) OnConfigure(tenEnv ten.TenEnv) {
	tenEnv.LogDebug("OnConfigure")
	tenEnv.OnConfigureDone()
}

func (ext *defaultExtension) OnCmd(
	tenEnv ten.TenEnv,
	cmd ten.Cmd,
) {
	newData, error := ten.NewData("data")
	if error != nil {
		panic("Failed to create data.")
	}

	testBytes := []byte("hello world")

	if error := newData.AllocBuf(len(testBytes)); error != nil {
		panic("Failed to alloc buf.")
	}

	buf, error := newData.LockBuf()
	if error != nil {
		panic("Failed to lock buf.")
	}

	copy(buf, testBytes)

	if error := newData.UnlockBuf(&buf); error != nil {
		panic("Failed to unlock buf.")
	}

	if error := tenEnv.SendData(newData, nil); error != nil {
		panic("Failed to send data.")
	}

	data2, error := ten.NewData("data2")
	if error != nil {
		panic("Failed to create data.")
	}

	data2.SetPropertyString("test_key", "test_value")

	if error := tenEnv.SendData(data2, nil); error != nil {
		panic("Failed to send data.")
	}

	cmdResult, error := ten.NewCmdResult(ten.StatusCodeOk, cmd)
	if error != nil {
		panic("Failed to create cmd result.")
	}

	if error := cmdResult.SetPropertyString("detail", "send data done"); error != nil {
		panic("Failed to set property string.")
	}

	if error := tenEnv.ReturnResult(cmdResult, nil); error != nil {
		panic("Failed to return result.")
	}
}

func (ext *defaultExtension) OnData(
	tenEnv ten.TenEnv,
	data ten.Data,
) {
	dataName, error := data.GetName()
	if error != nil {
		panic("Failed to get data name.")
	}

	tenEnv.LogDebug("OnData" + dataName)

	if dataName == "data2" {
		testValue, error := data.GetPropertyString("test_key")
		if error != nil {
			panic("Failed to get property string.")
		}

		if testValue != "test_value" {
			panic("Property string not match.")
		}

		return
	}

	buf, error := data.GetBuf()
	if error != nil {
		panic("Failed to get buf.")
	}

	if string(buf) != "hello world" {
		panic("Data not match.")
	}

	for i := 0; i < len(buf); i++ {
		buf[i] = 0
	}

	lockedBuf, error := data.LockBuf()
	if error != nil {
		panic("Failed to lock buf.")
	}

	if string(lockedBuf) != "hello world" {
		panic("Data not match.")
	}

	if error := data.UnlockBuf(&lockedBuf); error != nil {
		panic("Failed to unlock buf.")
	}

	tenEnv.LogDebug("Data process done.")
}

func newAExtension(name string) ten.Extension {
	return &defaultExtension{counter: 0}
}

func init() {
	// Register addon.
	err := ten.RegisterAddonAsExtension(
		"default_extension_go",
		ten.NewDefaultExtensionAddon(newAExtension),
	)
	if err != nil {
		panic("Failed to register addon.")
	}
}
