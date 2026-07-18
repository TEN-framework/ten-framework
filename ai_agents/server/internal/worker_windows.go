//go:build windows
// +build windows

package internal

import (
	"log/slog"
	"os"
	"os/exec"
	"syscall"
	"time"
)

func (w *Worker) start(req *StartReq) (err error) {
	// Use separate arguments to avoid shell injection
	slog.Info("Worker start", "requestId", req.RequestId, "property", w.PropertyJsonFile, "tenappDir", w.TenappDir, logTag)

	// Use tman run start to be consistent with Linux and support different tenapp structures
	cmd := exec.Command("tman", "run", "start", "--", "--property", w.PropertyJsonFile)

	// Windows: Create a new process group using CREATE_NEW_PROCESS_GROUP
	cmd.SysProcAttr = &syscall.SysProcAttr{
		CreationFlags: syscall.CREATE_NEW_PROCESS_GROUP,
	}

	// Set working directory if tenapp_dir is specified
	if w.TenappDir != "" {
		cmd.Dir = w.TenappDir
		slog.Info("Worker start with tenapp_dir", "requestId", req.RequestId, "tenappDir", w.TenappDir, logTag)
	}

	var stdoutWriter, stderrWriter = os.Stdout, os.Stderr
	var logFile *os.File

	if !w.Log2Stdout {
		// Open the log file for writing
		var openErr error
		logFile, openErr = os.OpenFile(w.LogFile, os.O_APPEND|os.O_CREATE|os.O_WRONLY, 0644)
		if openErr != nil {
			slog.Error("Failed to open log file", "err", openErr, "requestId", req.RequestId, logTag)
		} else {
			stdoutWriter = logFile
			stderrWriter = logFile
		}
	}

	// Create PrefixWriter instances with appropriate writers
	stdoutPrefixWriter := &PrefixWriter{
		prefix: "-", // Initial prefix, will update after process starts
		writer: stdoutWriter,
	}
	stderrPrefixWriter := &PrefixWriter{
		prefix: "-", // Initial prefix, will update after process starts
		writer: stderrWriter,
	}

	cmd.Stdout = stdoutPrefixWriter
	cmd.Stderr = stderrPrefixWriter

	if err = cmd.Start(); err != nil {
		slog.Error("Worker start failed", "err", err, "requestId", req.RequestId, logTag)
		return
	}

	pid := cmd.Process.Pid

	// Update the prefix with the actual PID
	stdoutPrefixWriter.prefix = w.ChannelName
	stderrPrefixWriter.prefix = w.ChannelName
	w.Pid = pid

	// Monitor the background process in a separate goroutine
	go func() {
		err := cmd.Wait() // Wait for the command to exit
		if err != nil {
			slog.Error("Worker process failed", "err", err, "requestId", req.RequestId, logTag)
		} else {
			slog.Info("Worker process completed successfully", "requestId", req.RequestId, logTag)
		}
		// Close the log file when the command finishes
		if logFile != nil {
			logFile.Close()
		}

		// Remove the worker from the map (defensive check for concurrent stop)
		if workers.Contains(w.ChannelName) {
			workers.Remove(w.ChannelName)
		}

	}()

	return
}

func (w *Worker) stop(requestId string, channelName string) (err error) {
	slog.Info("Worker stop start", "channelName", channelName, "requestId", requestId, "pid", w.Pid, logTag)

	// Windows: Use TerminateProcess to kill the process
	// Get the process handle
	handle, err := syscall.OpenProcess(syscall.PROCESS_TERMINATE, false, uint32(w.Pid))
	if err != nil {
		slog.Error("Worker open process failed", "err", err, "channelName", channelName, "pid", w.Pid, "requestId", requestId, logTag)
		if workers.Contains(channelName) {
			workers.Remove(channelName)
		}
		return
	}
	defer syscall.CloseHandle(handle)

	// Try graceful shutdown first by waiting a bit
	// Note: Windows doesn't have a direct equivalent to SIGTERM
	// We'll just wait a bit before force killing
	for i := 0; i < 5; i++ {
		// Check if process is still running
		var exitCode uint32
		err = syscall.GetExitCodeProcess(handle, &exitCode)
		if err != nil || exitCode != 259 { // 259 = STILL_ACTIVE
			// Process no longer exists
			if workers.Contains(channelName) {
				workers.Remove(channelName)
			}
			slog.Info("Worker stop end (process already exited)", "channelName", channelName, "worker", w, "requestId", requestId, logTag)
			return nil
		}
		time.Sleep(200 * time.Millisecond)
	}

	// Force kill the process
	slog.Warn("Worker force killing process", "channelName", channelName, "requestId", requestId, logTag)
	err = syscall.TerminateProcess(handle, 1)
	if err != nil {
		slog.Error("Worker TerminateProcess failed", "err", err, "channelName", channelName, "worker", w, "requestId", requestId, logTag)
		return
	}

	if workers.Contains(channelName) {
		workers.Remove(channelName)
	}
	slog.Info("Worker stop end (forced)", "channelName", channelName, "worker", w, "requestId", requestId, logTag)
	return
}

// Windows version: Get running worker PIDs (simplified)
func getRunningWorkerPIDs() map[int]struct{} {
	// On Windows, we'll rely on the workers map instead of ps command
	// This is a simplified implementation
	runningPIDs := make(map[int]struct{})
	for _, channelName := range workers.Keys() {
		worker := workers.Get(channelName).(*Worker)
		runningPIDs[worker.Pid] = struct{}{}
	}
	return runningPIDs
}

// Windows version: Kill a process by PID
func killProcess(pid int) {
	handle, err := syscall.OpenProcess(syscall.PROCESS_TERMINATE, false, uint32(pid))
	if err != nil {
		slog.Info("Failed to open process", "pid", pid, "error", err)
		return
	}
	defer syscall.CloseHandle(handle)

	err = syscall.TerminateProcess(handle, 1)
	if err != nil {
		slog.Info("Failed to kill process", "pid", pid, "error", err)
	} else {
		slog.Info("Successfully killed process", "pid", pid)
	}
}
