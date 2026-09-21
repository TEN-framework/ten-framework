// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0.
package ten_runtime

// #include "ten_runtime/binding/go/interface/ten_runtime/log.h"
import "C"

import "unsafe"

// ConfigureLogFromJSONBytes applies an advanced logging configuration to the
// current TEN process. It is intended for preload components that receive a
// session-specific log policy before starting a graph.
func ConfigureLogFromJSONBytes(config []byte) error {
	if len(config) == 0 {
		return NewTenError(ErrorCodeInvalidArgument, "log configuration is empty")
	}

	status := C.ten_go_configure_log_json(
		unsafe.Pointer(&config[0]), C.int(len(config)),
	)
	return withCGoError(&status)
}
