package internal

import (
	"bufio"
	"bytes"
	"fmt"
	"io"
	"log/slog"
	"net/http"
	"os"
	"os/exec"
	"strconv"
	"strings"
	"sync/atomic"
	"syscall"
	"time"

	"github.com/go-resty/resty/v2"
	"github.com/gogf/gf/container/gmap"
	"github.com/google/uuid"
)

type Worker struct {
	ChannelName        string
	HttpServerPort     int32
	LogFile            string
	Log2Stdout         bool
	PropertyJsonFile   string
	GraphName          string // New field to store the graphName
	TenappDir          string // Base directory for tman run start
	Pid                int
	QuitTimeoutSeconds int
	CreateTs           int64
	UpdateTs           int64
}

type WorkerUpdateReq struct {
	RequestId   string              `form:"request_id,omitempty" json:"request_id,omitempty"`
	ChannelName string              `form:"channel_name,omitempty" json:"channel_name,omitempty"`
	Collection  string              `form:"collection,omitempty" json:"collection"`
	FileName    string              `form:"filename,omitempty" json:"filename"`
	Path        string              `form:"path,omitempty" json:"path,omitempty"`
	Ten         *WorkerUpdateReqTen `form:"ten,omitempty" json:"ten,omitempty"`
}

type WorkerUpdateReqTen struct {
	Name string `form:"name,omitempty" json:"name,omitempty"`
	Type string `form:"type,omitempty" json:"type,omitempty"`
}

const (
	workerCleanSleepSeconds = 5
	workerExec              = "/app/agents/bin/start"
	workerHttpServerUrl     = "http://127.0.0.1"
)

var (
	workers           = gmap.New(true)
	httpServerPort    = httpServerPortMin
	httpServerPortMin = int32(10000)
	httpServerPortMax = int32(30000)
)

func newWorker(channelName string, logFile string, log2Stdout bool, propertyJsonFile string, tenappDir string) *Worker {
	nowTs := time.Now().Unix()
	worker := &Worker{
		ChannelName:        channelName,
		LogFile:            logFile,
		Log2Stdout:         log2Stdout,
		PropertyJsonFile:   propertyJsonFile,
		TenappDir:          tenappDir,
		QuitTimeoutSeconds: 60,
		CreateTs:           nowTs,
		UpdateTs:           nowTs,
	}

	slog.Info("Worker created",
		"channelName", channelName,
		"createTs", nowTs,
		"updateTs", nowTs,
		"quitTimeoutSeconds", 60,
		"timeoutAt", nowTs+60,
		logTag)

	return worker
}

func getHttpServerPort() int32 {
	if atomic.LoadInt32(&httpServerPort) > httpServerPortMax {
		atomic.StoreInt32(&httpServerPort, httpServerPortMin)
	}

	atomic.AddInt32(&httpServerPort, 1)
	return httpServerPort
}

// PrefixWriter is a custom writer that prefixes each line with a PID.
type PrefixWriter struct {
	prefix string
	writer io.Writer
}

// Write implements the io.Writer interface.
func (pw *PrefixWriter) Write(p []byte) (n int, err error) {
	// Create a scanner to split input into lines
	scanner := bufio.NewScanner(strings.NewReader(string(p)))
	var totalWritten int

	for scanner.Scan() {
		// Prefix each line with the provided prefix
		line := fmt.Sprintf("[%s] %s", pw.prefix, scanner.Text())
		// Write the prefixed line to the underlying writer
		n, err := pw.writer.Write([]byte(line + "\n"))
		totalWritten += n

		if err != nil {
			return totalWritten, err
		}
	}

	// Check if the scanner encountered any error
	if err := scanner.Err(); err != nil {
		return totalWritten, err
	}

	return len(p), nil
}

// Function to check if a PID is in the correct process group
func isInProcessGroup(pid, pgid int) bool {
	actualPgid, err := syscall.Getpgid(pid)
	if err != nil {
		// If an error occurs, the process might not exist anymore
		return false
	}
	return actualPgid == pgid
}

func (w *Worker) start(req *StartReq) (err error) {
	// Use exec.Command with separate arguments to avoid shell injection vulnerabilities
	// PropertyJsonFile could contain shell metacharacters if user-controlled
	cmd := exec.Command("tman", "run", "start", "--", "--property", w.PropertyJsonFile)
	slog.Info("Worker start", "requestId", req.RequestId, "command", "tman run start", "property", w.PropertyJsonFile, "tenappDir", w.TenappDir, logTag)
	cmd.SysProcAttr = &syscall.SysProcAttr{
		Setpgid: true, // Start a new process group
	}

	// Set working directory if tenapp_dir is specified
	if w.TenappDir != "" {
		cmd.Dir = w.TenappDir
		slog.Info("Worker start with tenapp_dir", "requestId", req.RequestId, "tenappDir", w.TenappDir, logTag)
	}

	var stdoutWriter, stderrWriter io.Writer
	var logFile *os.File

	if w.Log2Stdout {
		// Write logs to stdout and stderr
		stdoutWriter = os.Stdout
		stderrWriter = os.Stderr
	} else {
		// Open the log file for writing
		logFile, err := os.OpenFile(w.LogFile, os.O_APPEND|os.O_CREATE|os.O_WRONLY, 0644)
		if err != nil {
			slog.Error("Failed to open log file", "err", err, "requestId", req.RequestId, logTag)
			// return err
		}

		// Write logs to the log file
		stdoutWriter = logFile
		stderrWriter = logFile
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

	// Ensure the process has fully started
	// Note: pgrep with pid is safe (pid is an integer, not user input)
	pgrepCmd := fmt.Sprintf("pgrep -P %d", pid)
	slog.Info("Worker get pid", "requestId", req.RequestId, "command", pgrepCmd, logTag)

	var subprocessPid int
	for i := 0; i < 10; i++ { // retry for 3 times
		output, err := exec.Command("sh", "-c", pgrepCmd).CombinedOutput()
		if err == nil {
			subprocessPid, err = strconv.Atoi(strings.TrimSpace(string(output)))
			if err == nil && subprocessPid > 0 && isInProcessGroup(subprocessPid, cmd.Process.Pid) {
				break // if pid is successfully obtained, exit loop
			}
		}
		slog.Warn("Worker get pid failed, retrying...", "attempt", i+1, "pid", pid, "subpid", subprocessPid, "requestId", req.RequestId, logTag)
		time.Sleep(1000 * time.Millisecond) // wait for 500ms
	}

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

		// Remove the worker from the map
		workers.Remove(w.ChannelName)

	}()

	return
}

func (w *Worker) stop(requestId string, channelName string) (err error) {
	slog.Info("Worker stop start", "channelName", channelName, "requestId", requestId, "pid", w.Pid, logTag)

	// First try graceful shutdown with SIGTERM
	slog.Info("Worker sending SIGTERM", "channelName", channelName, "requestId", requestId, "pid", w.Pid, logTag)
	err = syscall.Kill(-w.Pid, syscall.SIGTERM)
	if err != nil {
		slog.Error("Worker SIGTERM failed", "err", err, "channelName", channelName, "worker", w, "requestId", requestId, logTag)
		// If SIGTERM fails, try SIGKILL immediately
		err = syscall.Kill(-w.Pid, syscall.SIGKILL)
		if err != nil {
			slog.Error("Worker SIGKILL failed", "err", err, "channelName", channelName, "worker", w, "requestId", requestId, logTag)
			return
		}
		workers.Remove(channelName)
		slog.Info("Worker stop end (SIGKILL after SIGTERM failure)", "channelName", channelName, "worker", w, "requestId", requestId, logTag)
		return
	}

	// Wait up to 2 seconds for graceful shutdown
	gracefulTimeout := 2
	for i := 0; i < gracefulTimeout*10; i++ {
		// Check if process still exists by sending signal 0
		err = syscall.Kill(-w.Pid, 0)
		if err != nil {
			// Process is gone, graceful shutdown succeeded
			slog.Info("Worker graceful shutdown succeeded", "channelName", channelName, "requestId", requestId, "pid", w.Pid, "waitTime", float64(i)*0.1, logTag)
			workers.Remove(channelName)
			slog.Info("Worker stop end (graceful)", "channelName", channelName, "worker", w, "requestId", requestId, logTag)
			return nil
		}
		time.Sleep(100 * time.Millisecond)
	}

	// Process still running after timeout, force kill with SIGKILL
	slog.Warn("Worker graceful shutdown timeout, sending SIGKILL", "channelName", channelName, "requestId", requestId, "pid", w.Pid, logTag)
	err = syscall.Kill(-w.Pid, syscall.SIGKILL)
	if err != nil {
		slog.Error("Worker SIGKILL failed", "err", err, "channelName", channelName, "worker", w, "requestId", requestId, logTag)
		return
	}

	workers.Remove(channelName)

	slog.Info("Worker stop end (forced SIGKILL)", "channelName", channelName, "worker", w, "requestId", requestId, logTag)
	return
}

func (w *Worker) update(req *WorkerUpdateReq) (err error) {
	slog.Info("Worker update start", "channelName", req.ChannelName, "requestId", req.RequestId, logTag)

	var res *resty.Response

	defer func() {
		if err != nil {
			slog.Error("Worker update error", "err", err, "channelName", req.ChannelName, "requestId", req.RequestId, logTag)
		}
	}()

	workerUpdateUrl := fmt.Sprintf("%s:%d/cmd", workerHttpServerUrl, w.HttpServerPort)
	res, err = HttpClient.R().
		SetHeader("Content-Type", "application/json").
		SetBody(req).
		Post(workerUpdateUrl)
	if err != nil {
		return
	}

	if res.StatusCode() != http.StatusOK {
		return fmt.Errorf("%s, status: %d", codeErrHttpStatusNotOk.msg, res.StatusCode())
	}

	slog.Info("Worker update end", "channelName", req.ChannelName, "worker", w, "requestId", req.RequestId, logTag)
	return
}

// Function to get the PIDs of running workers
func getRunningWorkerPIDs() map[int]struct{} {
	// Define the command to find processes
	cmd := exec.Command("sh", "-c", `ps aux | grep "bin/worker --property" | grep -v grep`)

	// Run the command and capture the output
	var out bytes.Buffer
	cmd.Stdout = &out
	err := cmd.Run()
	if err != nil {
		return nil
	}

	// Parse the PIDs from the output
	lines := strings.Split(out.String(), "\n")
	runningPIDs := make(map[int]struct{})
	for _, line := range lines {
		fields := strings.Fields(line)
		if len(fields) > 1 {
			pid, err := strconv.Atoi(fields[1]) // PID is typically the second field
			if err == nil {
				runningPIDs[pid] = struct{}{}
			}
		}
	}
	return runningPIDs
}

// Function to kill a process by PID
func killProcess(pid int) {
	err := syscall.Kill(pid, syscall.SIGKILL)
	if err != nil {
		slog.Info("Failed to kill process", "pid", pid, "error", err)
	} else {
		slog.Info("Successfully killed process", "pid", pid)
	}
}

func timeoutWorkers() {
	slog.Info("Worker timeout monitor started", "checkIntervalSeconds", workerCleanSleepSeconds, logTag)

	for {
		nowTs := time.Now().Unix()
		workerCount := len(workers.Keys())

		slog.Info("Worker timeout check starting",
			"nowTs", nowTs,
			"workerCount", workerCount,
			logTag)

		for _, channelName := range workers.Keys() {
			worker := workers.Get(channelName).(*Worker)

			// Skip workers with infinite timeout
			if worker.QuitTimeoutSeconds == WORKER_TIMEOUT_INFINITY {
				slog.Info("Worker has infinite timeout, skipping",
					"channelName", channelName,
					logTag)
				continue
			}

			timeoutAt := worker.UpdateTs + int64(worker.QuitTimeoutSeconds)
			ageSeconds := nowTs - worker.UpdateTs
			secondsUntilTimeout := timeoutAt - nowTs

			slog.Info("Worker timeout check",
				"channelName", channelName,
				"pid", worker.Pid,
				"ageSeconds", ageSeconds,
				"quitTimeoutSeconds", worker.QuitTimeoutSeconds,
				"updateTs", worker.UpdateTs,
				"timeoutAt", timeoutAt,
				"nowTs", nowTs,
				"secondsUntilTimeout", secondsUntilTimeout,
				logTag)

			if timeoutAt < nowTs {
				slog.Warn("Worker TIMEOUT EXCEEDED - stopping worker",
					"channelName", channelName,
					"pid", worker.Pid,
					"ageSeconds", ageSeconds,
					"exceededBySeconds", nowTs - timeoutAt,
					logTag)

				if err := worker.stop(uuid.New().String(), channelName.(string)); err != nil {
					slog.Error("Timeout worker stop failed", "err", err, "channelName", channelName, logTag)
					continue
				}

				slog.Info("Timeout worker stop success", "channelName", channelName, "worker", worker, "nowTs", nowTs, logTag)
			}
		}

		slog.Info("Worker timeout check complete",
			"nextCheckInSeconds", workerCleanSleepSeconds,
			logTag)
		time.Sleep(workerCleanSleepSeconds * time.Second)
	}
}

func CleanWorkers() {
	// Stop all workers
	for _, channelName := range workers.Keys() {
		worker := workers.Get(channelName).(*Worker)
		if err := worker.stop(uuid.New().String(), channelName.(string)); err != nil {
			slog.Error("Worker cleanWorker failed", "err", err, "channelName", channelName, logTag)
			continue
		}

		slog.Info("Worker cleanWorker success", "channelName", channelName, "worker", worker, logTag)
	}

	// Get running processes with the specific command pattern
	runningPIDs := getRunningWorkerPIDs()

	// Create maps for easy lookup
	workerMap := make(map[int]*Worker)
	for _, channelName := range workers.Keys() {
		worker := workers.Get(channelName).(*Worker)
		workerMap[worker.Pid] = worker
	}

	// Kill processes that are running but not in the workers list
	for pid := range runningPIDs {
		if _, exists := workerMap[pid]; !exists {
			slog.Info("Killing redundant process", "pid", pid)
			killProcess(pid)
		}
	}
}
