//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

package ten

import (
	"testing"
	"time"
)

func TestAudioFrame(t *testing.T) {
	frame, err := NewAudioFrame("audio_frame")
	if err != nil {
		t.FailNow()
	}

	if err := frame.AllocBuf(10); err != nil {
		t.FailNow()
	}

	buf, err := frame.LockBuf()
	if err != nil {
		t.FailNow()
	}

	if len(buf) != 10 {
		t.FailNow()
	}

	buf[0] = 1

	if err := frame.UnlockBuf(&buf); err != nil {
		t.FailNow()
	}

	buf, err = frame.LockBuf()
	if err != nil {
		t.FailNow()
	}

	if buf[0] != 1 {
		t.FailNow()
	}

	now := time.Now().UnixMilli()
	if err := frame.SetTimestamp(now); err != nil {
		t.FailNow()
	}

	if timestamp, err := frame.GetTimestamp(); err != nil || timestamp != now {
		t.FailNow()
	}

	if err := frame.SetSampleRate(44100); err != nil {
		t.FailNow()
	}

	if sampleRate, err := frame.GetSampleRate(); err != nil ||
		sampleRate != 44100 {
		t.FailNow()
	}

	if lineSize, err := frame.GetLineSize(); err != nil {
		t.FailNow()
	} else {
		if lineSize != 0 {
			t.FailNow()
		}
	}
}
