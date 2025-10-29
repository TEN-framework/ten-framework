package main

import (
	"flag"
	"fmt"
	"log/slog"
	"os"
	"os/signal"
	"strconv"
	"syscall"

	"github.com/joho/godotenv"

	"app/internal"
)

func main() {
	// Parse command line arguments
	var tenappDir string
	flag.StringVar(&tenappDir, "tenapp_dir", "", "The base folder path for tman run start command (required)")
	flag.Parse()

	// Validate required tenapp_dir parameter
	if tenappDir == "" {
		slog.Error("tenapp_dir parameter is required")
		fmt.Println("Usage: ./api -tenapp_dir=<path>")
		fmt.Println("  -tenapp_dir: The base folder path for tman run start command (required)")
		os.Exit(1)
	}

	// Load .env
	err := godotenv.Load()
	if err != nil {
		slog.Warn("load .env file failed", "err", err)
	}

	// Check if the directory exists
	logPath := os.Getenv("LOG_PATH")
	if _, err := os.Stat(logPath); os.IsNotExist(err) {
		if err := os.MkdirAll(logPath, os.ModePerm); err != nil {
			slog.Error("create log directory failed", "err", err)
			os.Exit(1)
		}
	}

	log2Stdout, err := strconv.ParseBool(os.Getenv("LOG_STDOUT"))
	if err != nil {
		slog.Error("environment LOG_STDOUT invalid")
		log2Stdout = false
	}

	// Check environment
	agoraAppId := os.Getenv("AGORA_APP_ID")
	if len(agoraAppId) != 32 {
		slog.Error("environment AGORA_APP_ID invalid")
		os.Exit(1)
	}

	workersMax, err := strconv.Atoi(os.Getenv("WORKERS_MAX"))
	if err != nil || workersMax <= 0 {
		slog.Error("environment WORKERS_MAX invalid")
		os.Exit(1)
	}

	// Read worker quit timeout with support for legacy misspelled var
	workerQuitTimeoutEnv := os.Getenv("WORKER_QUIT_TIMEOUT_SECONDS")
	if workerQuitTimeoutEnv == "" {
		workerQuitTimeoutEnv = os.Getenv("WORKER_QUIT_TIMEOUT_SECONDES")
	}
	workerQuitTimeoutSeconds, err := strconv.Atoi(workerQuitTimeoutEnv)
	if err != nil || workerQuitTimeoutSeconds <= 0 {
		slog.Error("environment WORKER_QUIT_TIMEOUT_SECONDS invalid")
		os.Exit(1)
	}

	// Set up signal handler to clean up all workers on Ctrl+C
	sigs := make(chan os.Signal, 1)
	signal.Notify(sigs, syscall.SIGINT, syscall.SIGTERM)

	go func() {
		<-sigs
		fmt.Println("Received interrupt signal, cleaning up workers...")
		internal.CleanWorkers()
		os.Exit(0)
	}()

	// Start server
	httpServerConfig := &internal.HttpServerConfig{
		AppId:                    agoraAppId,
		AppCertificate:           os.Getenv("AGORA_APP_CERTIFICATE"),
		LogPath:                  logPath,
		Port:                     os.Getenv("SERVER_PORT"),
		WorkersMax:               workersMax,
		WorkerQuitTimeoutSeconds: workerQuitTimeoutSeconds,
		Log2Stdout:               log2Stdout,
		TenappDir:                tenappDir,
	}

	slog.Info("Server configured with tenapp_dir", "tenappDir", tenappDir)

	httpServer := internal.NewHttpServer(httpServerConfig)
	httpServer.Start()
}
