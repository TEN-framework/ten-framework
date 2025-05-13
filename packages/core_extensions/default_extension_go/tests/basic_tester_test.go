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
