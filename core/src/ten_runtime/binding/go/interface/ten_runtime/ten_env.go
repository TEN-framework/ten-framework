//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

package ten_runtime

// #include "ten_env.h"
import "C"

import (
	"runtime"
	"strings"
	"unsafe"
)

type (
	// ResultHandler is a function type that represents a handler for the result
	// of a command.
	ResultHandler func(TenEnv, CmdResult, error)

	// ErrorHandler is a function type that represents a handler for errors of a
	// non-command type message.
	ErrorHandler func(TenEnv, error)
)

// TenEnv represents the interface for the TEN (Run Time Environment) component.
type TenEnv interface {
	SendCmd(cmd Cmd, handler ResultHandler) error
	SendCmdEx(cmd Cmd, handler ResultHandler) error
	SendData(data Data, handler ErrorHandler) error
	SendVideoFrame(videoFrame VideoFrame, handler ErrorHandler) error
	SendAudioFrame(audioFrame AudioFrame, handler ErrorHandler) error

	ReturnResult(result CmdResult, handler ErrorHandler) error

	OnConfigureDone() error
	OnInitDone() error
	OnStartDone() error
	OnStopDone() error
	OnDeinitDone() error
	OnCreateInstanceDone(instance any, context uintptr) error

	iProperty
	InitPropertyFromJSONBytes(value []byte) error

	LogDebug(msg string) error
	LogInfo(msg string) error
	LogWarn(msg string) error
	LogError(msg string) error
	Log(
		level LogLevel,
		msg string,
		category *string,
		fields *Value,
		option *LogOption,
	) error
}

// Making a compile-time assertion which indicates that if 'ten' type doesn't
// implement all the methods of the 'Ten' interface, the code will fail to
// compile, and you'll get an error. This is a way to ensure at compile time
// that a certain type satisfies an interface, even if you don't use that type
// to call the interface's methods in your code.
//
// In simpler terms, it's a way to say: "Make sure that the 'ten' type
// implements the 'Ten' interface. I don't need to use this check anywhere else
// in my code; I just want to make sure it's true." If 'ten' doesn't implement
// Ten, you'll know as soon as you try to compile.
var (
	_ TenEnv = new(tenEnv)
)

type tenAttachTo uint8

const (
	tenAttachToInvalid tenAttachTo = iota
	tenAttachToExtension
	tenAttachToExtensionGroup
	tenAttachToApp
	tenAttachToAddon
)

type tenEnv struct {
	baseTenObject[C.uintptr_t]

	attachToType tenAttachTo
}

func (p *tenEnv) attachToExtension() {
	if p.attachToType != tenAttachToInvalid {
		panic("The ten object can only be attached once.")
	}

	p.attachToType = tenAttachToExtension
}

func (p *tenEnv) attachToApp() {
	if p.attachToType != tenAttachToInvalid {
		panic("The ten object can only be attached once.")
	}

	p.attachToType = tenAttachToApp
}

func (p *tenEnv) SendCmd(cmd Cmd, handler ResultHandler) error {
	if cmd == nil {
		return NewTenError(
			ErrorCodeInvalidArgument,
			"cmd is required.",
		)
	}

	return withCGOLimiter(func() error {
		return p.sendCmd(cmd, handler)
	})
}

func (p *tenEnv) sendCmd(cmd Cmd, handler ResultHandler) error {
	defer cmd.keepAlive()

	cb := goHandleNil
	if handler != nil {
		cb = newGoHandle(handler)
	}

	cStatus := C.ten_go_ten_env_send_cmd(
		p.cPtr,
		cmd.getCPtr(),
		cHandle(cb),
		C.bool(false),
	)

	return withCGoError(&cStatus)
}

func (p *tenEnv) SendCmdEx(cmd Cmd, handler ResultHandler) error {
	if cmd == nil {
		return NewTenError(
			ErrorCodeInvalidArgument,
			"cmd is required.",
		)
	}

	return withCGOLimiter(func() error {
		return p.sendCmdEx(cmd, handler)
	})
}

func (p *tenEnv) sendCmdEx(cmd Cmd, handler ResultHandler) error {
	defer cmd.keepAlive()

	cb := goHandleNil
	if handler != nil {
		cb = newGoHandle(handler)
	}

	cStatus := C.ten_go_ten_env_send_cmd(
		p.cPtr,
		cmd.getCPtr(),
		cHandle(cb),
		C.bool(true),
	)

	return withCGoError(&cStatus)
}

// Exported function to be called from C when the async operation in C
// completes.
//
//export tenGoCAsyncApiCallback
func tenGoCAsyncApiCallback(
	callbackHandle C.uintptr_t,
	apiStatus C.ten_go_error_t,
) {
	// Start a Go routine for asynchronous processing to prevent blocking C code
	// on the native thread, which would in turn block the Go code calling the C
	// code.
	go func() {
		goHandle := goHandle(callbackHandle)
		done := loadAndDeleteGoHandle(goHandle).(chan error)

		err := withCGoError(&apiStatus)

		done <- err
	}()
}

func (p *tenEnv) SendData(data Data, handler ErrorHandler) error {
	if data == nil {
		return NewTenError(
			ErrorCodeInvalidArgument,
			"data is required.",
		)
	}

	defer data.keepAlive()

	cb := goHandleNil
	if handler != nil {
		cb = newGoHandle(handler)
	}

	err := withCGOLimiter(func() error {
		apiStatus := C.ten_go_ten_env_send_data(
			p.cPtr,
			data.getCPtr(),
			cHandle(cb),
		)
		err := withCGoError(&apiStatus)
		return err
	})

	if err != nil {
		// Clean up the handle if there was an error.
		loadAndDeleteGoHandle(cb)
	}

	return err
}

func (p *tenEnv) SendVideoFrame(
	videoFrame VideoFrame,
	handler ErrorHandler,
) error {
	if videoFrame == nil {
		return NewTenError(
			ErrorCodeInvalidArgument,
			"videoFrame is required.",
		)
	}

	defer videoFrame.keepAlive()

	cb := goHandleNil
	if handler != nil {
		cb = newGoHandle(handler)
	}

	err := withCGOLimiter(func() error {
		apiStatus := C.ten_go_ten_env_send_video_frame(
			p.cPtr,
			videoFrame.getCPtr(),
			cHandle(cb),
		)
		return withCGoError(&apiStatus)
	})

	if err != nil {
		// Clean up the handle if there was an error.
		loadAndDeleteGoHandle(cb)
	}

	return err
}

func (p *tenEnv) SendAudioFrame(
	audioFrame AudioFrame,
	handler ErrorHandler,
) error {
	if audioFrame == nil {
		return NewTenError(
			ErrorCodeInvalidArgument,
			"audioFrame is required.",
		)
	}

	defer audioFrame.keepAlive()

	cb := goHandleNil
	if handler != nil {
		cb = newGoHandle(handler)
	}

	err := withCGOLimiter(func() error {
		apiStatus := C.ten_go_ten_env_send_audio_frame(
			p.cPtr,
			audioFrame.getCPtr(),
			cHandle(cb),
		)
		return withCGoError(&apiStatus)
	})

	if err != nil {
		// Clean up the handle if there was an error.
		loadAndDeleteGoHandle(cb)
	}

	return err
}

func (p *tenEnv) OnConfigureDone() error {
	p.LogDebug("OnConfigureDone")

	C.ten_go_ten_env_on_configure_done(p.cPtr)

	return nil
}

func (p *tenEnv) OnInitDone() error {
	C.ten_go_ten_env_on_init_done(p.cPtr)

	return nil
}

func (p *tenEnv) OnStartDone() error {
	C.ten_go_ten_env_on_start_done(p.cPtr)

	return nil
}

func (p *tenEnv) OnStopDone() error {
	C.ten_go_ten_env_on_stop_done(p.cPtr)

	return nil
}

func (p *tenEnv) OnDeinitDone() error {
	C.ten_go_ten_env_on_deinit_done(p.cPtr)

	return nil
}

func (p *tenEnv) OnCreateInstanceDone(instance any, context uintptr) error {
	if instance == nil {
		C.ten_go_ten_env_on_create_instance_done(
			p.cPtr,
			C.uintptr_t(0), // Represent NULL in C.
			C.uintptr_t(context),
		)
	} else {
		switch instance := instance.(type) {
		case *extension:
			C.ten_go_ten_env_on_create_instance_done(p.cPtr, instance.cPtr, C.uintptr_t(context))
		default:
			panic("instance must be extension or extension group.")
		}
	}

	return nil
}

func (p *tenEnv) String() string {
	cString := C.ten_go_ten_env_debug_info(p.cPtr)
	defer C.free(unsafe.Pointer(cString))

	return C.GoString(cString)
}

func (p *tenEnv) LogDebug(msg string) error {
	return p.logInternal(LogLevelDebug, msg, nil, nil, nil)
}

func (p *tenEnv) LogInfo(msg string) error {
	return p.logInternal(LogLevelInfo, msg, nil, nil, nil)
}

func (p *tenEnv) LogWarn(msg string) error {
	return p.logInternal(LogLevelWarn, msg, nil, nil, nil)
}

func (p *tenEnv) LogError(msg string) error {
	return p.logInternal(LogLevelError, msg, nil, nil, nil)
}

func (p *tenEnv) Log(
	level LogLevel,
	msg string,
	category *string,
	fields *Value,
	option *LogOption,
) error {
	return p.logInternal(level, msg, category, fields, option)
}

func (p *tenEnv) logInternal(
	level LogLevel,
	msg string,
	category *string,
	fields *Value,
	option *LogOption,
) error {
	if option == nil {
		option = &DefaultLogOption
	}

	// Get caller info.
	pc, fileName, lineNo, ok := runtime.Caller(option.Skip)
	funcName := "unknown"
	if ok {
		fn := runtime.FuncForPC(pc)
		if fn != nil {
			funcName = fn.Name()

			parts := strings.Split(funcName, ".")
			if len(parts) > 0 {
				// The last part is the method name.
				funcName = parts[len(parts)-1]
			}
		}
	} else {
		fileName = "unknown"
		lineNo = 0
	}

	var cCategory unsafe.Pointer
	var cCategoryLen int = 0
	if category != nil {
		cCategory = unsafe.Pointer(unsafe.StringData(*category))
		cCategoryLen = len(*category)
	}

	cStatus := C.ten_go_ten_env_log(
		p.cPtr,
		C.int(level),
		unsafe.Pointer(unsafe.StringData(funcName)),
		C.int(len(funcName)),
		unsafe.Pointer(unsafe.StringData(fileName)),
		C.int(len(fileName)),
		C.int(lineNo),
		unsafe.Pointer(unsafe.StringData(msg)),
		C.int(len(msg)),
		cCategory,
		C.int(cCategoryLen),
	)

	return withCGoError(&cStatus)
}
