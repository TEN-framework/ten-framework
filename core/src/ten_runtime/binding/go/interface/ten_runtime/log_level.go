//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

package ten_runtime

// LogLevel is the log level.
type LogLevel int32

const (
	// LogLevelDebug is the debug log level.
	LogLevelDebug = 1

	// LogLevelInfo is the info log level.
	LogLevelInfo = 2

	// LogLevelWarn is the warn log level.
	LogLevelWarn = 3

	// LogLevelError is the error log level.
	LogLevelError = 4
)
