//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

package ten_runtime

// #include "cmd_start_graph.h"
import "C"

import (
	"unsafe"
)

// StartGraphCmd is the interface for the start graph command.
type StartGraphCmd interface {
	Cmd

	SetPredefinedGraphName(predefinedGraphName string) error
	SetGraphFromJSONBytes(graphJSONBytes []byte) error
	SetLongRunningMode(longRunningMode bool) error
}

// NewStartGraphCmd creates a new start graph command.
func NewStartGraphCmd() (StartGraphCmd, error) {
	var bridge C.uintptr_t
	err := withCGOLimiter(func() error {
		cStatus := C.ten_go_cmd_create_start_graph_cmd(
			&bridge,
		)
		e := withCGoError(&cStatus)

		return e
	})
	if err != nil {
		return nil, err
	}

	return newStartGraphCmd(bridge), nil
}

type startGraphCmd struct {
	*cmd
}

func newStartGraphCmd(bridge C.uintptr_t) *startGraphCmd {
	return &startGraphCmd{
		cmd: newCmd(bridge),
	}
}

func (p *startGraphCmd) SetPredefinedGraphName(
	predefinedGraphName string,
) error {
	defer p.keepAlive()

	err := withCGOLimiter(func() error {
		apiStatus := C.ten_go_cmd_start_graph_set_predefined_graph_name(
			p.getCPtr(),
			unsafe.Pointer(unsafe.StringData(predefinedGraphName)),
			C.int(len(predefinedGraphName)),
		)
		return withCGoError(&apiStatus)
	})

	return err
}

func (p *startGraphCmd) SetGraphFromJSONBytes(graphJSONBytes []byte) error {
	defer p.keepAlive()

	err := withCGOLimiter(func() error {
		apiStatus := C.ten_go_cmd_start_graph_set_graph_from_json_bytes(
			p.getCPtr(),
			unsafe.Pointer(unsafe.SliceData(graphJSONBytes)),
			C.int(len(graphJSONBytes)),
		)
		return withCGoError(&apiStatus)
	})

	return err
}

func (p *startGraphCmd) SetLongRunningMode(longRunningMode bool) error {
	defer p.keepAlive()

	err := withCGOLimiter(func() error {
		apiStatus := C.ten_go_cmd_start_graph_set_long_running_mode(
			p.getCPtr(),
			C.bool(longRunningMode),
		)
		return withCGoError(&apiStatus)
	})

	return err
}

var _ StartGraphCmd = new(startGraphCmd)
