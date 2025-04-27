//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

//go:build !dev

package ten

// All methods in this file are only enabled without build flags: -tags "dev".

// escapeCheck checks if the baseTenObject is invalid now, ex: after calling
// SendCmd.
//
// There's no check in "release" mode, it's the user's responsibility to make
// sure the calling sequences on one baseTenObject are correct.
func (p *baseTenObject[T]) escapeCheck(fn func()) {
	fn()
}
