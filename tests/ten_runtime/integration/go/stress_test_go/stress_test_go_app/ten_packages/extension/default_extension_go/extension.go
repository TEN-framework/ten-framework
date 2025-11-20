//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

package default_extension_go

import (
	"fmt"
	"runtime"
	"strconv"
	"strings"
	"sync/atomic"
	"time"

	ten "ten_framework/ten_runtime"
)

// 全局计数器用于统计
var (
	cmdSentCount     int64
	cmdReceivedCount int64
	resultCount      int64
	callbackCount    int64
	startTime        time.Time
)

type serverExtension struct {
	ten.DefaultExtension

	returnOk      bool
	responseDelay int32 // 响应延迟（毫秒），用于增加 GC 压力
}

func (ext *serverExtension) OnInit(tenEnv ten.TenEnv) {
	var err error
	ext.returnOk, err = tenEnv.GetPropertyBool("return_ok")
	if err != nil {
		panic("Failed to get property: " + err.Error())
	}

	// 读取可选的响应延迟参数
	delay, err := tenEnv.GetPropertyInt32("response_delay")
	if err == nil {
		ext.responseDelay = delay
	}

	tenEnv.OnInitDone()
}

func (ext *serverExtension) OnCmd(tenEnv ten.TenEnv, cmd ten.Cmd) {
	atomic.AddInt64(&cmdReceivedCount, 1)

	// 可选延迟，增加对象在内存中的停留时间，提高 GC 触发概率
	if ext.responseDelay > 0 {
		time.Sleep(time.Duration(ext.responseDelay) * time.Millisecond)
	}

	// 读取 CMD 的一些属性（增加 CGO 调用频率）
	cmdName, _ := cmd.GetName()
	_ = cmdName

	// 创建响应
	var newCmdResult ten.CmdResult
	if ext.returnOk {
		newCmdResult, _ = ten.NewCmdResult(ten.StatusCodeOk, cmd)
	} else {
		newCmdResult, _ = ten.NewCmdResult(ten.StatusCodeError, cmd)
	}

	// 设置一些属性（增加内存分配和 CGO 调用）
	newCmdResult.SetPropertyString("timestamp", time.Now().Format(time.RFC3339Nano))
	newCmdResult.SetProperty("counter", atomic.LoadInt64(&cmdReceivedCount))

	tenEnv.ReturnResult(newCmdResult, nil)
}

type clientExtension struct {
	ten.DefaultExtension

	// 压力测试配置
	enableStressTest bool
	burstCount       int32
	burstInterval    int32 // 毫秒
	totalBursts      int32
	forceGC          bool  // 强制触发 GC
	concurrentSends  int32 // 并发发送数

	currentBurst int32
	stopChan     chan struct{}
}

func (ext *clientExtension) OnInit(tenEnv ten.TenEnv) {
	// 检查是否启用压力测试模式
	enableStress, err := tenEnv.GetPropertyBool("enable_stress_test")
	if err == nil && enableStress {
		ext.enableStressTest = true

		// 读取压力测试参数
		if count, err := tenEnv.GetPropertyInt32("burst_count"); err == nil {
			ext.burstCount = count
		} else {
			ext.burstCount = 100
		}

		if interval, err := tenEnv.GetPropertyInt32("burst_interval_ms"); err == nil {
			ext.burstInterval = interval
		} else {
			ext.burstInterval = 100
		}

		if total, err := tenEnv.GetPropertyInt32("total_bursts"); err == nil {
			ext.totalBursts = total
		} else {
			ext.totalBursts = 50
		}

		// 强制 GC 选项
		if forceGC, err := tenEnv.GetPropertyBool("force_gc"); err == nil {
			ext.forceGC = forceGC
		}

		// 并发发送数
		if concurrent, err := tenEnv.GetPropertyInt32("concurrent_sends"); err == nil {
			ext.concurrentSends = concurrent
			if ext.concurrentSends < 1 {
				ext.concurrentSends = 1
			}
		} else {
			ext.concurrentSends = 1
		}

		ext.stopChan = make(chan struct{})

		tenEnv.LogInfo(fmt.Sprintf(
			"[GC Stress Test] Enabled - burst_count=%d, interval=%dms, total=%d, force_gc=%v, concurrent=%d",
			ext.burstCount, ext.burstInterval, ext.totalBursts, ext.forceGC, ext.concurrentSends,
		))
	}

	tenEnv.OnInitDone()
}

func (ext *clientExtension) OnStart(tenEnv ten.TenEnv) {
	if ext.enableStressTest {
		tenEnv.LogInfo("[GC Stress Test] Starting stress test in background...")
		startTime = time.Now()
		go ext.runStressTest(tenEnv)
	}
	tenEnv.OnStartDone()
}

func (ext *clientExtension) OnStop(tenEnv ten.TenEnv) {
	if ext.enableStressTest && ext.stopChan != nil {
		close(ext.stopChan)
		ext.reportFinalStats(tenEnv)
	}
	tenEnv.OnStopDone()
}

func (ext *clientExtension) runStressTest(tenEnv ten.TenEnv) {
	ticker := time.NewTicker(time.Duration(ext.burstInterval) * time.Millisecond)
	defer ticker.Stop()

	for {
		select {
		case <-ext.stopChan:
			return
		case <-ticker.C:
			if ext.currentBurst >= ext.totalBursts {
				// 完成所有爆发，停止测试
				ext.reportFinalStats(tenEnv)

				time.Sleep(1 * time.Second)
				tenEnv.LogInfo("[GC Stress Test] All bursts completed, closing app...")

				// 发送停止命令
				closeCmd, _ := ten.NewCmd("close_app")
				tenEnv.SendCmd(closeCmd, nil)
				return
			}

			// 执行一次爆发
			ext.executeBurst(tenEnv)
			ext.currentBurst++

			// 定期报告统计
			if ext.currentBurst%10 == 0 || ext.currentBurst == ext.totalBursts {
				ext.reportStats(tenEnv)
			}
		}
	}
}

func (ext *clientExtension) executeBurst(tenEnv ten.TenEnv) {
	for i := int32(0); i < ext.burstCount; i++ {
		// 创建 CMD
		cmd, err := ten.NewCmd("test_stress")
		if err != nil {
			tenEnv.LogError(fmt.Sprintf("Failed to create cmd: %v", err))
			continue
		}

		// 设置属性（增加内存分配）
		cmd.SetProperty("burst_id", ext.currentBurst)
		cmd.SetProperty("cmd_id", i)
		cmd.SetPropertyString("timestamp", time.Now().Format(time.RFC3339Nano))

		// 使用 SendCmdEx 增加回调频率和 handle 操作
		err = tenEnv.SendCmdEx(cmd, ext.makeResultHandler(tenEnv))
		if err != nil {
			tenEnv.LogError(fmt.Sprintf("Failed to send cmd: %v", err))
		} else {
			atomic.AddInt64(&cmdSentCount, 1)
		}
	}
}

// makeResultHandler 创建结果处理函数
// ⚠️ 这里是关键：每次都创建新的 closure，增加 Go 对象分配
func (ext *clientExtension) makeResultHandler(tenEnv ten.TenEnv) func(ten.TenEnv, ten.CmdResult, error) {
	// 创建一些临时数据来增加内存压力
	tempData := make([]byte, 1024) // 1KB 临时数据
	_ = tempData

	return func(tenEnv ten.TenEnv, cmdResult ten.CmdResult, err error) {
		atomic.AddInt64(&callbackCount, 1)

		if err != nil {
			return
		}

		atomic.AddInt64(&resultCount, 1)

		// 读取属性（增加 CGO 调用频率）
		// ⚠️ 这些 CGO 调用正是触发漏洞的关键
		statusCode, _ := cmdResult.GetStatusCode()
		_, _ = cmdResult.GetPropertyString("timestamp")
		_, _ = cmdResult.GetPropertyInt64("counter")

		// ⚠️ 增加更多的属性读取来增加 CGO 调用
		_, _ = cmdResult.GetPropertyString("extra_data_1")
		_, _ = cmdResult.GetPropertyString("extra_data_2")
		_, _ = cmdResult.GetPropertyInt64("nano_time")
		_, _ = cmdResult.GetPropertyBool("test_flag")

		_ = statusCode

		// 检查是否完成
		completed, _ := cmdResult.IsCompleted()
		_ = completed

		// 创建更多临时对象增加 GC 压力
		_ = fmt.Sprintf("callback_%d", atomic.LoadInt64(&callbackCount))
	}
}

func (ext *clientExtension) reportStats(tenEnv ten.TenEnv) {
	elapsed := time.Since(startTime)
	sent := atomic.LoadInt64(&cmdSentCount)
	received := atomic.LoadInt64(&cmdReceivedCount)
	results := atomic.LoadInt64(&resultCount)
	callbacks := atomic.LoadInt64(&callbackCount)

	var m runtime.MemStats
	runtime.ReadMemStats(&m)

	tenEnv.LogInfo(fmt.Sprintf(
		"[GC Stress Stats] Burst: %d/%d | Sent: %d | Received: %d | Results: %d | Callbacks: %d | "+
			"Time: %v | QPS: %.0f | Heap: %.1fMB | GC: %d | NumGoroutine: %d",
		ext.currentBurst, ext.totalBursts,
		sent, received, results, callbacks,
		elapsed.Round(time.Millisecond),
		float64(sent)/elapsed.Seconds(),
		float64(m.HeapAlloc)/1024/1024,
		m.NumGC,
		runtime.NumGoroutine(),
	))
}

func (ext *clientExtension) reportFinalStats(tenEnv ten.TenEnv) {
	elapsed := time.Since(startTime)
	sent := atomic.LoadInt64(&cmdSentCount)
	received := atomic.LoadInt64(&cmdReceivedCount)
	results := atomic.LoadInt64(&resultCount)
	callbacks := atomic.LoadInt64(&callbackCount)

	var m runtime.MemStats
	runtime.ReadMemStats(&m)

	tenEnv.LogInfo(fmt.Sprintf(
		"[GC Stress FINAL] Total Commands - Sent: %d | Received: %d | Results: %d | Callbacks: %d",
		sent, received, results, callbacks,
	))
	tenEnv.LogInfo(fmt.Sprintf(
		"[GC Stress FINAL] Performance - Time: %v | QPS: %.0f",
		elapsed.Round(time.Millisecond),
		float64(sent)/elapsed.Seconds(),
	))
	tenEnv.LogInfo(fmt.Sprintf(
		"[GC Stress FINAL] Memory - Heap: %.1fMB | TotalAlloc: %.1fMB | Sys: %.1fMB | GC Runs: %d",
		float64(m.HeapAlloc)/1024/1024,
		float64(m.TotalAlloc)/1024/1024,
		float64(m.Sys)/1024/1024,
		m.NumGC,
	))
	tenEnv.LogInfo(fmt.Sprintf(
		"[GC Stress FINAL] Goroutines: %d | GC Pause Total: %v",
		runtime.NumGoroutine(),
		time.Duration(m.PauseTotalNs),
	))
}

func (ext *clientExtension) OnCmd(tenEnv ten.TenEnv, cmd ten.Cmd) {
	cmdName, _ := cmd.GetName()

	// 原有的 test 逻辑保持不变（用于原测试）
	if cmdName == "test" {
		receivedOkCount := 0
		receivedErrCount := 0

		newCmd, _ := ten.NewCmd("test")

		tenEnv.SendCmdEx(
			newCmd,
			func(tenEnv ten.TenEnv, cmdResult ten.CmdResult, err error) {
				if err != nil {
					panic("Failed to send cmd: " + err.Error())
				}

				statusCode, _ := cmdResult.GetStatusCode()
				if statusCode == ten.StatusCodeOk {
					receivedOkCount++
				} else {
					receivedErrCount++
				}

				completed, _ := cmdResult.IsCompleted()
				tenEnv.LogInfo("completed: " + strconv.FormatBool(completed))
				if completed {
					if receivedOkCount != 2 || receivedErrCount != 1 {
						panic("Invalid number of received ok or err" +
							"receivedOkCount: " + strconv.Itoa(receivedOkCount) +
							"receivedErrCount: " + strconv.Itoa(receivedErrCount))
					}

					newCmdResult, _ := ten.NewCmdResult(ten.StatusCodeOk, cmd)
					newCmdResult.SetPropertyString("detail", "ok")
					tenEnv.ReturnResult(newCmdResult, nil)
				}
			},
		)
		return
	}

	// 默认响应
	newCmdResult, _ := ten.NewCmdResult(ten.StatusCodeOk, cmd)
	tenEnv.ReturnResult(newCmdResult, nil)
}

func newAExtension(name string) ten.Extension {
	// if name starts with "server", return serverExtension
	if strings.HasPrefix(name, "server") {
		return &serverExtension{}
	} else if strings.HasPrefix(name, "client") {
		return &clientExtension{}
	}

	return nil
}

func init() {
	// Register addon.
	err := ten.RegisterAddonAsExtension(
		"default_extension_go",
		ten.NewDefaultExtensionAddon(newAExtension),
	)
	if err != nil {
		panic("Failed to register addon.")
	}
}
