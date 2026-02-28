//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

package ten_runtime

// #include "cmd_stop_graph.h"
import "C"

import (
	"unsafe"
)

// StopGraphCmd is the interface for the stop graph command.
type StopGraphCmd interface {
	Cmd

	SetGraphID(graphID string) error
}

// NewStopGraphCmd creates a new stop graph command.
func NewStopGraphCmd() (StopGraphCmd, error) {
	var bridge C.uintptr_t
	err := withCGOLimiter(func() error {
		cStatus := C.ten_go_cmd_create_stop_graph_cmd(
			&bridge,
		)
		e := withCGoError(&cStatus)

		return e
	})
	if err != nil {
		return nil, err
	}

	return newStopGraphCmd(bridge), nil
}

type stopGraphCmd struct {
	*cmd
}

func newStopGraphCmd(bridge C.uintptr_t) *stopGraphCmd {
	return &stopGraphCmd{
		cmd: newCmd(bridge),
	}
}

func (p *stopGraphCmd) SetGraphID(graphID string) error {
	defer p.keepAlive()

	err := withCGOLimiter(func() error {
		apiStatus := C.ten_go_cmd_stop_graph_set_graph_id(
			p.getCPtr(),
			unsafe.Pointer(unsafe.StringData(graphID)),
			C.int(len(graphID)),
		)
		return withCGoError(&apiStatus)
	})

	return err
}

var _ StopGraphCmd = new(stopGraphCmd)
