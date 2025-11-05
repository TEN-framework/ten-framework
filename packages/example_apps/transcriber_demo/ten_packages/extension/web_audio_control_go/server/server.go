//
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0.
// See the LICENSE file for more information.
//

package server

import (
	"embed"
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"os"
	"path/filepath"
	"sync"
	"time"

	ten "ten_framework/ten_runtime"

	"github.com/gorilla/websocket"
)

//go:embed static/*
var staticFiles embed.FS

var (
	logCategoryKeyPoint = "key_point"
)

type WebServer struct {
	port             int
	tenEnv           ten.TenEnv
	clients          map[*websocket.Conn]bool
	clientsMu        sync.RWMutex
	upgrader         websocket.Upgrader
	server           *http.Server
	uploadDir        string
	audioDataHandler func(audioData []byte, sampleRate int, channels int, samplesPerChannel int)
}

type StartPlayRequest struct {
	FilePath     string `json:"file_path"`
	LoopPlayback bool   `json:"loop_playback"`
}

type WebSocketMessage struct {
	Type              string `json:"type"` // "asr_result", "audio_data" or "error"
	Text              string `json:"text"`
	Final             bool   `json:"final"`
	SampleRate        int    `json:"sample_rate"`
	Channels          int    `json:"channels"`
	SamplesPerChannel int    `json:"samples_per_channel"`
}

func NewWebServer(port int, tenEnv ten.TenEnv) *WebServer {
	// Create upload directory
	uploadDir := filepath.Join(os.TempDir(), "audio_uploads")
	if err := os.MkdirAll(uploadDir, 0755); err != nil {
		tenEnv.LogError(fmt.Sprintf("Failed to create upload directory: %v", err))
	}

	return &WebServer{
		port:      port,
		tenEnv:    tenEnv,
		clients:   make(map[*websocket.Conn]bool),
		uploadDir: uploadDir,
		upgrader: websocket.Upgrader{
			CheckOrigin: func(r *http.Request) bool {
				return true // Allow all origins for development
			},
		},
	}
}

func (s *WebServer) Start() {
	mux := http.NewServeMux()

	// WebSocket endpoint
	mux.HandleFunc("/ws", s.handleWebSocket)

	// API endpoint for uploading files
	mux.HandleFunc("/api/upload", s.handleUpload)

	// API endpoint for starting playback
	mux.HandleFunc("/api/start_play", s.handleStartPlay)

	// API endpoints for recording control
	mux.HandleFunc("/api/start_recording", s.handleStartRecording)
	mux.HandleFunc("/api/stop_recording", s.handleStopRecording)
	mux.HandleFunc("/api/list_sessions", s.handleListSessions)

	// Serve recordings directory
	recordingsDir := "./recordings"
	if _, err := os.Stat(recordingsDir); os.IsNotExist(err) {
		os.MkdirAll(recordingsDir, 0755)
	}
	mux.Handle("/recordings/", http.StripPrefix("/recordings/", http.FileServer(http.Dir(recordingsDir))))

	// Root path handler - redirect to index.html
	mux.HandleFunc("/", func(w http.ResponseWriter, r *http.Request) {
		if r.URL.Path == "/" {
			http.Redirect(w, r, "/static/index.html", http.StatusFound)
			return
		}
		// Serve other static files
		http.FileServer(http.FS(staticFiles)).ServeHTTP(w, r)
	})

	s.server = &http.Server{
		Addr:    fmt.Sprintf(":%d", s.port),
		Handler: mux,
	}

	s.tenEnv.LogInfo(fmt.Sprintf("Starting HTTP server on :%d", s.port))
	if err := s.server.ListenAndServe(); err != nil && err != http.ErrServerClosed {
		s.tenEnv.LogError(fmt.Sprintf("HTTP server error: %v", err))
	}
}

func (s *WebServer) Stop() {
	s.tenEnv.LogInfo("Stopping HTTP server")

	// Close all WebSocket connections
	s.clientsMu.Lock()
	for client := range s.clients {
		client.Close()
	}
	s.clients = make(map[*websocket.Conn]bool)
	s.clientsMu.Unlock()

	// Shutdown HTTP server
	if s.server != nil {
		s.server.Close()
	}
}

func (s *WebServer) SetAudioDataHandler(handler func(audioData []byte, sampleRate int, channels int, samplesPerChannel int)) {
	s.audioDataHandler = handler
}

func (s *WebServer) handleWebSocket(w http.ResponseWriter, r *http.Request) {
	conn, err := s.upgrader.Upgrade(w, r, nil)
	if err != nil {
		s.tenEnv.LogError(fmt.Sprintf("WebSocket upgrade failed: %v", err))
		return
	}

	s.clientsMu.Lock()
	s.clients[conn] = true
	s.clientsMu.Unlock()

	s.tenEnv.Log(ten.LogLevelInfo, "New WebSocket client connected", &logCategoryKeyPoint, nil, nil)

	// Handle client disconnection
	defer func() {
		s.clientsMu.Lock()
		delete(s.clients, conn)
		s.clientsMu.Unlock()
		conn.Close()
		s.tenEnv.Log(ten.LogLevelInfo, "WebSocket client disconnected", &logCategoryKeyPoint, nil, nil)
	}()

	var audioMetadata *WebSocketMessage

	// Keep connection alive and handle incoming messages
	for {
		messageType, message, err := conn.ReadMessage()
		if err != nil {
			break
		}

		if messageType == websocket.TextMessage {
			// Parse JSON message
			var msg WebSocketMessage
			if err := json.Unmarshal(message, &msg); err != nil {
				s.tenEnv.LogError(fmt.Sprintf("Failed to parse JSON message: %v", err))
				continue
			}

			if msg.Type == "audio_data" {
				// Store metadata for next binary message
				audioMetadata = &msg
			}
		} else if messageType == websocket.BinaryMessage {
			// Handle binary audio data
			if audioMetadata != nil && s.audioDataHandler != nil {
				s.audioDataHandler(message, audioMetadata.SampleRate, audioMetadata.Channels, audioMetadata.SamplesPerChannel)
				audioMetadata = nil
			}
		}
	}
}

func (s *WebServer) handleUpload(w http.ResponseWriter, r *http.Request) {
	// Helper function to send JSON error response
	sendJSONError := func(message string, statusCode int) {
		w.Header().Set("Content-Type", "application/json")
		w.WriteHeader(statusCode)
		w.Write([]byte(fmt.Sprintf(`{"status":"error","message":"%s"}`, message)))
	}

	if r.Method != http.MethodPost {
		sendJSONError("Method not allowed", http.StatusMethodNotAllowed)
		return
	}

	// Parse multipart form (max 100MB)
	if err := r.ParseMultipartForm(100 << 20); err != nil {
		s.tenEnv.LogError(fmt.Sprintf("Failed to parse multipart form: %v", err))
		sendJSONError("Failed to parse form", http.StatusBadRequest)
		return
	}

	// Get uploaded file
	file, header, err := r.FormFile("file")
	if err != nil {
		s.tenEnv.LogError(fmt.Sprintf("Failed to get file from form: %v", err))
		sendJSONError("No file uploaded", http.StatusBadRequest)
		return
	}
	defer file.Close()

	// Create unique filename with timestamp
	timestamp := time.Now().Unix()
	filename := fmt.Sprintf("%d_%s", timestamp, filepath.Base(header.Filename))
	destPath := filepath.Join(s.uploadDir, filename)

	// Create destination file
	destFile, err := os.Create(destPath)
	if err != nil {
		s.tenEnv.LogError(fmt.Sprintf("Failed to create destination file: %v", err))
		sendJSONError("Failed to save file", http.StatusInternalServerError)
		return
	}
	defer destFile.Close()

	// Copy file content
	written, err := io.Copy(destFile, file)
	if err != nil {
		s.tenEnv.LogError(fmt.Sprintf("Failed to copy file content: %v", err))
		sendJSONError("Failed to save file", http.StatusInternalServerError)
		return
	}

	s.tenEnv.LogInfo(fmt.Sprintf("File uploaded successfully: %s (%d bytes)", destPath, written))

	// Return success response with file path
	w.Header().Set("Content-Type", "application/json")
	w.WriteHeader(http.StatusOK)
	w.Write([]byte(fmt.Sprintf(`{"status":"ok","message":"File uploaded successfully","file_path":"%s"}`, destPath)))
}

func (s *WebServer) handleStartPlay(w http.ResponseWriter, r *http.Request) {
	// Helper function to send JSON error response
	sendJSONError := func(message string, statusCode int) {
		w.Header().Set("Content-Type", "application/json")
		w.WriteHeader(statusCode)
		w.Write([]byte(fmt.Sprintf(`{"status":"error","message":"%s"}`, message)))
	}

	if r.Method != http.MethodPost {
		sendJSONError("Method not allowed", http.StatusMethodNotAllowed)
		return
	}

	// Parse multipart form data (supports both multipart/form-data and application/x-www-form-urlencoded)
	if err := r.ParseMultipartForm(10 << 20); err != nil {
		// If multipart parsing fails, try regular form parsing
		if err := r.ParseForm(); err != nil {
			s.tenEnv.LogError(fmt.Sprintf("Failed to parse form: %v", err))
			sendJSONError("Failed to parse form", http.StatusBadRequest)
			return
		}
	}

	filePath := r.FormValue("file_path")
	if filePath == "" {
		s.tenEnv.LogError("file_path is empty or missing")
		sendJSONError("file_path is required", http.StatusBadRequest)
		return
	}

	loopPlayback := r.FormValue("loop_playback") == "true"

	s.tenEnv.LogInfo(fmt.Sprintf("Received start_play request: file_path=%s, loop=%v", filePath, loopPlayback))

	// Create TEN command
	cmd, err := ten.NewCmd("start_play")
	if err != nil {
		s.tenEnv.LogError(fmt.Sprintf("Failed to create command: %v", err))
		sendJSONError("Failed to create command", http.StatusInternalServerError)
		return
	}

	// Set command properties
	if err := cmd.SetPropertyString("file_path", filePath); err != nil {
		s.tenEnv.LogError(fmt.Sprintf("Failed to set file_path property: %v", err))
		sendJSONError("Failed to set file_path", http.StatusInternalServerError)
		return
	}

	if err := cmd.SetProperty("loop_playback", loopPlayback); err != nil {
		s.tenEnv.LogError(fmt.Sprintf("Failed to set loop_playback property: %v", err))
		sendJSONError("Failed to set loop_playback", http.StatusInternalServerError)
		return
	}

	// Send command
	if err := s.tenEnv.SendCmd(cmd, nil); err != nil {
		s.tenEnv.LogError(fmt.Sprintf("Failed to send command: %v", err))
		sendJSONError("Failed to send command", http.StatusInternalServerError)
		return
	}

	s.tenEnv.LogInfo("start_play command sent successfully")

	// Return success response
	w.Header().Set("Content-Type", "application/json")
	w.WriteHeader(http.StatusOK)
	w.Write([]byte(`{"status":"ok","message":"Playback started"}`))
}

func (s *WebServer) BroadcastAsrResult(text string, final bool) {
	s.clientsMu.RLock()
	defer s.clientsMu.RUnlock()

	message := WebSocketMessage{
		Type:  "asr_result",
		Text:  text,
		Final: final,
	}

	for client := range s.clients {
		err := client.WriteJSON(message)
		if err != nil {
			s.tenEnv.LogError(fmt.Sprintf("Failed to send message to client: %v", err))
			client.Close()
			delete(s.clients, client)
		}
	}
}

// handleStartRecording starts a new recording session
func (s *WebServer) handleStartRecording(w http.ResponseWriter, r *http.Request) {
	sendJSONError := func(message string, statusCode int) {
		w.Header().Set("Content-Type", "application/json")
		w.WriteHeader(statusCode)
		json.NewEncoder(w).Encode(map[string]string{
			"status":  "error",
			"message": message,
		})
	}

	if r.Method != http.MethodPost {
		sendJSONError("Method not allowed", http.StatusMethodNotAllowed)
		return
	}

	// Create TEN command
	cmd, err := ten.NewCmd("start_recording")
	if err != nil {
		s.tenEnv.LogError(fmt.Sprintf("Failed to create command: %v", err))
		sendJSONError("Failed to create command", http.StatusInternalServerError)
		return
	}

	// Send command (fire and forget)
	if err := s.tenEnv.SendCmd(cmd, nil); err != nil {
		s.tenEnv.LogError(fmt.Sprintf("Failed to send command: %v", err))
		sendJSONError("Failed to send command", http.StatusInternalServerError)
		return
	}

	s.tenEnv.LogInfo("start_recording command sent")

	w.Header().Set("Content-Type", "application/json")
	w.WriteHeader(http.StatusOK)
	json.NewEncoder(w).Encode(map[string]string{
		"status":  "ok",
		"message": "Recording started",
	})
}

// handleStopRecording stops the current recording session
func (s *WebServer) handleStopRecording(w http.ResponseWriter, r *http.Request) {
	sendJSONError := func(message string, statusCode int) {
		w.Header().Set("Content-Type", "application/json")
		w.WriteHeader(statusCode)
		json.NewEncoder(w).Encode(map[string]string{
			"status":  "error",
			"message": message,
		})
	}

	if r.Method != http.MethodPost {
		sendJSONError("Method not allowed", http.StatusMethodNotAllowed)
		return
	}

	// Create TEN command
	cmd, err := ten.NewCmd("stop_recording")
	if err != nil {
		s.tenEnv.LogError(fmt.Sprintf("Failed to create command: %v", err))
		sendJSONError("Failed to create command", http.StatusInternalServerError)
		return
	}

	// Send command (fire and forget)
	if err := s.tenEnv.SendCmd(cmd, nil); err != nil {
		s.tenEnv.LogError(fmt.Sprintf("Failed to send command: %v", err))
		sendJSONError("Failed to send command", http.StatusInternalServerError)
		return
	}

	s.tenEnv.LogInfo("stop_recording command sent")

	w.Header().Set("Content-Type", "application/json")
	w.WriteHeader(http.StatusOK)
	json.NewEncoder(w).Encode(map[string]string{
		"status":  "ok",
		"message": "Recording stopped",
	})
}

// handleListSessions lists all recording sessions
func (s *WebServer) handleListSessions(w http.ResponseWriter, r *http.Request) {
	sendJSONError := func(message string, statusCode int) {
		w.Header().Set("Content-Type", "application/json")
		w.WriteHeader(statusCode)
		json.NewEncoder(w).Encode(map[string]string{
			"status":  "error",
			"message": message,
		})
	}

	if r.Method != http.MethodGet {
		sendJSONError("Method not allowed", http.StatusMethodNotAllowed)
		return
	}

	// Read sessions from recordings directory
	recordingsDir := "./recordings"
	sessions := []map[string]interface{}{}

	entries, err := os.ReadDir(recordingsDir)
	if err != nil {
		s.tenEnv.LogError(fmt.Sprintf("Failed to read recordings directory: %v", err))
	} else {
		for _, entry := range entries {
			if entry.IsDir() {
				sessionId := entry.Name()
				metadataPath := filepath.Join(recordingsDir, sessionId, "metadata.json")

				// Read metadata file
				if metadataBytes, err := os.ReadFile(metadataPath); err == nil {
					var metadata map[string]interface{}
					if err := json.Unmarshal(metadataBytes, &metadata); err == nil {
						sessions = append(sessions, metadata)
					}
				}
			}
		}
	}

	s.tenEnv.LogInfo(fmt.Sprintf("list_sessions: found %d sessions", len(sessions)))

	// Return sessions list
	w.Header().Set("Content-Type", "application/json")
	w.WriteHeader(http.StatusOK)
	json.NewEncoder(w).Encode(map[string]interface{}{
		"status":   "ok",
		"count":    len(sessions),
		"sessions": sessions,
	})
}
