//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

package ten

// #include "cmd.h"
import "C"

// StatusCode is an alias of TEN_STATUS_CODE from TEN runtime.
type StatusCode int8

// StatusCode values. These definitions need to be the same as the
// TEN_STATUS_CODE enum in C.
//
// Note: To achieve the best compatibility, any new enum item, should be added
// to the end to avoid changing the value of previous enum items.
const (
	statusCodeInvalid StatusCode = -1

	// StatusCodeOk is an alias of TEN_STATUS_CODE_OK from TEN runtime.
	StatusCodeOk = 0

	// StatusCodeError is an alias of TEN_STATUS_CODE_ERROR from TEN runtime.
	StatusCodeError = 1

	statusCodeLast = 2
)

func (s StatusCode) valid() bool {
	return s > statusCodeInvalid && s < statusCodeLast
}

// CmdResult is the interface for the cmd result.
type CmdResult interface {
	CmdBase

	Clone() (CmdResult, error)
	GetStatusCode() (StatusCode, error)
	SetFinal(isFinal bool) error
	IsFinal() (bool, error)
	IsCompleted() (bool, error)
}

type cmdResult struct {
	*cmd
}

// NewCmdResult creates a new cmd result.
func NewCmdResult(statusCode StatusCode, targetCmd Cmd) (CmdResult, error) {
	defer targetCmd.keepAlive()

	res := globalPool.process(func() any {
		cmdBridge := C.ten_go_cmd_create_cmd_result(
			C.int(statusCode),
			targetCmd.getCPtr(),
		)
		cmdInstance := newCmdResult(cmdBridge)
		return cmdInstance
	})
	switch res := res.(type) {
	case error:
		return nil, res
	case *cmdResult:
		return res, nil
	default:
		panic("Should not happen.")
	}
}

var _ CmdResult = new(cmdResult)

func newCmdResult(bridge C.uintptr_t) *cmdResult {
	cs := &cmdResult{cmd: newCmd(bridge)}

	return cs
}

func (p *cmdResult) GetStatusCode() (StatusCode, error) {
	return globalPool.process(func() any {
		defer p.keepAlive()

		return (StatusCode)(C.ten_go_cmd_result_get_status_code(p.cPtr))
	}).(StatusCode), nil
}

//export tenGoCreateCmdResult
func tenGoCreateCmdResult(bridge C.uintptr_t) C.uintptr_t {
	cmdStatusInstance := newCmdResult(bridge)

	id := newhandle(cmdStatusInstance)
	cmdStatusInstance.goObjID = id

	return C.uintptr_t(id)
}

func (p *cmdResult) SetFinal(isFinal bool) error {
	return withCGOLimiter(func() error {
		apiStatus := C.ten_go_cmd_result_set_final(
			p.getCPtr(),
			C.bool(isFinal),
		)
		return withCGoError(&apiStatus)
	})
}

func (p *cmdResult) IsFinal() (bool, error) {
	var isFinal C.bool
	err := withCGOLimiter(func() error {
		apiStatus := C.ten_go_cmd_result_is_final(
			p.getCPtr(),
			&isFinal,
		)
		return withCGoError(&apiStatus)
	})

	if err != nil {
		return false, err
	}

	return bool(isFinal), nil
}

func (p *cmdResult) IsCompleted() (bool, error) {
	var isCompleted C.bool
	err := withCGOLimiter(func() error {
		apiStatus := C.ten_go_cmd_result_is_completed(
			p.getCPtr(),
			&isCompleted,
		)
		return withCGoError(&apiStatus)
	})

	if err != nil {
		return false, err
	}

	return bool(isCompleted), nil
}

func (p *cmdResult) Clone() (CmdResult, error) {
	var bridge C.uintptr_t
	err := withCGOLimiter(func() error {
		apiStatus := C.ten_go_cmd_result_clone(p.getCPtr(), &bridge)
		return withCGoError(&apiStatus)
	})

	if err != nil {
		return nil, err
	}

	if bridge == 0 {
		// Should not happen.
		return nil, newTenError(
			ErrorCodeInvalidArgument,
			"bridge is nil",
		)
	}

	return newCmdResult(bridge), nil
}
