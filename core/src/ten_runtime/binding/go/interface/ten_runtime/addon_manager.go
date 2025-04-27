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
	"runtime"
	"sync"
	"unsafe"
)

// AddonManager is a manager for addons.
type AddonManager struct {
	// Define a registry map to store addon registration functions.
	// The key is the addonName (string), and the value is a function that takes
	// a registerCtx (cHandle) and returns an error.
	registry map[string]func(cHandle) error

	registryMutex sync.RWMutex
}

func newAddonManager() *AddonManager {
	return &AddonManager{
		registry: make(map[string]func(cHandle) error),
	}
}

// RegisterAddonAsExtension registers the addon as an extension.
func (am *AddonManager) RegisterAddonAsExtension(
	addonName string,
	instance Addon,
) error {
	if len(addonName) == 0 {
		return newTenError(
			ErrorCodeInvalidArgument,
			"addon name is empty",
		)
	}

	// Define the registration function that will be stored in the registry.
	registerHandler := func(registerCtx cHandle) error {
		addonWrapper := &addon{
			Addon: instance,
		}

		addonID := newImmutableHandle(addonWrapper)

		var bridge C.uintptr_t
		cgoError := C.ten_go_addon_register_extension(
			unsafe.Pointer(unsafe.StringData(addonName)),
			C.int(len(addonName)),
			cHandle(addonID),
			registerCtx,
			&bridge,
		)

		if err := withCGoError(&cgoError); err != nil {
			loadAndDeleteImmutableHandle(addonID)
			return err
		}

		addonWrapper.cPtr = bridge

		runtime.SetFinalizer(addonWrapper, func(p *addon) {
			C.ten_go_addon_finalize(p.cPtr)
		})

		return nil
	}

	// Store the registration function in the registry map.
	am.registryMutex.Lock()
	defer am.registryMutex.Unlock()

	if _, exists := am.registry[addonName]; exists {
		return newTenError(
			ErrorCodeInvalidArgument,
			fmt.Sprintf("Addon '%s' is already registered", addonName),
		)
	}

	am.registry[addonName] = registerHandler

	// Register the addon to the native addon manager.
	C.ten_go_addon_manager_add_extension_addon(
		unsafe.Pointer(unsafe.StringData(addonName)),
		C.int(len(addonName)),
	)

	return nil
}

var defaultAddonManager = newAddonManager()

// RegisterAddonAsExtension registers the addon as an extension.
func RegisterAddonAsExtension(addonName string, instance Addon) error {
	return defaultAddonManager.RegisterAddonAsExtension(addonName, instance)
}

//export tenGoAddonManagerCallRegisterHandler
func tenGoAddonManagerCallRegisterHandler(
	addonType C.int,
	addonName *C.char,
	registerCtx C.uintptr_t,
) {
	defaultAddonManager.registryMutex.RLock()
	defer defaultAddonManager.registryMutex.RUnlock()

	registerHandler, exists := defaultAddonManager.registry[C.GoString(addonName)]

	if !exists {
		panic(
			fmt.Sprintf("Addon '%s' is not registered", C.GoString(addonName)),
		)
	}

	registerHandler(cHandle(registerCtx))
}
