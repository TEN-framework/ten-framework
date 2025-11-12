//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

package default_extension_go

import (
	"encoding/json"
	ten "ten_framework/ten_runtime"
)

type baseExtension struct {
	ten.DefaultExtension
}

type PredefinedProperty struct {
	PredefinedInt8    int8              `json:"predefined_int8"`
	PredefinedInt16   int16             `json:"predefined_int16"`
	PredefinedInt32   int32             `json:"predefined_int32"`
	PredefinedInt64   int64             `json:"predefined_int64"`
	PredefinedUint8   uint8             `json:"predefined_uint8"`
	PredefinedUint16  uint16            `json:"predefined_uint16"`
	PredefinedUint32  uint32            `json:"predefined_uint32"`
	PredefinedUint64  uint64            `json:"predefined_uint64"`
	PredefinedFloat32 float32           `json:"predefined_float32"`
	PredefinedFloat64 float64           `json:"predefined_float64"`
	PredefinedBool    bool              `json:"predefined_bool"`
	PredefinedString  string            `json:"predefined_string"`
	PredefinedObject  map[string]string `json:"predefined_object"`
	PredefinedArray   []string          `json:"predefined_array"`
}

func (ext *baseExtension) OnInit(tenEnv ten.TenEnv) {
	tenEnv.LogDebug("OnInit")

	if prop, err := tenEnv.GetPropertyString("env_not_set_has_default"); err != nil &&
		prop != "" {
		panic("The default value should be used.")
	}

	propJsonBytes, err := tenEnv.GetPropertyToJSONBytes("")
	if err != nil {
		panic("GetPropertyToJSONBytes with empty path should not fail.")
	}

	// Print the json bytes.
	tenEnv.LogInfo("propJsonBytes: " + string(propJsonBytes))

	// Parse the json bytes to a map.
	var predefinedProperty PredefinedProperty
	if err := json.Unmarshal(propJsonBytes, &predefinedProperty); err != nil {
		panic("Failed to unmarshal json bytes.")
	}

	// Check the predefined properties.
	if predefinedProperty.PredefinedInt8 != 123 {
		panic("predefined_int8 should be 123.")
	}

	if predefinedProperty.PredefinedInt16 != 12345 {
		panic("predefined_int16 should be 12345.")
	}

	if predefinedProperty.PredefinedInt32 != 1234567890 {
		panic("predefined_int32 should be 1234567890.")
	}

	if predefinedProperty.PredefinedInt64 != 1234567890 {
		panic("predefined_int64 should be 1234567890.")
	}

	if predefinedProperty.PredefinedUint8 != 123 {
		panic("predefined_uint8 should be 123.")
	}

	if predefinedProperty.PredefinedUint16 != 12345 {
		panic("predefined_uint16 should be 12345.")
	}

	if predefinedProperty.PredefinedUint32 != 1234567890 {
		panic("predefined_uint32 should be 1234567890.")
	}

	if predefinedProperty.PredefinedUint64 != 1234567890 {
		panic("predefined_uint64 should be 1234567890.")
	}

	if predefinedProperty.PredefinedFloat32 != 123.456 {
		panic("predefined_float32 should be 123.456.")
	}

	if predefinedProperty.PredefinedFloat64 != 1234567890.123 {
		panic("predefined_float64 should be 1234567890.123.")
	}

	if predefinedProperty.PredefinedBool != true {
		panic("predefined_bool should be true.")
	}

	if predefinedProperty.PredefinedString != "hello" {
		panic("predefined_string should be hello.")
	}

	if predefinedProperty.PredefinedObject["prop_key"] != "prop_value" {
		panic("predefined_object should be a map with prop_key and prop_value.")
	}

	if len(predefinedProperty.PredefinedArray) != 2 ||
		predefinedProperty.PredefinedArray[0] != "item1" ||
		predefinedProperty.PredefinedArray[1] != "item2" {
		panic("predefined_array should be an array with two items.")
	}

	tenEnv.OnInitDone()
}

func (ext *baseExtension) OnStop(tenEnv ten.TenEnv) {
	tenEnv.LogDebug("OnStop")

	tenEnv.OnStopDone()
}

type UserStruct struct {
	Num int
	Str string
}

type NestedUserStruct struct {
	UserData *UserStruct
	StrSlice []string
}

type aExtension struct {
	baseExtension
}

func newAExtension(name string) ten.Extension {
	return &aExtension{}
}

func (p *aExtension) OnCmd(
	tenEnv ten.TenEnv,
	cmd ten.Cmd,
) {
	if err := tenEnv.SetProperty("testBool", false); err != nil {
		panic("Should not happen.")
	}

	if err := tenEnv.SetProperty("testInt", 3); err != nil {
		panic("Should not happen.")
	}

	var testUint uint = 4
	if err := tenEnv.SetProperty("testUint", testUint); err != nil {
		panic("Should not happen.")
	}

	var testUint32 uint32 = 44
	if err := tenEnv.SetProperty("testUint32", testUint32); err != nil {
		panic("Should not happen.")
	}

	var testInt8 int8 = 8
	if err := tenEnv.SetProperty("testInt8", testInt8); err != nil {
		panic("Should not happen.")
	}

	var testInt16 int16 = 16
	if err := tenEnv.SetProperty("testInt16", testInt16); err != nil {
		panic("Should not happen.")
	}

	var testInt32 int32 = 32
	if err := tenEnv.SetProperty("testInt32", testInt32); err != nil {
		panic("Should not happen.")
	}

	var testInt64 int64 = 64
	if err := tenEnv.SetProperty("testInt64", testInt64); err != nil {
		panic("Should not happen.")
	}

	if err := tenEnv.SetProperty("testString", "test string"); err != nil {
		panic("Should not happen.")
	}

	var testFloat32 float32 = 32.0
	if err := tenEnv.SetProperty("testFloat32", testFloat32); err != nil {
		panic("Should not happen.")
	}

	var testFloat64 float64 = 64.0
	if err := tenEnv.SetProperty("testFloat64", testFloat64); err != nil {
		panic("Should not happen.")
	}

	if err := tenEnv.SetProperty(
		"testObject",
		&UserStruct{2, "hello"},
	); err != nil {
		panic("Should not happen.")
	}

	// The struct property is not supported.
	if err := tenEnv.SetProperty(
		"testStruct",
		NestedUserStruct{
			UserData: &UserStruct{5, "world"},
			StrSlice: []string{"a", "b", "c"},
		},
	); err == nil {
		panic("Should not happen.")
	}

	strBytes := []byte("hello")
	if err := tenEnv.SetProperty("testByteArray", strBytes); err != nil {
		panic("Should not happen.")
	}

	// ================================================

	testBool, err := tenEnv.GetPropertyBool("testBool")
	if err != nil || testBool != false {
		panic("Should not happen.")
	}

	testInt, err := tenEnv.GetPropertyInt64("testInt")
	if err != nil || testInt != 3 {
		panic("Should not happen.")
	}

	testUint64, err := tenEnv.GetPropertyUint64("testUint")
	if err != nil || testUint64 != 4 {
		panic("Should not happen.")
	}

	testUint32, err = tenEnv.GetPropertyUint32("testUint32")
	if err != nil || testUint32 != 44 {
		panic("Should not happen.")
	}

	testInt8, err = tenEnv.GetPropertyInt8("testInt8")
	if err != nil || testInt8 != 8 {
		panic("Should not happen.")
	}

	testInt16, err = tenEnv.GetPropertyInt16("testInt16")
	if err != nil || testInt16 != 16 {
		panic("Should not happen.")
	}

	testInt32, err = tenEnv.GetPropertyInt32("testInt32")
	if err != nil || testInt32 != 32 {
		panic("Should not happen.")
	}

	testInt64, err = tenEnv.GetPropertyInt64("testInt64")
	if err != nil || testInt64 != 64 {
		panic("Should not happen.")
	}

	testFloat32, err = tenEnv.GetPropertyFloat32("testFloat32")
	if err != nil || testFloat32 != 32.0 {
		panic("Should not happen.")
	}

	testFloat64, err = tenEnv.GetPropertyFloat64("testFloat64")
	if err != nil || testFloat64 != 64.0 {
		panic("Should not happen.")
	}

	testString, err := tenEnv.GetPropertyString("testString")
	if err != nil || testString != "test string" {
		panic("Should not happen.")
	}

	testObject, err := tenEnv.GetPropertyPtr(
		"testObject",
	)
	if err != nil || testObject.(*UserStruct).Num != 2 {
		panic("Should not happen.")
	}

	testByteArray, err := tenEnv.GetPropertyBytes(
		"testByteArray",
	)
	if err != nil || string(testByteArray) != "hello" {
		panic("Should not happen.")
	}

	cmdResult, _ := ten.NewCmdResult(ten.StatusCodeOk, cmd)
	cmdResult.SetPropertyString("detail", "okok")
	err = tenEnv.ReturnResult(cmdResult, nil)
	if err != nil {
		panic("ReturnResult failed")
	}
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
