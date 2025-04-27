//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

package ten

// #include "ten_env_tester.h"
import "C"
import "runtime"

type (
	TesterResultHandler func(TenEnvTester, CmdResult)
)

type TenEnvTester interface {
	SendCmd(cmd Cmd, handler TesterResultHandler) error
	OnStartDone() error
}

var (
	_ TenEnvTester = new(tenEnvTester)
)

type tenEnvTester struct {
	baseTenObject[C.uintptr_t]
}

func (p *tenEnvTester) SendCmd(cmd Cmd, handler TesterResultHandler) error {
	if cmd == nil {
		return newTenError(
			ErrorCodeInvalidArgument,
			"cmd is required.",
		)
	}

	return withCGOLimiter(func() error {
		return p.sendCmd(cmd, handler)
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
	)

	return withCGoError(&cStatus)
}

func (p *tenEnvTester) OnStartDone() error {
	C.ten_go_ten_env_tester_on_start_done(p.cPtr)

	return nil
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
