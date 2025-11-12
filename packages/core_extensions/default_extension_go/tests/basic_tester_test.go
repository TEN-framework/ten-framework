//
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0.
// See the LICENSE file for more information.
//

package tests

import (
	ten "ten_framework/ten_runtime"
	"testing"
)

func TestBasicExtensionTester(t *testing.T) {
	myTester := &BasicExtensionTester{}

	tester, err := ten.NewExtensionTester(myTester)
	if err != nil {
		t.FailNow()
	}

	tester.SetTestModeSingle("default_extension_go", "{}")
	tester.Run()
}
