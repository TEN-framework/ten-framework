//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

package ten

// #include "ten_env.h"
import "C"

func (p *tenEnv) ReturnResult(
	statusCmd CmdResult,
	handler ErrorHandler,
) error {
	if statusCmd == nil {
		return newTenError(
			ErrorCodeInvalidArgument,
			"cmd result is required.",
		)
	}

	defer func() {
		p.keepAlive()
		statusCmd.keepAlive()
	}()

	cb := goHandleNil
	if handler != nil {
		cb = newGoHandle(handler)
	}

	err := withCGOLimiter(func() error {
		apiStatus := C.ten_go_ten_env_return_result(
			p.cPtr,
			statusCmd.getCPtr(),
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
