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
	"fmt"
	"runtime"
)

//export tenGoCreateTenEnv
func tenGoCreateTenEnv(cInstance C.uintptr_t) C.uintptr_t {
	tenEnvInstance := &tenEnv{
		attachToType: tenAttachToInvalid,
	}
	tenEnvInstance.cPtr = cInstance
	tenEnvInstance.pool = newJobPool(5)

	id := newhandle(tenEnvInstance)
	tenEnvInstance.goObjID = id

	runtime.SetFinalizer(tenEnvInstance, func(p *tenEnv) {
		C.ten_go_ten_env_finalize(p.cPtr)
	})

	return C.uintptr_t(id)
}

//export tenGoDestroyTenEnv
func tenGoDestroyTenEnv(tenEnvObjID C.uintptr_t) {
	r, ok := handle(tenEnvObjID).free().(*tenEnv)

	r.attachToType = tenAttachToInvalid

	if !ok {
		panic(
			fmt.Sprintf(
				"Failed to get ten env from handle map, id: %d.",
				uintptr(tenEnvObjID),
			),
		)
	} else {
		r.close()
	}
}

//export tenGoOnCmdResult
func tenGoOnCmdResult(
	tenEnvObjID C.uintptr_t,
	cmdResultBridge C.uintptr_t,
	resultHandler C.uintptr_t,
	isCompleted C.bool,
	cgoError C.ten_go_error_t,
) {
	tenEnvObj, ok := handle(tenEnvObjID).get().(TenEnv)
	if !ok {
		panic(
			fmt.Sprintf(
				"Failed to get ten env from handle map, id: %d.",
				uintptr(tenEnvObjID),
			),
		)
	}

	var cr *cmdResult = nil
	if cmdResultBridge == 0 {
		cr = nil
	} else {
		cr = newCmdResult(cmdResultBridge)
	}

	var cb any = nil
	if isCompleted {
		cb = loadAndDeleteGoHandle(goHandle(resultHandler))
	} else {
		cb = loadGoHandle(goHandle(resultHandler))
	}

	if cb == nil || cb == goHandleNil {
		// Should not happen.
		panic("The result handler is not found from handle map.")
	}

	err := withCGoError(&cgoError)

	cb.(ResultHandler)(tenEnvObj, cr, err)
}

//export tenGoOnError
func tenGoOnError(
	tenEnvObjID C.uintptr_t,
	errorHandler C.uintptr_t,
	cgoError C.ten_go_error_t,
) {
	tenEnvObj, ok := handle(tenEnvObjID).get().(TenEnv)
	if !ok {
		panic(
			fmt.Sprintf(
				"Failed to get ten env from handle map, id: %d.",
				uintptr(tenEnvObjID),
			),
		)
	}

	cb := loadAndDeleteGoHandle(goHandle(errorHandler))
	if cb == nil || cb == goHandleNil {
		// Should not happen.
		panic("The error handler is not found from handle map.")
	}

	err := withCGoError(&cgoError)

	cb.(ErrorHandler)(tenEnvObj, err)
}

//export tenGoDestroyTenEnvTester
func tenGoDestroyTenEnvTester(tenEnvTesterObjID C.uintptr_t) {
	r, ok := handle(tenEnvTesterObjID).free().(*tenEnvTester)

	if !ok {
		panic(
			fmt.Sprintf(
				"Failed to get ten env tester from handle map, id: %d.",
				uintptr(tenEnvTesterObjID),
			),
		)
	} else {
		r.close()
	}
}

//export tenGoTesterOnCmdResult
func tenGoTesterOnCmdResult(
	tenEnvTesterObjID C.uintptr_t,
	cmdResultBridge C.uintptr_t,
	resultHandler C.uintptr_t,
	isCompleted C.bool,
	cgoError C.ten_go_error_t,
) {
	tenEnvTesterObj, ok := handle(tenEnvTesterObjID).get().(TenEnvTester)
	if !ok {
		panic(
			fmt.Sprintf(
				"Failed to get ten env tester from handle map, id: %d.",
				uintptr(tenEnvTesterObjID),
			),
		)
	}

	var cr *cmdResult = nil
	if cmdResultBridge == 0 {
		cr = nil
	} else {
		cr = newCmdResult(cmdResultBridge)
	}

	var cb any = nil
	if isCompleted {
		cb = loadAndDeleteGoHandle(goHandle(resultHandler))
	} else {
		cb = loadGoHandle(goHandle(resultHandler))
	}

	if cb == nil || cb == goHandleNil {
		// Should not happen.
		panic("The result handler is not found from handle map.")
	}

	err := withCGoError(&cgoError)

	cb.(TesterResultHandler)(tenEnvTesterObj, cr, err)
}

//export tenGoTesterOnError
func tenGoTesterOnError(
	tenEnvTesterObjID C.uintptr_t,
	errorHandler C.uintptr_t,
	cgoError C.ten_go_error_t,
) {
	tenEnvTesterObj, ok := handle(tenEnvTesterObjID).get().(TenEnvTester)
	if !ok {
		panic(
			fmt.Sprintf(
				"Failed to get ten env tester from handle map, id: %d.",
				uintptr(tenEnvTesterObjID),
			),
		)
	}

	cb := loadAndDeleteGoHandle(goHandle(errorHandler))
	if cb == nil || cb == goHandleNil {
		panic("The error handler is not found from handle map.")
	}

	err := withCGoError(&cgoError)

	cb.(TesterErrorHandler)(tenEnvTesterObj, err)
}

//export tenGoSetPropertyCallback
func tenGoSetPropertyCallback(
	tenEnvObjID C.uintptr_t,
	handlerObjID C.uintptr_t,
	result C.bool,
) {
	tenEnvObj, ok := handle(tenEnvObjID).get().(TenEnv)
	if !ok {
		panic(
			fmt.Sprintf(
				"Failed to get ten env from handle map, id: %d.",
				uintptr(tenEnvObjID),
			),
		)
	}

	handlerObj, ok := handle(handlerObjID).free().(func(TenEnv, error))
	if !ok {
		panic(
			fmt.Sprintf(
				"Failed to get callback from handle map, id: %d.",
				uintptr(handlerObjID),
			),
		)
	}

	if result {
		handlerObj(tenEnvObj, nil)
	} else {
		handlerObj(tenEnvObj, NewTenError(
			ErrorCodeGeneric,
			"Failed to set property",
		))
	}
}

//export tenGoGetPropertyCallback
func tenGoGetPropertyCallback(
	tenEnvObjID C.uintptr_t,
	handlerObjID C.uintptr_t,
	valueObjID C.uintptr_t,
) {
	tenEnvObj, ok := handle(tenEnvObjID).get().(TenEnv)
	if !ok {
		panic(
			fmt.Sprintf(
				"Failed to get ten env from handle map, id: %d.",
				uintptr(tenEnvObjID),
			),
		)
	}

	handlerObj, ok := handle(handlerObjID).free().(func(TenEnv, *cValue, error))
	if !ok {
		panic(
			fmt.Sprintf(
				"Failed to get callback from handle map, id: %d.",
				uintptr(handlerObjID),
			),
		)
	}

	valueObj, ok := handle(valueObjID).get().(*cValue)
	if !ok {
		handlerObj(tenEnvObj, nil, NewTenError(
			ErrorCodeGeneric,
			"Failed to get property",
		))
		return
	}

	handlerObj(tenEnvObj, valueObj, nil)
}
