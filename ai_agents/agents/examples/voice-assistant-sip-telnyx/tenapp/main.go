package main

import (
	"context"
	"os"
	"os/signal"
	"syscall"
	"time"

	"github.com/TEN-framework/ten_runtime_go/app"
	"github.com/TEN-framework/ten_runtime_go/pkg/log"
)

func main() {
	// Initialize the application
	tenApp := app.NewApp("agent_demo", "0.10.0")

	// Configure logging
	logConfig := log.NewConfig()
	logConfig.Level = log.InfoLevel
	logConfig.Format = "plain"
	logConfig.Color = true

	// Add log handler
	tenApp.AddLogHandler(log.NewConsoleHandler(logConfig))

	// Load and run the app
	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()

	// Handle shutdown signals
	sigChan := make(chan os.Signal, 1)
	signal.Notify(sigChan, syscall.SIGINT, syscall.SIGTERM)

	go func() {
		sig := <-sigChan
		log.Info("Received signal:", sig)
		cancel()
	}()

	// Start the application
	if err := tenApp.Run(ctx); err != nil {
		log.Error("Failed to run app:", err)
		os.Exit(1)
	}

	// Wait for context cancellation
	<-ctx.Done()

	// Give some time for graceful shutdown
	time.Sleep(time.Second)
	log.Info("Application shutdown complete")
}