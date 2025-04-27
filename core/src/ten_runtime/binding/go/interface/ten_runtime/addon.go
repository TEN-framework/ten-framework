//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

package ten

// #include "addon.h"
import "C"

import (
	"fmt"
)

// Addon is the interface for the addon.
type Addon interface {
	OnInit(tenEnv TenEnv)
	OnDeinit(tenEnv TenEnv)
	OnCreateInstance(tenEnv TenEnv, name string, context uintptr)
}

// ExtensionConstructor is the constructor for the extension.
type ExtensionConstructor func(name string) Extension

// ExtensionAddon is the addon for the extension.
type ExtensionAddon struct {
	constructor ExtensionConstructor
}

// OnInit initializes the addon.
func (p *ExtensionAddon) OnInit(
	tenEnv TenEnv,
) {
	tenEnv.OnInitDone()
}

// OnDeinit deinitializes the addon.
func (p *ExtensionAddon) OnDeinit(tenEnv TenEnv) {
	tenEnv.OnDeinitDone()
}

// OnCreateInstance creates an instance of the extension.
func (p *ExtensionAddon) OnCreateInstance(
	tenEnv TenEnv,
	name string,
	context uintptr,
) {
	if p.constructor == nil {
		panic("Extension constructor is not provided")
	}

	var extWrapper Extension = nil

	ext := p.constructor(name)
	if ext != nil {
		extWrapper = WrapExtension(ext, name)
	} else {
		tenEnv.LogError("Failed to create extension " + name)
	}

	tenEnv.OnCreateInstanceDone(extWrapper, context)
}

// TODO(Liu): move this definition to a internal package.
type addon struct {
	Addon

	baseTenObject[C.uintptr_t]
}

// NewDefaultExtensionAddon creates a new default extension addon.
func NewDefaultExtensionAddon(constructor ExtensionConstructor) Addon {
	return &ExtensionAddon{
		constructor: constructor,
	}
}

//export tenGoAddonOnInit
func tenGoAddonOnInit(
	addonID C.uintptr_t,
	tenEnvID C.uintptr_t,
) {
	addonObj, ok := loadImmutableHandle(goHandle(addonID)).(*addon)
	if !ok {
		panic(
			fmt.Sprintf(
				"Failed to get addon from handle map, id: %d.",
				uintptr(addonID),
			),
		)
	}

	tenEnvObj, ok := handle(tenEnvID).get().(TenEnv)
	if !ok {
		panic(
			fmt.Sprintf(
				"Failed to get ten env from handle map, id: %d.",
				uintptr(tenEnvID),
			),
		)
	}

	addonObj.OnInit(tenEnvObj)
}

//export tenGoAddonOnDeinit
func tenGoAddonOnDeinit(addonID C.uintptr_t, tenEnvID C.uintptr_t) {
	addonObj, ok := loadImmutableHandle(goHandle(addonID)).(*addon)
	if !ok {
		panic(
			fmt.Sprintf(
				"Failed to get addon from handle map, id: %d.",
				uintptr(addonID),
			),
		)
	}

	tenEnvObj, ok := handle(tenEnvID).get().(TenEnv)
	if !ok {
		panic(
			fmt.Sprintf(
				"Failed to get ten env from handle map, id: %d.",
				uintptr(tenEnvID),
			),
		)
	}

	addonObj.OnDeinit(tenEnvObj)
}

//export tenGoAddonCreateInstance
func tenGoAddonCreateInstance(
	addonID C.uintptr_t,
	tenEnvID C.uintptr_t,
	name *C.char,
	context C.uintptr_t,
) {
	addonObj, ok := loadImmutableHandle(goHandle(addonID)).(*addon)
	if !ok {
		panic(
			fmt.Sprintf(
				"Failed to get addon from handle map, id: %d.",
				uintptr(addonID),
			),
		)
	}

	tenEnvObj, ok := handle(tenEnvID).get().(TenEnv)
	if !ok {
		panic(
			fmt.Sprintf(
				"Failed to get ten env from handle map, id: %d.",
				uintptr(tenEnvID),
			),
		)
	}

	extensionName := C.GoString(name)
	addonObj.OnCreateInstance(tenEnvObj, extensionName, uintptr(context))
}

//export tenGoAddonDestroyInstance
func tenGoAddonDestroyInstance(
	instanceID C.uintptr_t,
) {
	obj := loadAndDeleteImmutableHandle(goHandle(instanceID))
	if obj == nil {
		panic(
			fmt.Sprintf(
				"Failed to find addon instance from handle map, id: %d.",
				uintptr(instanceID),
			),
		)
	}
}

//export tenGoAddonOnDestroy
func tenGoAddonOnDestroy(
	addonID C.uintptr_t,
) {
	obj := loadAndDeleteImmutableHandle(goHandle(addonID))
	if obj == nil {
		panic(
			fmt.Sprintf(
				"Failed to find addon from handle map, id: %d.",
				uintptr(addonID),
			),
		)
	}
}
