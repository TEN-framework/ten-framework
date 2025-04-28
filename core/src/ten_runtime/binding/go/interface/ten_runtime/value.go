//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

package ten

// #include "value.h"
import "C"

import (
	"runtime"
)

type value struct {
	baseTenObject[*C.ten_go_value_t]
}

func tenValueDestroy(cValue C.uintptr_t) {
	C.ten_go_value_destroy(cValue)
}

//export tenGoCreateValue
func tenGoCreateValue(cInstance *C.ten_go_value_t) C.uintptr_t {
	valueInstance := &value{}
	valueInstance.cPtr = cInstance
	runtime.SetFinalizer(valueInstance, func(p *value) {
		C.ten_go_value_finalize(p.cPtr)
	})

	id := newhandle(valueInstance)
	valueInstance.goObjID = id

	return C.uintptr_t(id)
}

//export tenGoUnrefObj
func tenGoUnrefObj(id C.uintptr_t) {
	handle(id).free()
}
