//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

package ten_runtime

// #include "extension_tester.h"
import "C"

import (
	"fmt"
	"log"
	"runtime"
	"time"
	"unsafe"
)

// IExtensionTester is the interface for the extension tester.
type IExtensionTester interface {
	OnStart(tenEnv TenEnvTester)
	OnStop(tenEnv TenEnvTester)
	OnDeinit(tenEnv TenEnvTester)

	OnCmd(tenEnv TenEnvTester, cmd Cmd)
	OnData(tenEnv TenEnvTester, data Data)
	OnAudioFrame(tenEnv TenEnvTester, audioFrame AudioFrame)
	OnVideoFrame(tenEnv TenEnvTester, videoFrame VideoFrame)
}

// DefaultExtensionTester implements the Extension interface.
type DefaultExtensionTester struct{}

var _ IExtensionTester = new(DefaultExtensionTester)

// OnStart starts the extension.
func (p *DefaultExtensionTester) OnStart(tenEnv TenEnvTester) {
	tenEnv.OnStartDone()
}

// OnStop stops the extension.
func (p *DefaultExtensionTester) OnStop(tenEnv TenEnvTester) {
	tenEnv.OnStopDone()
}

// OnDeinit deinitializes the extension.
func (p *DefaultExtensionTester) OnDeinit(tenEnv TenEnvTester) {
	tenEnv.OnDeinitDone()
}

// OnCmd handles the command.
func (p *DefaultExtensionTester) OnCmd(tenEnv TenEnvTester, cmd Cmd) {
}

// OnData handles the data.
func (p *DefaultExtensionTester) OnData(tenEnv TenEnvTester, data Data) {
}

// OnAudioFrame handles the audio frame.
func (p *DefaultExtensionTester) OnAudioFrame(
	tenEnv TenEnvTester,
	audioFrame AudioFrame,
) {
}

// OnVideoFrame handles the video frame.
func (p *DefaultExtensionTester) OnVideoFrame(
	tenEnv TenEnvTester,
	videoFrame VideoFrame,
) {
}

type extTester struct {
	IExtensionTester
	baseTenObject[*C.ten_go_extension_tester_t]
}

// ExtensionTester is the interface for the extension tester.
type ExtensionTester interface {
	SetTestModeSingle(addonName string, propertyJSONStr string) error
	SetTimeout(timeout time.Duration) error
	Run() error
}

var _ ExtensionTester = new(extTester)

func (p *extTester) SetTestModeSingle(
	addonName string,
	propertyJSONStr string,
) error {
	cStatus := C.ten_go_extension_tester_set_test_mode_single(
		p.cPtr,
		unsafe.Pointer(unsafe.StringData(addonName)),
		C.int(len(addonName)),
		unsafe.Pointer(unsafe.StringData(propertyJSONStr)),
		C.int(len(propertyJSONStr)),
	)

	return withCGoError(&cStatus)
}

func (p *extTester) SetTimeout(timeout time.Duration) error {
	cStatus := C.ten_go_extension_tester_set_timeout(
		p.cPtr,
		C.uint64_t(timeout.Microseconds()),
	)

	return withCGoError(&cStatus)
}

func (p *extTester) Run() error {
	cStatus := C.ten_go_extension_tester_run(p.cPtr)

	return withCGoError(&cStatus)
}

// NewExtensionTester creates a new extension tester.
func NewExtensionTester(
	iExtensionTester IExtensionTester,
) (ExtensionTester, error) {
	if iExtensionTester == nil {
		return nil, NewTenError(
			ErrorCodeInvalidArgument,
			"iExtensionTester is nil",
		)
	}

	extTesterInstance := &extTester{
		IExtensionTester: iExtensionTester,
	}

	extTesterObjID := newImmutableHandle(extTesterInstance)

	var bridge *C.ten_go_extension_tester_t
	cgoError := C.ten_go_extension_tester_create(
		cHandle(extTesterObjID),
		&bridge,
	)
	if err := withCGoError(&cgoError); err != nil {
		log.Printf("Failed to create extension tester, %v\n", err)
		loadAndDeleteImmutableHandle(extTesterObjID)
		return nil, err
	}

	extTesterInstance.cPtr = (*C.ten_go_extension_tester_t)(
		unsafe.Pointer(bridge),
	)

	runtime.SetFinalizer(extTesterInstance, func(p *extTester) {
		C.ten_go_extension_tester_finalize(p.cPtr)
	})

	return extTesterInstance, nil
}

//export tenGoExtensionTesterOnStart
func tenGoExtensionTesterOnStart(
	extTesterID C.uintptr_t,
	tenEnvTesterID C.uintptr_t,
) {
	extTesterObj, ok := loadImmutableHandle(goHandle(extTesterID)).(*extTester)
	if !ok {
		panic(
			fmt.Sprintf(
				"Failed to get extension tester  from handle map, id: %d.",
				uintptr(extTesterID),
			),
		)
	}

	tenEnvTesterObj, ok := handle(tenEnvTesterID).get().(TenEnvTester)
	if !ok {
		panic(
			fmt.Sprintf(
				"Failed to get ten env tester from handle map, id: %d.",
				uintptr(tenEnvTesterID),
			),
		)
	}

	extTesterObj.OnStart(tenEnvTesterObj)
}

//export tenGoExtensionTesterOnStop
func tenGoExtensionTesterOnStop(
	extTesterID C.uintptr_t,
	tenEnvTesterID C.uintptr_t,
) {
	extTesterObj, ok := loadImmutableHandle(goHandle(extTesterID)).(*extTester)
	if !ok {
		panic(
			fmt.Sprintf(
				"Failed to get extension tester  from handle map, id: %d.",
				uintptr(extTesterID),
			),
		)
	}

	tenEnvTesterObj, ok := handle(tenEnvTesterID).get().(TenEnvTester)
	if !ok {
		panic(
			fmt.Sprintf(
				"Failed to get ten env tester from handle map, id: %d.",
				uintptr(tenEnvTesterID),
			),
		)
	}

	extTesterObj.OnStop(tenEnvTesterObj)
}

//export tenGoExtensionTesterOnDeinit
func tenGoExtensionTesterOnDeinit(
	extTesterID C.uintptr_t,
	tenEnvTesterID C.uintptr_t,
) {
	extTesterObj, ok := loadAndDeleteImmutableHandle(goHandle(extTesterID)).(*extTester)
	if !ok {
		panic(
			fmt.Sprintf(
				"Failed to get extension tester  from handle map, id: %d.",
				uintptr(extTesterID),
			),
		)
	}

	tenEnvTesterObj, ok := handle(tenEnvTesterID).get().(TenEnvTester)
	if !ok {
		panic(
			fmt.Sprintf(
				"Failed to get ten env tester from handle map, id: %d.",
				uintptr(tenEnvTesterID),
			),
		)
	}

	extTesterObj.OnDeinit(tenEnvTesterObj)
}

//export tenGoExtensionTesterOnCmd
func tenGoExtensionTesterOnCmd(
	extTesterID C.uintptr_t,
	tenEnvTesterID C.uintptr_t,
	cmdBridge C.uintptr_t,
) {
	extTesterObj, ok := loadImmutableHandle(goHandle(extTesterID)).(*extTester)
	if !ok {
		panic(
			fmt.Sprintf(
				"Failed to get extension tester from handle map, id: %d.",
				uintptr(extTesterID),
			),
		)
	}

	tenEnvTesterObj, ok := handle(tenEnvTesterID).get().(TenEnvTester)
	if !ok {
		panic(
			fmt.Sprintf(
				"Failed to get ten env tester from handle map, id: %d.",
				uintptr(tenEnvTesterID),
			),
		)
	}

	// The GO cmd object should be created in GO side, and managed by the GO GC.
	customCmd := newCmd(cmdBridge)
	extTesterObj.OnCmd(tenEnvTesterObj, customCmd)
}

//export tenGoExtensionTesterOnData
func tenGoExtensionTesterOnData(
	extTesterID C.uintptr_t,
	tenEnvTesterID C.uintptr_t,
	dataBridge C.uintptr_t,
) {
	extTesterObj, ok := loadImmutableHandle(goHandle(extTesterID)).(*extTester)
	if !ok {
		panic(
			fmt.Sprintf(
				"Failed to get extension tester from handle map, id: %d.",
				uintptr(extTesterID),
			),
		)
	}

	tenEnvTesterObj, ok := handle(tenEnvTesterID).get().(TenEnvTester)
	if !ok {
		panic(
			fmt.Sprintf(
				"Failed to get ten env tester from handle map, id: %d.",
				uintptr(tenEnvTesterID),
			),
		)
	}

	// The GO data object should be created in GO side, and managed by the GO
	// GC.
	customData := newData(dataBridge)
	extTesterObj.OnData(tenEnvTesterObj, customData)
}

//export tenGoExtensionTesterOnAudioFrame
func tenGoExtensionTesterOnAudioFrame(
	extTesterID C.uintptr_t,
	tenEnvTesterID C.uintptr_t,
	audioFrameBridge C.uintptr_t,
) {
	extTesterObj, ok := loadImmutableHandle(goHandle(extTesterID)).(*extTester)
	if !ok {
		panic(
			fmt.Sprintf(
				"Failed to get extension tester from handle map, id: %d.",
				uintptr(extTesterID),
			),
		)
	}

	tenEnvTesterObj, ok := handle(tenEnvTesterID).get().(TenEnvTester)
	if !ok {
		panic(
			fmt.Sprintf(
				"Failed to get ten env tester from handle map, id: %d.",
				uintptr(tenEnvTesterID),
			),
		)
	}

	// The GO audio_frame object should be created in GO side, and managed by
	// the GO GC.
	customAudioFrame := newAudioFrame(audioFrameBridge)
	extTesterObj.OnAudioFrame(tenEnvTesterObj, customAudioFrame)
}

//export tenGoExtensionTesterOnVideoFrame
func tenGoExtensionTesterOnVideoFrame(
	extTesterID C.uintptr_t,
	tenEnvTesterID C.uintptr_t,
	videoFrameBridge C.uintptr_t,
) {
	extTesterObj, ok := loadImmutableHandle(goHandle(extTesterID)).(*extTester)
	if !ok {
		panic(
			fmt.Sprintf(
				"Failed to get extension tester from handle map, id: %d.",
				uintptr(extTesterID),
			),
		)
	}

	tenEnvTesterObj, ok := handle(tenEnvTesterID).get().(TenEnvTester)
	if !ok {
		panic(
			fmt.Sprintf(
				"Failed to get ten env tester from handle map, id: %d.",
				uintptr(tenEnvTesterID),
			),
		)
	}

	// The GO video_frame object should be created in GO side, and managed by
	// the GO GC.
	customVideoFrame := newVideoFrame(videoFrameBridge)
	extTesterObj.OnVideoFrame(tenEnvTesterObj, customVideoFrame)
}
