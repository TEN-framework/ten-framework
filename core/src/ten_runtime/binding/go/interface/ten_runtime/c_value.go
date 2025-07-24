//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

package ten_runtime

// #include "c_value.h"
import "C"

type value struct {
	baseTenObject[*C.ten_go_value_t]
}

func tenValueDestroy(cValue C.uintptr_t) {
	C.ten_go_value_destroy(cValue)
}
