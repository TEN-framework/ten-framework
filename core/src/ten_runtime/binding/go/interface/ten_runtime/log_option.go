//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

package ten_runtime

// LogOption represents configuration options for logging, including skip
// parameter for extensibility
type LogOption struct {
	// Skip is the number of stack frames to skip when determining caller
	// information
	Skip int
}

// DefaultLogOption is the default log option instance with Skip=2
var DefaultLogOption = LogOption{
	Skip: 2,
}
