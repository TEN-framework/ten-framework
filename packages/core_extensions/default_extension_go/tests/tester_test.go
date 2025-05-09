package tests

import (
	"testing"
	ten "ten_framework/ten_runtime"

	_ "default_extension_go"
)

func TestMyExtensionTester(t *testing.T) {
	myTester := &MyExtensionTester{}

	tester, err := ten.NewExtensionTester(myTester)
	if err != nil {
		t.FailNow()
	}

	tester.SetTestModeSingle("default_extension_go", "{}")
	tester.Run()
}