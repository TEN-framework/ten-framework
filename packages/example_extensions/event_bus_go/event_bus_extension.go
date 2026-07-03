package extension

import (
	"encoding/json"
	"fmt"

	ten "ten_framework/ten_runtime"

	"dario.cat/mergo"
)

const (
	cmdTenEvent = "ten_event"
)

type eventBusExtension struct {
	ten.DefaultExtension
}

func newEventBusExtension(name string) ten.Extension {
	return &eventBusExtension{}
}

func (p *eventBusExtension) OnInit(tenEnv ten.TenEnv) {
	tenEnv.LogInfo("OnInit")

	tenEnv.OnInitDone()
}

func (p *eventBusExtension) OnCmd(
	tenEnv ten.TenEnv,
	cmd ten.Cmd,
) {
	cmdName, err := cmd.GetName()
	if err != nil {
		tenEnv.LogError(fmt.Sprintf("OnCmd get name failed, err: %v", err))
		cmdResult, _ := ten.NewCmdResult(ten.StatusCodeError, cmd)
		tenEnv.ReturnResult(cmdResult, func(tenEnv ten.TenEnv, err error) {
			if err != nil {
				tenEnv.LogError(
					fmt.Sprintf("ReturnResult failed, err: %v", err),
				)
			}
		})
		return
	}

	switch cmdName {
	case cmdTenEvent:
		tenEnv.LogDebug(fmt.Sprintf("forward cmd %s", cmdName))

		cmdResult, _ := ten.NewCmdResult(ten.StatusCodeOk, cmd)
		cmdCopy, _ := cmd.Clone()
		cmdResultMerged := make(map[string]interface{})

		hasReturnedResult := false
		if err := tenEnv.SendCmdEx(cmdCopy, func(te ten.TenEnv, cr ten.CmdResult, er error) {
			if hasReturnedResult {
				// If the result has been returned, do not return it again
				tenEnv.LogDebug(fmt.Sprintf("cmd %s has returned result, do not return it again", cmdName))
				return
			}

			if er != nil {
				ten_err, ok := er.(*ten.TenError)
				if !ok {
					tenEnv.LogError(fmt.Sprintf("send cmd %s failed, err: %v", cmdName, er))
					cmdResult, _ := ten.NewCmdResult(ten.StatusCodeError, cmd)
					tenEnv.ReturnResult(cmdResult, func(tenEnv ten.TenEnv, err error) {
						if err != nil {
							tenEnv.LogError(fmt.Sprintf("ReturnResult failed, err: %v", err))
						}
					})
					hasReturnedResult = true
					return
				}
				if ten_err.ErrorCode == ten.ErrorCodeMsgNotConnected {
					tenEnv.LogInfo(fmt.Sprintf("cmd %s not connected", cmdName))
					tenEnv.ReturnResult(cmdResult, func(tenEnv ten.TenEnv, err error) {
						if err != nil {
							tenEnv.LogError(fmt.Sprintf("ReturnResult failed, err: %v", err))
						}
					})
					hasReturnedResult = true
					return
				} else {
					tenEnv.LogError(fmt.Sprintf("send cmd %s failed, err: %v", cmdName, er))
					cmdResult, _ := ten.NewCmdResult(ten.StatusCodeError, cmd)
					tenEnv.ReturnResult(cmdResult, func(tenEnv ten.TenEnv, err error) {
						if err != nil {
							tenEnv.LogError(fmt.Sprintf("ReturnResult failed, err: %v", err))
						}
					})
					hasReturnedResult = true
					return
				}
			} else {
				// Default: Return the merged result
				// If any non-OK status is received, immediately return the cmd.
				// Otherwise, return the merged result
				code, err := cr.GetStatusCode()
				if err != nil {
					tenEnv.LogError(fmt.Sprintf("get status code failed, err: %v", err))
					cmdResult, _ := ten.NewCmdResult(ten.StatusCodeError, cmd)
					tenEnv.ReturnResult(cmdResult, func(tenEnv ten.TenEnv, err error) {
						if err != nil {
							tenEnv.LogError(fmt.Sprintf("ReturnResult failed, err: %v", err))
						}
					})
					hasReturnedResult = true
					return
				}
				result, err := cr.GetPropertyToJSONBytes("")
				if err != nil {
					tenEnv.LogError(fmt.Sprintf("get property failed, err: %v", err))
					cmdResult, _ := ten.NewCmdResult(ten.StatusCodeError, cmd)
					tenEnv.ReturnResult(cmdResult, func(tenEnv ten.TenEnv, err error) {
						if err != nil {
							tenEnv.LogError(fmt.Sprintf("ReturnResult failed, err: %v", err))
						}
					})
					hasReturnedResult = true
					return
				}
				tenEnv.LogDebug(fmt.Sprintf("send cmd %s done, status: %d, result: %s", cmdName, code, string(result)))

				if code != ten.StatusCodeOk {
					tenEnv.ReturnResult(cmdResult, func(tenEnv ten.TenEnv, err error) {
						if err != nil {
							tenEnv.LogError(fmt.Sprintf("ReturnResult failed, err: %v", err))
						}
					})
					hasReturnedResult = true
					return
				}

				cmdResultMap := make(map[string]interface{})
				if err := json.Unmarshal(result, &cmdResultMap); err != nil {
					tenEnv.LogError(fmt.Sprintf("unmarshal cmdResult failed, err: %v", err))
					cmdResult, _ := ten.NewCmdResult(ten.StatusCodeError, cmd)
					tenEnv.ReturnResult(cmdResult, func(tenEnv ten.TenEnv, err error) {
						if err != nil {
							tenEnv.LogError(fmt.Sprintf("ReturnResult failed, err: %v", err))
						}
					})
					hasReturnedResult = true
					return
				}
				err = mergo.Merge(&cmdResultMerged, cmdResultMap, mergo.WithOverride)
				if err != nil {
					tenEnv.LogError(fmt.Sprintf("merge cmdResult failed, err: %v", err))
					cmdResult, _ := ten.NewCmdResult(ten.StatusCodeError, cmd)
					tenEnv.ReturnResult(cmdResult, func(tenEnv ten.TenEnv, err error) {
						if err != nil {
							tenEnv.LogError(fmt.Sprintf("ReturnResult failed, err: %v", err))
						}
					})
					return
				}
				isCompleted, _ := cr.IsCompleted()
				if isCompleted {
					cmdResultMergedBytes, err := json.Marshal(cmdResultMerged)
					if err != nil {
						tenEnv.LogError(fmt.Sprintf("marshal cmdResultMerged failed, err: %v", err))
						cmdResult, _ := ten.NewCmdResult(ten.StatusCodeError, cmd)
						tenEnv.ReturnResult(cmdResult, func(tenEnv ten.TenEnv, err error) {
							if err != nil {
								tenEnv.LogError(fmt.Sprintf("ReturnResult failed, err: %v", err))
							}
						})
						hasReturnedResult = true
						return
					}

					cmdResult.SetPropertyFromJSONBytes("", cmdResultMergedBytes)
					tenEnv.LogDebug(fmt.Sprintf("cmd %s completed, result: %s", cmdName, string(cmdResultMergedBytes)))
					tenEnv.ReturnResult(cmdResult, func(tenEnv ten.TenEnv, err error) {
						if err != nil {
							tenEnv.LogError(fmt.Sprintf("ReturnResult failed, err: %v", err))
						}
					})
					hasReturnedResult = true
					return
				}
			}
		}); err != nil {
			tenEnv.LogError(
				fmt.Sprintf("send cmd %s failed, err: %v", cmdName, err),
			)
			cmdResult, _ := ten.NewCmdResult(ten.StatusCodeError, cmd)
			tenEnv.ReturnResult(cmdResult, func(tenEnv ten.TenEnv, err error) {
				if err != nil {
					tenEnv.LogError(
						fmt.Sprintf("ReturnResult failed, err: %v", err),
					)
				}
			})
			hasReturnedResult = true
			return
		}

	default:
		tenEnv.LogWarn(fmt.Sprintf("unsupported cmd %s", cmdName))
		cmdResult, _ := ten.NewCmdResult(ten.StatusCodeError, cmd)
		tenEnv.ReturnResult(cmdResult, func(tenEnv ten.TenEnv, err error) {
			if err != nil {
				tenEnv.LogError(
					fmt.Sprintf("ReturnResult failed, err: %v", err),
				)
			}
		})
	}
}

func init() {
	// Register addon
	ten.RegisterAddonAsExtension(
		"event_bus_go",
		ten.NewDefaultExtensionAddon(newEventBusExtension),
	)
}
