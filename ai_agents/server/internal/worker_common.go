package internal

import (
	"bufio"
	"fmt"
	"io"
	"log/slog"
	"net/http"
	"strings"
	"sync/atomic"
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
	workerHttpServerUrl     = "http://127.0.0.1"
)

var (
	workers           = gmap.New(true)
	httpServerPort    = httpServerPortMin
	httpServerPortMin = int32(10000)
	httpServerPortMax = int32(30000)
)

func newWorker(channelName string, logFile string, log2Stdout bool, propertyJsonFile string, tenappDir string) *Worker {
	return &Worker{
		ChannelName:        channelName,
		LogFile:            logFile,
		Log2Stdout:         log2Stdout,
		PropertyJsonFile:   propertyJsonFile,
		TenappDir:          tenappDir,
		QuitTimeoutSeconds: 60,
		CreateTs:           time.Now().Unix(),
		UpdateTs:           time.Now().Unix(),
	}
}

func getHttpServerPort() int32 {
	for {
		old := atomic.LoadInt32(&httpServerPort)
		new := old + 1
		if new > httpServerPortMax {
			new = httpServerPortMin
		}
		if atomic.CompareAndSwapInt32(&httpServerPort, old, new) {
			return new
		}
	}
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

// Platform-specific implementations are in worker_linux.go and worker_windows.go
// The start(), stop(), getRunningWorkerPIDs(), and killProcess() functions
// are implemented separately for each platform.

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

func timeoutWorkers() {
	for {
		for _, channelName := range workers.Keys() {
			worker := workers.Get(channelName).(*Worker)

			// Skip workers with infinite timeout
			if worker.QuitTimeoutSeconds == WORKER_TIMEOUT_INFINITY {
				continue
			}

			nowTs := time.Now().Unix()
			if worker.UpdateTs+int64(worker.QuitTimeoutSeconds) < nowTs {
				if err := worker.stop(uuid.New().String(), channelName.(string)); err != nil {
					slog.Error("Timeout worker stop failed", "err", err, "channelName", channelName, logTag)
					continue
				}

				slog.Info("Timeout worker stop success", "channelName", channelName, "worker", worker, "nowTs", nowTs, logTag)
			}
		}

		slog.Debug("Worker timeout check", "sleep", workerCleanSleepSeconds, logTag)
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
