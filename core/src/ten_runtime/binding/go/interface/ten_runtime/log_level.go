//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

package ten

// LogLevel is the log level.
type LogLevel int32

const (
	// LogLevelVerbose is the verbose log level.
	LogLevelVerbose = 1

	// LogLevelDebug is the debug log level.
	LogLevelDebug = 2

	// LogLevelInfo is the info log level.
	LogLevelInfo = 3

	// LogLevelWarn is the warn log level.
	LogLevelWarn = 4

	// LogLevelError is the error log level.
	LogLevelError = 5

	// LogLevelFatal is the fatal log level.
	LogLevelFatal = 6
)
