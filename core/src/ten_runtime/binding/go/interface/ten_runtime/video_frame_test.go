//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

package ten

import (
	"testing"
)

func TestNewVideoFrame(t *testing.T) {
	frame, err := NewVideoFrame("video_frame")
	if err != nil {
		t.FailNow()
	}

	err = frame.AllocBuf(10)
	if err != nil {
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

	err = frame.UnlockBuf(&buf)
	if err != nil {
		t.FailNow()
	}

	buf2, err := frame.GetBuf()
	if err != nil {
		t.FailNow()
	}

	if buf2[0] != 1 {
		t.FailNow()
	}
}
