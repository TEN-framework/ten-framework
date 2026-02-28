/*
 * WebRTC VAD (Voice Activity Detection) Implementation
 * This is a simplified standalone implementation.
 */

#include "webrtc_vad.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#define FRAME_LENGTH_8KHZ_10MS 80
#define FRAME_LENGTH_16KHZ_10MS 160
#define FRAME_LENGTH_32KHZ_10MS 320
#define FRAME_LENGTH_48KHZ_10MS 480

struct WebRtcVadInst {
  int mode;           // Aggressiveness mode (0-3)
  int fs;             // Sampling frequency
  int frame_counter;  // Frame counter for smoothing
  int speech_count;   // Count of consecutive speech frames
  int noise_count;    // Count of consecutive noise frames
};

VadInst *WebRtcVad_Create(void) {
  VadInst *handle = (VadInst *)malloc(sizeof(VadInst));
  if (handle != NULL) {
    memset(handle, 0, sizeof(VadInst));
  }
  return handle;
}

void WebRtcVad_Free(VadInst *handle) {
  if (handle != NULL) {
    free(handle);
  }
}

int WebRtcVad_Init(VadInst *handle) {
  if (handle == NULL) {
    return -1;
  }

  memset(handle, 0, sizeof(VadInst));
  handle->mode = 0;
  handle->fs = 16000;

  return 0;
}

int WebRtcVad_set_mode(VadInst *handle, int mode) {
  if (handle == NULL) {
    return -1;
  }

  if (mode < 0 || mode > 3) {
    return -1;
  }

  handle->mode = mode;
  return 0;
}

// Simple energy-based VAD implementation
static int ComputeVadDecision(VadInst *handle, const int16_t *audio_frame,
                              size_t frame_length) {
  // Calculate RMS (Root Mean Square) energy
  double energy = 0.0;
  for (size_t i = 0; i < frame_length; i++) {
    energy += (double)audio_frame[i] * (double)audio_frame[i];
  }
  energy = sqrt(energy / frame_length);

  // Energy threshold based on mode (more aggressive = higher threshold)
  double threshold = 500.0 + handle->mode * 300.0;

  int is_speech = (energy > threshold) ? 1 : 0;

  // Apply smoothing based on history
  if (is_speech) {
    handle->speech_count++;
    handle->noise_count = 0;
  } else {
    handle->noise_count++;
    handle->speech_count = 0;
  }

  // Require multiple consecutive frames for state change
  int hysteresis =
      (3 - handle->mode);  // Less hysteresis for more aggressive modes

  if (handle->speech_count > hysteresis) {
    return 1;
  } else if (handle->noise_count > hysteresis) {
    return 0;
  }

  // Return previous state if in transition
  return (handle->speech_count > 0) ? 1 : 0;
}

int WebRtcVad_Process(VadInst *handle, int fs, const int16_t *audio_frame,
                      size_t frame_length) {
  if (handle == NULL || audio_frame == NULL) {
    return -1;
  }

  // Validate rate and frame length
  if (WebRtcVad_ValidRateAndFrameLength(fs, frame_length) != 0) {
    return -1;
  }

  handle->fs = fs;
  handle->frame_counter++;

  return ComputeVadDecision(handle, audio_frame, frame_length);
}

int WebRtcVad_ValidRateAndFrameLength(int rate, size_t frame_length) {
  // Support 10, 20, and 30 ms frames
  int valid = 0;

  switch (rate) {
  case 8000:
    valid = (frame_length == 80 || frame_length == 160 || frame_length == 240);
    break;
  case 16000:
    valid = (frame_length == 160 || frame_length == 320 || frame_length == 480);
    break;
  case 32000:
    valid = (frame_length == 320 || frame_length == 640 || frame_length == 960);
    break;
  case 48000:
    valid =
        (frame_length == 480 || frame_length == 960 || frame_length == 1440);
    break;
  default:
    valid = 0;
    break;
  }

  return valid ? 0 : -1;
}
