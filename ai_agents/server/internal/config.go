package internal

import (
	"log/slog"
	"os"
	"strconv"
)

type Prop struct {
	ExtensionName string
	Property      string
}

const (
	// Extension name
	extensionNameAgoraRTC   = "agora_rtc"
	extensionNameAgoraRTM   = "agora_rtm"
	extensionNameHttpServer = "http_server"

	// Property json
	PropertyJsonFile = "./agents/property.json"
	// Token expire time
	tokenExpirationInSeconds = uint32(86400)

	WORKER_TIMEOUT_INFINITY = -1
)

// getMaxGeminiWorkerCount returns the maximum number of Gemini workers from environment variable
// or default value of 3 if not set
func getMaxGeminiWorkerCount() int {
	if envVal := os.Getenv("MAX_GEMINI_WORKER_COUNT"); envVal != "" {
		if count, err := strconv.Atoi(envVal); err == nil && count > 0 {
			return count
		}
	}
	return 3
}

var (
	MAX_GEMINI_WORKER_COUNT = getMaxGeminiWorkerCount()
	logTag                  = slog.String("service", "HTTP_SERVER")

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
