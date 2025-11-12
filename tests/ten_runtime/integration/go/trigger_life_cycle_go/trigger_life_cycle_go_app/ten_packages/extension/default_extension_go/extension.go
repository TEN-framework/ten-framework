//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

package default_extension_go

import (
	ten "ten_framework/ten_runtime"
	"time"
)

type mainExtension struct {
	ten.DefaultExtension
}

func (p *mainExtension) OnInit(
	tenEnv ten.TenEnv,
) {
	go func() {
		// Sleep 1 second
		time.Sleep(1 * time.Second)

		// Send cmd to biz extension to check if it is started.
		cmd, _ := ten.NewCmd("check_start")
		tenEnv.SendCmd(cmd, func(te ten.TenEnv, cr ten.CmdResult, err error) {
			if err != nil {
				panic("Failed to send cmd: " + err.Error())
			}

			statusCode, _ := cr.GetStatusCode()
			if statusCode != ten.StatusCodeOk {
				panic("Failed to check start")
			}

			started, _ := cr.GetPropertyBool("started")
			if started {
				panic(
					"Biz extension should not be started as it has been set manual trigger life cycle",
				)
			}

			// Send manual trigger life cycle cmd to biz extension.
			lifeCycleCmd, _ := ten.NewTriggerLifeCycleCmd()
			lifeCycleCmd.SetStage("start")
			lifeCycleCmd.SetDests(ten.Loc{
				AppURI:        ten.Ptr(""),
				GraphID:       ten.Ptr(""),
				ExtensionName: ten.Ptr("biz"),
			})

			tenEnv.SendCmd(
				lifeCycleCmd,
				func(te ten.TenEnv, cr ten.CmdResult, err error) {
					if err != nil {
						panic("Failed to send cmd: " + err.Error())
					}

					statusCode, _ := cr.GetStatusCode()
					if statusCode != ten.StatusCodeOk {
						panic("Failed to send cmd: " + err.Error())
					}

					// Send cmd to biz extension to check if it is started.
					cmd, _ := ten.NewCmd("check_start")
					tenEnv.SendCmd(
						cmd,
						func(te ten.TenEnv, cr ten.CmdResult, err error) {
							if err != nil {
								panic("Failed to send cmd: " + err.Error())
							}

							statusCode, _ := cr.GetStatusCode()
							if statusCode != ten.StatusCodeOk {
								panic("Failed to check start")
							}

							started, _ := cr.GetPropertyBool("started")
							if !started {
								panic(
									"Biz extension should be started as it has received manual trigger life cycle cmd",
								)
							}

							tenEnv.OnInitDone()
						},
					)
				},
			)
		})
	}()
}

func (p *mainExtension) OnStop(
	tenEnv ten.TenEnv,
) {
	go func() {
		// Sleep 1 second
		time.Sleep(1 * time.Second)

		// Send cmd to biz extension to check if it is stopped.
		cmd, _ := ten.NewCmd("check_stop")
		tenEnv.SendCmd(cmd, func(te ten.TenEnv, cr ten.CmdResult, err error) {
			if err != nil {
				panic("Failed to send cmd: " + err.Error())
			}

			statusCode, _ := cr.GetStatusCode()
			if statusCode != ten.StatusCodeOk {
				panic("Failed to check stop")
			}

			stopped, _ := cr.GetPropertyBool("stopped")
			if stopped {
				panic(
					"Biz extension should not be stopped as it has been set manual trigger life cycle",
				)
			}

			// Send manual trigger life cycle cmd to biz extension.
			lifeCycleCmd, _ := ten.NewTriggerLifeCycleCmd()
			lifeCycleCmd.SetStage("stop")
			lifeCycleCmd.SetDests(ten.Loc{
				AppURI:        ten.Ptr(""),
				GraphID:       ten.Ptr(""),
				ExtensionName: ten.Ptr("biz"),
			})
			tenEnv.SendCmd(
				lifeCycleCmd,
				func(te ten.TenEnv, cr ten.CmdResult, err error) {
					if err != nil {
						panic("Failed to send cmd: " + err.Error())
					}

					statusCode, _ := cr.GetStatusCode()
					if statusCode != ten.StatusCodeOk {
						panic("Failed to send cmd: " + err.Error())
					}

					tenEnv.OnStopDone()
				},
			)
		})
	}()
}

func (p *mainExtension) OnCmd(
	tenEnv ten.TenEnv,
	cmd ten.Cmd,
) {
	cmdName, _ := cmd.GetName()
	if cmdName == "test" {
		cmdResult, _ := ten.NewCmdResult(ten.StatusCodeOk, cmd)
		cmdResult.SetPropertyString("detail", "ok")
		tenEnv.ReturnResult(cmdResult, nil)
	} else {
		cmdResult, _ := ten.NewCmdResult(ten.StatusCodeError, cmd)
		cmdResult.SetPropertyString("detail", "unknown command")
		tenEnv.ReturnResult(cmdResult, nil)
	}
}

type bizExtension struct {
	ten.DefaultExtension

	started bool
	stopped bool
}

func (p *bizExtension) OnStart(
	tenEnv ten.TenEnv,
) {
	p.started = true

	tenEnv.OnStartDone()
}

func (p *bizExtension) OnStop(
	tenEnv ten.TenEnv,
) {
	p.stopped = true

	tenEnv.OnStopDone()
}

func (p *bizExtension) OnCmd(
	tenEnv ten.TenEnv,
	cmd ten.Cmd,
) {
	cmdName, _ := cmd.GetName()
	switch cmdName {
	case "check_start":
		cmdResult, _ := ten.NewCmdResult(ten.StatusCodeOk, cmd)
		cmdResult.SetProperty("started", p.started)
		tenEnv.ReturnResult(cmdResult, nil)
	case "check_stop":
		cmdResult, _ := ten.NewCmdResult(ten.StatusCodeOk, cmd)
		cmdResult.SetProperty("stopped", p.stopped)
		tenEnv.ReturnResult(cmdResult, nil)
	default:
		cmdResult, _ := ten.NewCmdResult(ten.StatusCodeError, cmd)
		cmdResult.SetPropertyString("detail", "unknown command")
		tenEnv.ReturnResult(cmdResult, nil)
	}
}

func newDefaultExtension(name string) ten.Extension {
	if name == "main" {
		return &mainExtension{}
	}
	return &bizExtension{}
}

func init() {
	// Register addon.
	err := ten.RegisterAddonAsExtension(
		"default_extension_go",
		ten.NewDefaultExtensionAddon(newDefaultExtension),
	)
	if err != nil {
		panic("Failed to register addon.")
	}
}
