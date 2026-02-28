//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

package ten_runtime

// #include "cmd_trigger_life_cycle.h"
import "C"

import (
	"unsafe"
)

// TriggerLifeCycleCmd is the interface for the trigger life cycle command.
type TriggerLifeCycleCmd interface {
	Cmd

	SetStage(stage string) error
}

// NewTriggerLifeCycleCmd creates a new trigger life cycle command.
func NewTriggerLifeCycleCmd() (TriggerLifeCycleCmd, error) {
	var bridge C.uintptr_t
	err := withCGOLimiter(func() error {
		cStatus := C.ten_go_cmd_create_trigger_life_cycle_cmd(
			&bridge,
		)
		e := withCGoError(&cStatus)

		return e
	})
	if err != nil {
		return nil, err
	}

	return newTriggerLifeCycleCmd(bridge), nil
}

type triggerLifeCycleCmd struct {
	*cmd
}

func newTriggerLifeCycleCmd(bridge C.uintptr_t) *triggerLifeCycleCmd {
	return &triggerLifeCycleCmd{
		cmd: newCmd(bridge),
	}
}

func (p *triggerLifeCycleCmd) SetStage(stage string) error {
	defer p.keepAlive()

	err := withCGOLimiter(func() error {
		apiStatus := C.ten_go_cmd_trigger_life_cycle_set_stage(
			p.getCPtr(),
			unsafe.Pointer(unsafe.StringData(stage)),
			C.int(len(stage)),
		)
		return withCGoError(&apiStatus)
	})

	return err
}

var _ TriggerLifeCycleCmd = new(triggerLifeCycleCmd)
