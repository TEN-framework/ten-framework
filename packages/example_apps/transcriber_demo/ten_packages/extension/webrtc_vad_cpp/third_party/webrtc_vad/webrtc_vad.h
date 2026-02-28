/*
 * WebRTC VAD (Voice Activity Detection) API
 * Extracted from WebRTC project for standalone use.
 */

#ifndef WEBRTC_COMMON_AUDIO_VAD_INCLUDE_WEBRTC_VAD_H_
#define WEBRTC_COMMON_AUDIO_VAD_INCLUDE_WEBRTC_VAD_H_

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct WebRtcVadInst VadInst;

// Creates an instance to the VAD structure.
VadInst *WebRtcVad_Create(void);

// Frees the dynamic memory of a specified VAD instance.
//
// - handle [i] : Pointer to VAD instance that should be freed.
void WebRtcVad_Free(VadInst *handle);

// Initializes a VAD instance.
//
// - handle [i/o] : Instance that should be initialized.
//
// returns        : 0 - (OK),
//                 -1 - (null pointer or Default mode could not be set).
int WebRtcVad_Init(VadInst *handle);

// Sets the VAD operating mode. A more aggressive (higher mode) VAD is more
// restrictive in reporting speech. Put in other words the probability of being
// speech when the VAD returns 1 is increased with increasing mode. As a
// consequence also the missed detection rate goes up.
//
// - handle [i/o] : VAD instance.
// - mode   [i]   : Aggressiveness mode (0, 1, 2, or 3).
//
// returns        : 0 - (OK),
//                 -1 - (null pointer, mode could not be set or the VAD instance
//                       has not been initialized).
int WebRtcVad_set_mode(VadInst *handle, int mode);

// Calculates a VAD decision for the |audio_frame|. For valid sampling rates
// frame lengths, see the description of WebRtcVad_ValidRatesAndFrameLengths().
//
// - handle       [i/o] : VAD Instance. Needs to be initialized by
//                        WebRtcVad_Init() before call.
// - fs           [i]   : Sampling frequency (Hz): 8000, 16000, or 32000
// - audio_frame  [i]   : Audio frame buffer.
// - frame_length [i]   : Length of audio frame buffer in number of samples.
//
// returns              : 1 - (Active Voice),
//                        0 - (Non-active Voice),
//                       -1 - (Error)
int WebRtcVad_Process(VadInst *handle, int fs, const int16_t *audio_frame,
                      size_t frame_length);

// Checks for valid combinations of |rate| and |frame_length|. We support 10,
// 20 and 30 ms frames and the rates 8000, 16000, 32000 and 48000 Hz.
//
// - rate         [i] : Sampling frequency (Hz).
// - frame_length [i] : Speech frame buffer length in number of samples.
//
// returns            : 0 - (valid combination), -1 - (invalid combination)
int WebRtcVad_ValidRateAndFrameLength(int rate, size_t frame_length);

#ifdef __cplusplus
}
#endif

#endif  // WEBRTC_COMMON_AUDIO_VAD_INCLUDE_WEBRTC_VAD_H_
