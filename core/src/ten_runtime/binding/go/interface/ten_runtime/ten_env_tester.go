//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

package ten_runtime

// #include "ten_env_tester.h"
import "C"
import (
	"runtime"
	"strings"
	"unsafe"
)

type (
	// TesterResultHandler is the handler for the result of the command.
	TesterResultHandler func(TenEnvTester, CmdResult, error)

	// TesterErrorHandler is the handler for the error of the command.
	TesterErrorHandler func(TenEnvTester, error)
)

// TenEnvTester is the interface for the ten env tester.
type TenEnvTester interface {
	OnStartDone() error
	OnStopDone() error
	OnDeinitDone() error

	SendCmd(cmd Cmd, handler TesterResultHandler) error
	SendCmdEx(cmd Cmd, handler TesterResultHandler) error
	SendData(data Data, handler TesterErrorHandler) error
	SendAudioFrame(audioFrame AudioFrame, handler TesterErrorHandler) error
	SendVideoFrame(videoFrame VideoFrame, handler TesterErrorHandler) error

	ReturnResult(result CmdResult, handler TesterErrorHandler) error

	StopTest(testResult *TenError) error

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

var (
	_ TenEnvTester = new(tenEnvTester)
)

type tenEnvTester struct {
	baseTenObject[C.uintptr_t]
}

func (p *tenEnvTester) OnStartDone() error {
	return withCGOLimiter(func() error {
		cStatus := C.ten_go_ten_env_tester_on_start_done(p.cPtr)
		return withCGoError(&cStatus)
	})
}

func (p *tenEnvTester) OnStopDone() error {
	return withCGOLimiter(func() error {
		cStatus := C.ten_go_ten_env_tester_on_stop_done(p.cPtr)
		return withCGoError(&cStatus)
	})
}

func (p *tenEnvTester) OnDeinitDone() error {
	return withCGOLimiter(func() error {
		cStatus := C.ten_go_ten_env_tester_on_deinit_done(p.cPtr)
		return withCGoError(&cStatus)
	})
}

func (p *tenEnvTester) SendCmd(cmd Cmd, handler TesterResultHandler) error {
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

func (p *tenEnvTester) SendCmdEx(cmd Cmd, handler TesterResultHandler) error {
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

func (p *tenEnvTester) SendData(data Data, handler TesterErrorHandler) error {
	if data == nil {
		return NewTenError(
			ErrorCodeInvalidArgument,
			"data is required.",
		)
	}

	return withCGOLimiter(func() error {
		return p.sendData(data, handler)
	})
}

func (p *tenEnvTester) SendAudioFrame(
	audioFrame AudioFrame,
	handler TesterErrorHandler,
) error {
	if audioFrame == nil {
		return NewTenError(
			ErrorCodeInvalidArgument,
			"audioFrame is required.",
		)
	}

	return withCGOLimiter(func() error {
		return p.sendAudioFrame(audioFrame, handler)
	})
}

func (p *tenEnvTester) SendVideoFrame(
	videoFrame VideoFrame,
	handler TesterErrorHandler,
) error {
	if videoFrame == nil {
		return NewTenError(
			ErrorCodeInvalidArgument,
			"videoFrame is required.",
		)
	}

	return withCGOLimiter(func() error {
		return p.sendVideoFrame(videoFrame, handler)
	})
}

func (p *tenEnvTester) ReturnResult(
	result CmdResult,
	handler TesterErrorHandler,
) error {
	if result == nil {
		return NewTenError(
			ErrorCodeInvalidArgument,
			"result is required.",
		)
	}

	return withCGOLimiter(func() error {
		return p.returnResult(result, handler)
	})
}

func (p *tenEnvTester) StopTest(testResult *TenError) error {
	return withCGOLimiter(func() error {
		return p.stopTest(testResult)
	})
}

func (p *tenEnvTester) sendCmd(cmd Cmd, handler TesterResultHandler) error {
	defer cmd.keepAlive()

	cb := goHandleNil
	if handler != nil {
		cb = newGoHandle(handler)
	}

	cStatus := C.ten_go_ten_env_tester_send_cmd(
		p.cPtr,
		cmd.getCPtr(),
		cHandle(cb),
		C.bool(false),
	)

	return withCGoError(&cStatus)
}

func (p *tenEnvTester) sendCmdEx(cmd Cmd, handler TesterResultHandler) error {
	defer cmd.keepAlive()

	cb := goHandleNil
	if handler != nil {
		cb = newGoHandle(handler)
	}

	cStatus := C.ten_go_ten_env_tester_send_cmd(
		p.cPtr,
		cmd.getCPtr(),
		cHandle(cb),
		C.bool(true),
	)

	return withCGoError(&cStatus)
}

func (p *tenEnvTester) sendData(data Data, handler TesterErrorHandler) error {
	defer data.keepAlive()

	cb := goHandleNil
	if handler != nil {
		cb = newGoHandle(handler)
	}

	cStatus := C.ten_go_ten_env_tester_send_data(
		p.cPtr,
		data.getCPtr(),
		cHandle(cb),
	)

	return withCGoError(&cStatus)
}

func (p *tenEnvTester) sendAudioFrame(
	audioFrame AudioFrame,
	handler TesterErrorHandler,
) error {
	defer audioFrame.keepAlive()

	cb := goHandleNil
	if handler != nil {
		cb = newGoHandle(handler)
	}

	cStatus := C.ten_go_ten_env_tester_send_audio_frame(
		p.cPtr,
		audioFrame.getCPtr(),
		cHandle(cb),
	)

	return withCGoError(&cStatus)
}

func (p *tenEnvTester) sendVideoFrame(
	videoFrame VideoFrame,
	handler TesterErrorHandler,
) error {
	defer videoFrame.keepAlive()

	cb := goHandleNil
	if handler != nil {
		cb = newGoHandle(handler)
	}

	cStatus := C.ten_go_ten_env_tester_send_video_frame(
		p.cPtr,
		videoFrame.getCPtr(),
		cHandle(cb),
	)

	return withCGoError(&cStatus)
}

func (p *tenEnvTester) returnResult(
	result CmdResult,
	handler TesterErrorHandler,
) error {
	if result == nil {
		return NewTenError(
			ErrorCodeInvalidArgument,
			"result is required.",
		)
	}

	cb := goHandleNil
	if handler != nil {
		cb = newGoHandle(handler)
	}

	cStatus := C.ten_go_ten_env_tester_return_result(
		p.cPtr,
		result.getCPtr(),
		cHandle(cb),
	)

	return withCGoError(&cStatus)
}

func (p *tenEnvTester) stopTest(testResult *TenError) error {
	var cStatus C.ten_go_error_t

	if testResult != nil {
		cStatus = C.ten_go_ten_env_tester_stop_test(
			p.cPtr,
			C.uint(testResult.ErrorCode),
			unsafe.Pointer(unsafe.StringData(testResult.ErrorMessage)),
			C.uint(len(testResult.ErrorMessage)),
		)
	} else {
		cStatus = C.ten_go_ten_env_tester_stop_test(
			p.cPtr,
			0,
			nil,
			0,
		)
	}

	return withCGoError(&cStatus)
}

func (p *tenEnvTester) LogDebug(msg string) error {
	return p.logInternal(LogLevelDebug, msg, nil, nil, nil)
}

func (p *tenEnvTester) LogInfo(msg string) error {
	return p.logInternal(LogLevelInfo, msg, nil, nil, nil)
}

func (p *tenEnvTester) LogWarn(msg string) error {
	return p.logInternal(LogLevelWarn, msg, nil, nil, nil)
}

func (p *tenEnvTester) LogError(msg string) error {
	return p.logInternal(LogLevelError, msg, nil, nil, nil)
}

func (p *tenEnvTester) Log(
	level LogLevel,
	msg string,
	category *string,
	fields *Value,
	option *LogOption,
) error {
	return p.logInternal(level, msg, category, fields, option)
}

func (p *tenEnvTester) logInternal(
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

	cStatus := C.ten_go_ten_env_tester_log(
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

//export tenGoCreateTenEnvTester
func tenGoCreateTenEnvTester(cInstance C.uintptr_t) C.uintptr_t {
	tenEnvTesterInstance := &tenEnvTester{}
	tenEnvTesterInstance.cPtr = cInstance
	tenEnvTesterInstance.pool = newJobPool(5)
	runtime.SetFinalizer(tenEnvTesterInstance, func(p *tenEnvTester) {
		C.ten_go_ten_env_tester_finalize(p.cPtr)
	})

	id := newhandle(tenEnvTesterInstance)
	tenEnvTesterInstance.goObjID = id

	return C.uintptr_t(id)
}
