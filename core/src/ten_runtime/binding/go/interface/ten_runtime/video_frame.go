//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

package ten

// #include <stdbool.h>
// #include "video_frame.h"
import "C"

import "unsafe"

// PixelFmt is the pixel format of the video frame.
type PixelFmt int32

// PixelFmt values. These definitions need to be the same as the
// TEN_PIXEL_FMT enum in C.
//
// Note: To achieve the best compatibility, any new enum item, should be added
// to the end to avoid changing the value of previous enum items.
const (
	pixelFmtInvalid PixelFmt = iota

	PixelFmtRGB24
	PixelFmtRGBA
	PixelFmtBGR24
	PixelFmtBGRA
	PixelFmtI422
	PixelFmtI420
	PixelFmtNV21
	PixelFmtNV12
)

// VideoFrame is the interface for the video frame.
type VideoFrame interface {
	Msg

	Clone() (VideoFrame, error)

	AllocBuf(size int) error
	LockBuf() ([]byte, error)
	UnlockBuf(buf *[]byte) error
	GetBuf() ([]byte, error)

	SetWidth(width int32) error
	GetWidth() (int32, error)

	SetHeight(height int32) error
	GetHeight() (int32, error)

	SetTimestamp(timestamp int64) error
	GetTimestamp() (int64, error)

	IsEOF() (bool, error)
	SetEOF(isEOF bool) error

	GetPixelFmt() (PixelFmt, error)
	SetPixelFmt(pixelFmt PixelFmt) error
}

type videoFrame struct {
	*msg
}

func newVideoFrame(bridge C.uintptr_t) *videoFrame {
	return &videoFrame{msg: newMsg(bridge)}
}

// Just check if `videoFrame` struct implements the `VideoFrame` interface.
var _ VideoFrame = new(videoFrame)

// NewVideoFrame creates a new video frame.
func NewVideoFrame(videoFrameName string) (VideoFrame, error) {
	if len(videoFrameName) == 0 {
		return nil, newTenError(
			ErrorCodeInvalidArgument,
			"video frame name is required.",
		)
	}

	return withCGOLimiterHasReturnValue[VideoFrame](func() (VideoFrame, error) {
		var bridge C.uintptr_t
		apiStatus := C.ten_go_video_frame_create(
			unsafe.Pointer(unsafe.StringData(videoFrameName)),
			C.int(len(videoFrameName)),
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

		return newVideoFrame(bridge), nil
	})
}

func (p *videoFrame) AllocBuf(size int) error {
	if size <= 0 {
		return newTenError(ErrorCodeInvalidArgument, "the size should be > 0")
	}

	err := withCGOLimiter(func() error {
		apiStatus := C.ten_go_video_frame_alloc_buf(p.getCPtr(), C.int(size))
		return withCGoError(&apiStatus)
	})

	return err
}

func (p *videoFrame) LockBuf() ([]byte, error) {
	var bufAddr *C.uint8_t
	var bufSize C.uint64_t

	err := withCGOLimiter(func() error {
		apiStatus := C.ten_go_video_frame_lock_buf(
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

func (p *videoFrame) UnlockBuf(buf *[]byte) error {
	if buf == nil {
		return newTenError(ErrorCodeInvalidArgument, "buf is nil")
	}

	err := withCGOLimiter(func() error {
		apiStatus := C.ten_go_video_frame_unlock_buf(
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

func (p *videoFrame) GetBuf() ([]byte, error) {
	var bufSize C.uint64_t

	err := withCGOLimiter(func() error {
		apiStatus := C.ten_go_video_frame_get_buf_size(p.getCPtr(), &bufSize)
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
		apiStatus := C.ten_go_video_frame_get_buf(
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

func (p *videoFrame) SetWidth(width int32) error {
	if width <= 0 {
		return newTenError(ErrorCodeInvalidArgument, "the width should be > 0")
	}

	return withCGOLimiter(func() error {
		apiStatus := C.ten_go_video_frame_set_width(
			p.getCPtr(),
			C.int32_t(width),
		)
		return withCGoError(&apiStatus)
	})
}

func (p *videoFrame) GetWidth() (int32, error) {
	var width C.int32_t

	err := withCGOLimiter(func() error {
		apiStatus := C.ten_go_video_frame_get_width(p.getCPtr(), &width)
		return withCGoError(&apiStatus)
	})

	if err != nil {
		return 0, err
	}

	return int32(width), nil
}

func (p *videoFrame) SetHeight(height int32) error {
	if height <= 0 {
		return newTenError(ErrorCodeInvalidArgument, "the height should be > 0")
	}

	return withCGOLimiter(func() error {
		apiStatus := C.ten_go_video_frame_set_height(
			p.getCPtr(),
			C.int32_t(height),
		)
		return withCGoError(&apiStatus)
	})
}

func (p *videoFrame) GetHeight() (int32, error) {
	var height C.int32_t

	err := withCGOLimiter(func() error {
		apiStatus := C.ten_go_video_frame_get_height(p.getCPtr(), &height)
		return withCGoError(&apiStatus)
	})

	if err != nil {
		return 0, err
	}

	return int32(height), nil
}

func (p *videoFrame) SetTimestamp(timestamp int64) error {
	if timestamp <= 0 {
		return newTenError(
			ErrorCodeInvalidArgument,
			"the timestamp should be > 0",
		)
	}

	return withCGOLimiter(func() error {
		apiStatus := C.ten_go_video_frame_set_timestamp(
			p.getCPtr(),
			C.int64_t(timestamp),
		)
		return withCGoError(&apiStatus)
	})
}

func (p *videoFrame) GetTimestamp() (int64, error) {
	var timestamp C.int64_t

	err := withCGOLimiter(func() error {
		apiStatus := C.ten_go_video_frame_get_timestamp(p.getCPtr(), &timestamp)
		return withCGoError(&apiStatus)
	})

	if err != nil {
		return 0, err
	}

	return int64(timestamp), nil
}

func (p *videoFrame) IsEOF() (bool, error) {
	var isEOF C.bool

	err := withCGOLimiter(func() error {
		apiStatus := C.ten_go_video_frame_is_eof(p.getCPtr(), &isEOF)
		return withCGoError(&apiStatus)
	})

	if err != nil {
		return false, err
	}

	return bool(isEOF), nil
}

func (p *videoFrame) SetEOF(isEOF bool) error {
	return withCGOLimiter(func() error {
		apiStatus := C.ten_go_video_frame_set_eof(
			p.getCPtr(),
			C.bool(isEOF),
		)
		return withCGoError(&apiStatus)
	})
}

func (p *videoFrame) GetPixelFmt() (PixelFmt, error) {
	var pixelFmt C.uint32_t

	err := withCGOLimiter(func() error {
		apiStatus := C.ten_go_video_frame_get_pixel_fmt(p.getCPtr(), &pixelFmt)
		return withCGoError(&apiStatus)
	})

	if err != nil {
		return 0, err
	}

	return PixelFmt(pixelFmt), nil
}

func (p *videoFrame) SetPixelFmt(pixelFmt PixelFmt) error {
	return withCGOLimiter(func() error {
		apiStatus := C.ten_go_video_frame_set_pixel_fmt(
			p.getCPtr(),
			C.uint32_t(pixelFmt),
		)
		return withCGoError(&apiStatus)
	})
}

func (p *videoFrame) Clone() (VideoFrame, error) {
	var bridge C.uintptr_t
	err := withCGOLimiter(func() error {
		apiStatus := C.ten_go_video_frame_clone(p.getCPtr(), &bridge)
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

	return newVideoFrame(bridge), nil
}
