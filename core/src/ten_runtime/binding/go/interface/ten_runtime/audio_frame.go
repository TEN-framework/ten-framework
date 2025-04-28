//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

package ten

// #include <stdbool.h>
// #include "audio_frame.h"
import "C"

import "unsafe"

// AudioFrameDataFmt is the audio frame data format.
type AudioFrameDataFmt int32

// AudioFrameDataFmt values. These definitions need to be the same as the
// TEN_AUDIO_FRAME_DATA_FMT enum in C.
//
// Note: To achieve the best compatibility, any new enum item, should be added
// to the end to avoid changing the value of previous enum items.
const (
	audioFrameDataFmtInvalid AudioFrameDataFmt = iota

	// Packet format in FFmpeg. Ex: ABABABAB
	AudioFrameDataFmtInterleave

	// Planar format in FFmpeg. Ex: AAAABBBB
	AudioFrameDataFmtNonInterleave
)

// AudioFrame is the interface for the audio frame.
type AudioFrame interface {
	Msg

	Clone() (AudioFrame, error)

	AllocBuf(size int) error
	LockBuf() ([]byte, error)
	UnlockBuf(buf *[]byte) error
	GetBuf() ([]byte, error)

	SetTimestamp(timestamp int64) error
	GetTimestamp() (int64, error)

	SetSampleRate(sampleRate int32) error
	GetSampleRate() (int32, error)

	SetChannelLayout(channelLayout uint64) error
	GetChannelLayout() (uint64, error)

	SetSamplesPerChannel(samplesPerChannel int32) error
	GetSamplesPerChannel() (int32, error)

	SetBytesPerSample(bytesPerSample int32) error
	GetBytesPerSample() (int32, error)

	SetNumberOfChannels(numberOfChannels int32) error
	GetNumberOfChannels() (int32, error)

	SetDataFmt(dataFmt AudioFrameDataFmt) error
	GetDataFmt() (AudioFrameDataFmt, error)

	SetLineSize(lineSize int32) error
	GetLineSize() (int32, error)

	IsEOF() (bool, error)
	SetEOF(isEOF bool) error
}

type audioFrame struct {
	*msg
}

func newAudioFrame(bridge C.uintptr_t) *audioFrame {
	return &audioFrame{msg: newMsg(bridge)}
}

var _ AudioFrame = new(audioFrame)

// NewAudioFrame creates a new audio frame.
func NewAudioFrame(audioFrameName string) (AudioFrame, error) {
	if len(audioFrameName) == 0 {
		return nil, newTenError(
			ErrorCodeInvalidArgument,
			"audio frame name is required",
		)
	}

	return withCGOLimiterHasReturnValue[AudioFrame](func() (AudioFrame, error) {
		var bridge C.uintptr_t
		apiStatus := C.ten_go_audio_frame_create(
			unsafe.Pointer(unsafe.StringData(audioFrameName)),
			C.int(len(audioFrameName)),
			&bridge,
		)
		err := withCGoError(&apiStatus)
		if err != nil {
			return nil, err
		}

		if bridge == 0 {
			// Should not happen.
			return nil, newTenError(
				ErrorCodeInvalidArgument,
				"bridge is nil",
			)
		}

		return newAudioFrame(bridge), nil
	})
}

func (p *audioFrame) SetTimestamp(timestamp int64) error {
	return withCGOLimiter(func() error {
		apiStatus := C.ten_go_audio_frame_set_timestamp(
			p.getCPtr(),
			C.int64_t(timestamp),
		)
		return withCGoError(&apiStatus)
	})
}
func (p *audioFrame) GetTimestamp() (int64, error) {
	var timestamp C.int64_t
	err := withCGOLimiter(func() error {
		apiStatus := C.ten_go_audio_frame_get_timestamp(
			p.getCPtr(),
			&timestamp,
		)
		return withCGoError(&apiStatus)
	})

	if err != nil {
		return 0, err
	}

	return int64(timestamp), nil
}

func (p *audioFrame) SetSampleRate(sampleRate int32) error {
	return withCGOLimiter(func() error {
		apiStatus := C.ten_go_audio_frame_set_sample_rate(
			p.getCPtr(),
			C.int32_t(sampleRate),
		)
		return withCGoError(&apiStatus)
	})
}

func (p *audioFrame) GetSampleRate() (int32, error) {
	var sampleRate C.int32_t
	err := withCGOLimiter(func() error {
		apiStatus := C.ten_go_audio_frame_get_sample_rate(
			p.getCPtr(),
			&sampleRate,
		)
		return withCGoError(&apiStatus)
	})

	if err != nil {
		return 0, err
	}

	return int32(sampleRate), nil
}

func (p *audioFrame) SetChannelLayout(channelLayout uint64) error {
	return withCGOLimiter(func() error {
		apiStatus := C.ten_go_audio_frame_set_channel_layout(
			p.getCPtr(),
			C.uint64_t(channelLayout),
		)
		return withCGoError(&apiStatus)
	})
}

func (p *audioFrame) GetChannelLayout() (uint64, error) {
	var channelLayout C.uint64_t
	err := withCGOLimiter(func() error {
		apiStatus := C.ten_go_audio_frame_get_channel_layout(
			p.getCPtr(),
			&channelLayout,
		)
		return withCGoError(&apiStatus)
	})

	if err != nil {
		return 0, err
	}

	return uint64(channelLayout), nil
}

func (p *audioFrame) SetSamplesPerChannel(samplesPerChannel int32) error {
	return withCGOLimiter(func() error {
		apiStatus := C.ten_go_audio_frame_set_samples_per_channel(
			p.getCPtr(),
			C.int32_t(samplesPerChannel),
		)
		return withCGoError(&apiStatus)
	})
}

func (p *audioFrame) GetSamplesPerChannel() (int32, error) {
	var samplesPerChannel C.int32_t
	err := withCGOLimiter(func() error {
		apiStatus := C.ten_go_audio_frame_get_samples_per_channel(
			p.getCPtr(),
			&samplesPerChannel,
		)
		return withCGoError(&apiStatus)
	})

	if err != nil {
		return 0, err
	}

	return int32(samplesPerChannel), nil
}

func (p *audioFrame) SetBytesPerSample(bytesPerSample int32) error {
	return withCGOLimiter(func() error {
		apiStatus := C.ten_go_audio_frame_set_bytes_per_sample(
			p.getCPtr(),
			C.int32_t(bytesPerSample),
		)
		return withCGoError(&apiStatus)
	})
}

func (p *audioFrame) GetBytesPerSample() (int32, error) {
	var bytesPerSample C.int32_t
	err := withCGOLimiter(func() error {
		apiStatus := C.ten_go_audio_frame_get_bytes_per_sample(
			p.getCPtr(),
			&bytesPerSample,
		)
		return withCGoError(&apiStatus)
	})

	if err != nil {
		return 0, err
	}

	return int32(bytesPerSample), nil
}

func (p *audioFrame) SetNumberOfChannels(numberOfChannels int32) error {
	return withCGOLimiter(func() error {
		apiStatus := C.ten_go_audio_frame_set_number_of_channels(
			p.getCPtr(),
			C.int32_t(numberOfChannels),
		)
		return withCGoError(&apiStatus)
	})
}

func (p *audioFrame) GetNumberOfChannels() (int32, error) {
	var numberOfChannels C.int32_t
	err := withCGOLimiter(func() error {
		apiStatus := C.ten_go_audio_frame_get_number_of_channels(
			p.getCPtr(),
			&numberOfChannels,
		)
		return withCGoError(&apiStatus)
	})

	if err != nil {
		return 0, err
	}

	return int32(numberOfChannels), nil
}

func (p *audioFrame) SetDataFmt(dataFmt AudioFrameDataFmt) error {
	return withCGOLimiter(func() error {
		apiStatus := C.ten_go_audio_frame_set_data_fmt(
			p.getCPtr(),
			C.uint32_t(dataFmt),
		)
		return withCGoError(&apiStatus)
	})
}

func (p *audioFrame) GetDataFmt() (AudioFrameDataFmt, error) {
	var dataFmt C.uint32_t
	err := withCGOLimiter(func() error {
		apiStatus := C.ten_go_audio_frame_get_data_fmt(
			p.getCPtr(),
			&dataFmt,
		)
		return withCGoError(&apiStatus)
	})

	if err != nil {
		return AudioFrameDataFmtInterleave, err
	}

	return AudioFrameDataFmt(dataFmt), nil
}

func (p *audioFrame) SetLineSize(lineSize int32) error {
	return withCGOLimiter(func() error {
		apiStatus := C.ten_go_audio_frame_set_line_size(
			p.getCPtr(),
			C.int32_t(lineSize),
		)
		return withCGoError(&apiStatus)
	})
}

func (p *audioFrame) GetLineSize() (int32, error) {
	var lineSize C.int32_t
	err := withCGOLimiter(func() error {
		apiStatus := C.ten_go_audio_frame_get_line_size(
			p.getCPtr(),
			&lineSize,
		)
		return withCGoError(&apiStatus)
	})

	if err != nil {
		return 0, err
	}

	return int32(lineSize), nil
}

func (p *audioFrame) AllocBuf(size int) error {
	if size <= 0 {
		return newTenError(ErrorCodeInvalidArgument, "the size should be > 0")
	}

	err := withCGOLimiter(func() error {
		apiStatus := C.ten_go_audio_frame_alloc_buf(
			p.getCPtr(),
			C.int(size),
		)
		return withCGoError(&apiStatus)
	})

	return err
}

func (p *audioFrame) GetBuf() ([]byte, error) {
	var bufSize C.uint64_t
	err := withCGOLimiter(func() error {
		apiStatus := C.ten_go_audio_frame_get_buf_size(p.getCPtr(), &bufSize)
		return withCGoError(&apiStatus)
	})

	if err != nil {
		return nil, err
	}

	if bufSize == 0 {
		return make([]byte, 0), nil
	}

	buf := make([]byte, bufSize)
	err = withCGOLimiter(func() error {
		apiStatus := C.ten_go_audio_frame_get_buf(
			p.getCPtr(),
			unsafe.Pointer(&buf[0]),
			bufSize,
		)

		return withCGoError(&apiStatus)
	})

	if err != nil {
		return nil, err
	}

	return buf, nil
}

func (p *audioFrame) LockBuf() ([]byte, error) {
	var bufAddr *C.uint8_t
	var bufSize C.uint64_t

	err := withCGOLimiter(func() error {
		apiStatus := C.ten_go_audio_frame_lock_buf(
			p.getCPtr(),
			&bufAddr,
			&bufSize,
		)

		return withCGoError(&apiStatus)
	})

	if err != nil {
		return nil, err
	}

	// This means the len and cap of this `buf` are equal.
	buf := unsafe.Slice((*byte)(unsafe.Pointer(bufAddr)), int(bufSize))

	return buf, nil
}

func (p *audioFrame) UnlockBuf(buf *[]byte) error {
	if buf == nil {
		return newTenError(ErrorCodeInvalidArgument, "buf is nil")
	}

	err := withCGOLimiter(func() error {
		apiStatus := C.ten_go_audio_frame_unlock_buf(
			p.getCPtr(),
			unsafe.Pointer(unsafe.SliceData(*buf)),
		)

		return withCGoError(&apiStatus)
	})

	if err == nil {
		// The buf can not be used anymore after giving back.
		*buf = nil
	}

	return err
}

func (p *audioFrame) IsEOF() (bool, error) {
	var isEOF C.bool
	err := withCGOLimiter(func() error {
		apiStatus := C.ten_go_audio_frame_is_eof(
			p.getCPtr(),
			&isEOF,
		)
		return withCGoError(&apiStatus)
	})

	if err != nil {
		return false, err
	}

	return bool(isEOF), nil
}

func (p *audioFrame) SetEOF(isEOF bool) error {
	return withCGOLimiter(func() error {
		apiStatus := C.ten_go_audio_frame_set_eof(
			p.getCPtr(),
			C.bool(isEOF),
		)
		return withCGoError(&apiStatus)
	})
}

func (p *audioFrame) Clone() (AudioFrame, error) {
	var bridge C.uintptr_t
	err := withCGOLimiter(func() error {
		apiStatus := C.ten_go_audio_frame_clone(p.getCPtr(), &bridge)
		return withCGoError(&apiStatus)
	})

	if err != nil {
		return nil, err
	}

	if bridge == 0 {
		// Should not happen.
		return nil, newTenError(
			ErrorCodeInvalidArgument,
			"bridge is nil",
		)
	}

	return newAudioFrame(bridge), nil
}
