//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

package ten

// #include "extension_tester.h"
import "C"

import (
	"fmt"
	"log"
	"runtime"
)

// ExtensionTester is the interface for the extension tester.
type ExtensionTester interface {
	OnStart(tenEnv TenEnvTester)
	OnCmd(tenEnv TenEnvTester, cmd Cmd)
	OnData(tenEnv TenEnvTester, data Data)
	OnAudioFrame(tenEnv TenEnvTester, audioFrame AudioFrame)
	OnVideoFrame(tenEnv TenEnvTester, videoFrame VideoFrame)
}

// DefaultExtensionTester implements the Extension interface.
type DefaultExtensionTester struct{}

var _ ExtensionTester = new(DefaultExtensionTester)

// OnStart starts the extension.
func (p *DefaultExtensionTester) OnStart(tenEnv TenEnvTester) {
	tenEnv.OnStartDone()
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
	ExtensionTester

	baseTenObject[C.uintptr_t]
}

// WrapExtensionTester wraps the extension tester.
func WrapExtensionTester(
	extensionTester ExtensionTester,
) ExtensionTester {
	extTesterInstance := &extTester{
		ExtensionTester: extensionTester,
	}

	extTesterObjID := newImmutableHandle(extTesterInstance)

	var bridge C.uintptr_t
	cgoError := C.ten_go_extension_tester_create(
		cHandle(extTesterObjID),
		&bridge,
	)
	if err := withCGoError(&cgoError); err != nil {
		log.Printf("Failed to create extension tester, %v\n", err)
		loadAndDeleteImmutableHandle(extTesterObjID)
		return nil
	}

	extTesterInstance.cPtr = bridge

	runtime.SetFinalizer(extTesterInstance, func(p *extTester) {
		C.ten_go_extension_tester_finalize(p.cPtr)
	})

	return extTesterInstance
}

func newExtensionTesterWithBridge(
	extensionTester ExtensionTester,
	bridge C.uintptr_t,
) goHandle {
	instance := &extTester{
		ExtensionTester: extensionTester,
	}

	instance.cPtr = bridge

	runtime.SetFinalizer(instance, func(p *extTester) {
		C.ten_go_extension_tester_finalize(p.cPtr)
	})

	return newImmutableHandle(instance)
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
