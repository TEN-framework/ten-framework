//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

typedef enum TEN_AUDIO_FRAME_FIELD {
  TEN_AUDIO_FRAME_FIELD_MSGHDR,

  TEN_AUDIO_FRAME_FIELD_TIMESTAMP,
  TEN_AUDIO_FRAME_FIELD_SAMPLE_RATE,
  TEN_AUDIO_FRAME_FIELD_BYTES_PER_SAMPLE,
  TEN_AUDIO_FRAME_FIELD_SAMPLES_PER_CHANNEL,
  TEN_AUDIO_FRAME_FIELD_NUMBER_OF_CHANNEL,
  TEN_AUDIO_FRAME_FIELD_DATA_FMT,
  TEN_AUDIO_FRAME_FIELD_BUF,
  TEN_AUDIO_FRAME_FIELD_LINE_SIZE,

  TEN_AUDIO_FRAME_FIELD_LAST,
} TEN_AUDIO_FRAME_FIELD;
