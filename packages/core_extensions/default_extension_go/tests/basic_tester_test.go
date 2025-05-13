package tests

import (
	ten "ten_framework/ten_runtime"
	"testing"

	_ "default_extension_go"
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

func TestBasicExtensionTester2(t *testing.T) {
	myTester := &BasicExtensionTester{}

	tester, err := ten.NewExtensionTester(myTester)
	if err != nil {
		t.FailNow()
	}

	tester.SetTestModeSingle("default_extension_go", "{}")
	tester.Run()
}
