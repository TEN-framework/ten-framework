package internal

import (
	"log/slog"
	"os"
	"strconv"
)

type Prop struct {
	ExtensionName string
	Property      string
	ConvertType   string // Optional: "string" to convert value to string
}

const (
	// Extension name
	extensionNameAgoraRTC   = "agora_rtc"
	extensionNameAgoraRTM   = "agora_rtm"
	extensionNameHttpServer = "http_server"

	// Property json
	PropertyJsonFile = "./property.json"
	// Token expire time
	tokenExpirationInSeconds = uint32(86400)

	WORKER_TIMEOUT_INFINITY = -1
)

var (
	logTag = slog.String("service", "HTTP_SERVER")

	// MAX_GEMINI_WORKER_COUNT can be configured via MAX_GEMINI_WORKER_COUNT env var, defaults to 3
	MAX_GEMINI_WORKER_COUNT = getMaxGeminiWorkerCount()

	// Retrieve parameters from the request and map them to the property.json file
	startPropMap = map[string][]Prop{
		"ChannelName": {
			{ExtensionName: extensionNameAgoraRTC, Property: "channel"},
			{ExtensionName: extensionNameAgoraRTM, Property: "channel"},
		},
		"RemoteStreamId": {
			{ExtensionName: extensionNameAgoraRTC, Property: "remote_stream_id"},
		},
		"BotStreamId": {
			{ExtensionName: extensionNameAgoraRTC, Property: "stream_id"},
			{ExtensionName: extensionNameAgoraRTM, Property: "user_id", ConvertType: "string"},
		},
		"Token": {
			{ExtensionName: extensionNameAgoraRTC, Property: "token"},
			{ExtensionName: extensionNameAgoraRTM, Property: "token"},
		},
		"WorkerHttpServerPort": {
			{ExtensionName: extensionNameHttpServer, Property: "listen_port"},
		},
	}
)

func getMaxGeminiWorkerCount() int {
	if val := os.Getenv("MAX_GEMINI_WORKER_COUNT"); val != "" {
		if count, err := strconv.Atoi(val); err == nil && count > 0 {
			slog.Info("Using MAX_GEMINI_WORKER_COUNT from environment", "value", count)
			return count
		}
		slog.Warn("Invalid MAX_GEMINI_WORKER_COUNT env var, using default", "value", val, "default", 3)
	}
	return 3
}
