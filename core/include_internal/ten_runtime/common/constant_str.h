//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

// Message.
#define TEN_STR_CMD "cmd"
#define TEN_STR_DATA "data"
#define TEN_STR_VIDEO_FRAME "video_frame"
#define TEN_STR_AUDIO_FRAME "audio_frame"

// Message relevant fields.
#define TEN_STR_PROPERTY "property"
#define TEN_STR_RESULT "result"
#define TEN_STR_CMD_ID "cmd_id"
#define TEN_STR_SEQ_ID "seq_id"

// Location relevant fields.
#define TEN_STR_SRC "src"
#define TEN_STR_DEST "dest"
#define TEN_STR_EXTENSION_GROUP "extension_group"
#define TEN_STR_EXTENSION "extension"
#define TEN_STR_LOC_EMPTY "<empty>"

// Result command relevant fields.
#define TEN_STR_STATUS_CODE "status_code"
#define TEN_STR_DETAIL "detail"
#define TEN_STR_IS_FINAL "is_final"
#define TEN_STR_GRAPH_ID "graph_id"

// Timer relevant fields.
#define TEN_STR_TIMER_ID "timer_id"
#define TEN_STR_TIMES "times"
#define TEN_STR_TIMEOUT_US "timeout_us"

// 'api' field relevant.
#define TEN_STR_API "api"
#define TEN_STR_CMD_IN "cmd_in"
#define TEN_STR_CMD_OUT "cmd_out"
#define TEN_STR_DATA_IN "data_in"
#define TEN_STR_DATA_OUT "data_out"
#define TEN_STR_VIDEO_FRAME_IN "video_frame_in"
#define TEN_STR_VIDEO_FRAME_OUT "video_frame_out"
#define TEN_STR_AUDIO_FRAME_IN "audio_frame_in"
#define TEN_STR_AUDIO_FRAME_OUT "audio_frame_out"

#define TEN_STR_MSG_NAME_TEN_NAMESPACE_PREFIX "ten:"

// The message name might be empty, however, the property schema will be stored
// in a hashtable which key is the name of the msg. Using a special name to
// store the schema if the msg name is not provided.
//
// Because ':' is not a legal character in the TEN schema specification for
// message names, ':' is used in special message names for purely internal use.
#define TEN_STR_MSG_NAME_TEN_EMPTY "ten:empty"

#define TEN_STR_MSG_NAME_TEN_CLOSE_APP "ten:close_app"
#define TEN_STR_MSG_NAME_TEN_STOP_GRAPH "ten:stop_graph"
#define TEN_STR_MSG_NAME_TEN_START_GRAPH "ten:start_graph"
#define TEN_STR_MSG_NAME_TEN_RESULT "ten:result"
#define TEN_STR_MSG_NAME_TEN_TIMEOUT "ten:timeout"
#define TEN_STR_MSG_NAME_TEN_TIMER "ten:timer"
#define TEN_STR_MSG_NAME_TEN_TRIGGER_LIFE_CYCLE "ten:trigger_life_cycle"

// Special command.
#define TEN_STR_STOP_GRAPH "stop_graph"
#define TEN_STR_CLOSE_APP "close_app"
#define TEN_STR_START_GRAPH "start_graph"
#define TEN_STR_TIMER "timer"
#define TEN_STR_TIMEOUT "timeout"
#define TEN_STR_TRIGGER_LIFE_CYCLE "trigger_life_cycle"

// Graph relevant.
#define TEN_STR_PREDEFINED_GRAPHS "predefined_graphs"
#define TEN_STR_PREDEFINED_GRAPH "predefined_graph"
#define TEN_STR_NODES "nodes"
#define TEN_STR_CONNECTIONS "connections"
#define TEN_STR_GRAPH_JSON "graph_json"

#define TEN_STR_PROPERTY_STORE_SCOPE_DELIMITER ":"

// Msg conversion.
#define TEN_STR_VALUE "value"
#define TEN_STR_MSG_CONVERSION "msg_conversion"
#define TEN_STR_CONVERSION_MODE "conversion_mode"
#define TEN_STR_PATH "path"
#define TEN_STR_FIXED_VALUE "fixed_value"
#define TEN_STR_FROM_ORIGINAL "from_original"
#define TEN_STR_RULES "rules"
#define TEN_STR_ORIGINAL_PATH "original_path"
#define TEN_STR_PER_PROPERTY "per_property"
#define TEN_STR_KEEP_ORIGINAL "keep_original"

// Path.
#define TEN_STR_PATH_TIMEOUT "path_timeout"
#define TEN_STR_IN_PATH "in_path"
#define TEN_STR_OUT_PATH "out_path"
#define TEN_STR_PATH_CHECK_INTERVAL "path_check_interval"
#define TEN_STR_ORIGINAL_CMD_TYPE "original_cmd_type"
#define TEN_STR_ORIGINAL_CMD_NAME "original_cmd_name"

// Protocol.
#define TEN_STR_PROTOCOL "protocol"

// Lang addon loader.
#define TEN_STR_ADDON_LOADER "addon_loader"

// Transport.
#define TEN_STR_TRANSPORT_TYPE "transport_type"
#define TEN_STR_TCP "tcp"

// App uri.
#define TEN_STR_URI "uri"
#define TEN_STR_LOCALHOST "localhost"
#define TEN_STR_CLIENT "client:"

// Data.
#define TEN_STR_BUF "buf"

// Video frame.
#define TEN_STR_PIXEL_FMT "pixel_fmt"
#define TEN_STR_TIMESTAMP "timestamp"
#define TEN_STR_WIDTH "width"
#define TEN_STR_HEIGHT "height"
#define TEN_STR_IS_EOF "is_eof"

// audio frame.
#define TEN_STR_DATA_FMT "data_fmt"
#define TEN_STR_LINE_SIZE "line_size"
#define TEN_STR_BYTES_PER_SAMPLE "bytes_per_sample"
#define TEN_STR_CHANNEL_LAYOUT "channel_layout"
#define TEN_STR_NUMBER_OF_CHANNEL "number_of_channel"
#define TEN_STR_SAMPLE_RATE "sample_rate"
#define TEN_STR_SAMPLES_PER_CHANNEL "samples_per_channel"

// Graph.
#define TEN_STR_AUTO_START "auto_start"
#define TEN_STR_SINGLETON "singleton"
#define TEN_STR_GRAPH "graph"
#define TEN_STR_GRAPH_NAME "graph_name"
#define TEN_STR_GRAPH_ID "graph_id"
#define TEN_STR_IMPORT_URI "import_uri"
// = is not a valid graph id or name, so use = as a special string for internal
// use.
#define TEN_STR_DUPLICATE "==duplicate=="
#define TEN_STR_CASCADE_CLOSE_UPWARD "cascade_close_upward"
#define TEN_STR_ADDON "addon"
#define TEN_STR_TEN_PACKAGES "ten_packages"
#define TEN_STR_ONE_EVENT_LOOP_PER_ENGINE "one_event_loop_per_engine"
#define TEN_STR_DEPRECATED_LOG "deprecated_log"
#define TEN_STR_LOG "log"
#define TEN_STR_LEVEL "level"
#define TEN_STR_FILE "file"
#define TEN_STR_ENCRYPTION "encryption"
#define TEN_STR_ENABLED "enabled"
#define TEN_STR_ALGORITHM "algorithm"
#define TEN_STR_PARAMS "params"
#define TEN_STR_KEY "key"
#define TEN_STR_NONCE "nonce"
#define TEN_STR_PROPERTIES "properties"
#define TEN_STR_LONG_RUNNING_MODE "long_running_mode"
#define TEN_STR_PRELOAD_ALL "preload_all"
#define TEN_STR_TYPE "type"
#define TEN_STR_APP "app"
#define TEN_STR_NAME "name"
#define TEN_STR_TEN "ten"
#define TEN_STR_ADDON_BASE_DIR_FIND_FROM_APP_BASE_DIR "=*=find_from_app=*="

// Services.
#define TEN_STR_SERVICES "services"
#define TEN_STR_HOST "host"
#define TEN_STR_PORT "port"

// Telemetry.
#define TEN_STR_TELEMETRY "telemetry"
#define TEN_STR_ENABLED "enabled"
#define TEN_STR_ENDPOINT "endpoint"

#define TEN_STR_DEFAULT_EXTENSION_GROUP "default_extension_group"
#define TEN_STR_TEN_TEST_EXTENSION "ten:test_extension"
#define TEN_STR_UNSPECIFIED_GROUP "ten:unspecified_group"
#define TEN_STR_APP_THREAD "ten:app_thread"
#define TEN_STR_TEN_GRAPH_PROXY_EXTENSION "ten:graph_proxy"
#define TEN_STR_HOST_LOC "host_loc"

#define TEN_STR_MANIFEST_JSON "manifest.json"
#define TEN_STR_MANUAL_TRIGGER_LIFE_CYCLE "manual_trigger_life_cycle"
#define TEN_STR_STAGE "stage"
#define TEN_STR_START "start"
#define TEN_STR_STOP "stop"
