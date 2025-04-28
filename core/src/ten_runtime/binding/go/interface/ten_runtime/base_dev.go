//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

//go:build dev

package ten

import "fmt"

// All methods in this file are only enabled with build flags: -tags "dev".

// escapeCheck checks if the baseTenObject is invalid now, ex: after calling
// SendCmd.
//
// In "dev" mode, we will check if the calling sequences on one baseTenObject
// are unexpected. This method will panic if the baseTenObject is invalid.
func (p *baseTenObject[T]) escapeCheck(fn func()) {
	defer p.RUnlock()
	p.RLock()

	if p.cPtr == nil {
		panic(fmt.Sprintf("Calling on an object %+v which is out of scope.", p))
	}

	fn()
}
