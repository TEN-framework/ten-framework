module ten_packages/extension/web_audio_control_go

go 1.20

replace ten_framework => ../../../ten_packages/system/ten_runtime_go/interface

require (
	github.com/gorilla/websocket v1.5.1
	ten_framework v0.0.0-00010101000000-000000000000
)

require golang.org/x/net v0.17.0 // indirect
