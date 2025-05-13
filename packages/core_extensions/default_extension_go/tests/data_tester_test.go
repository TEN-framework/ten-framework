package tests

import (
	ten "ten_framework/ten_runtime"
	"testing"

	_ "default_extension_go"
)

func TestDataExtensionTester(t *testing.T) {
	myTester := &DataExtensionTester{}

	tester, err := ten.NewExtensionTester(myTester)
	if err != nil {
		t.FailNow()
	}

	tester.SetTestModeSingle("default_extension_go", "{}")
	tester.Run()
}
