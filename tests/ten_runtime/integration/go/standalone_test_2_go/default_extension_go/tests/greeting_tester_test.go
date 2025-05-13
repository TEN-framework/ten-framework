package tests

import (
	ten "ten_framework/ten_runtime"
	"testing"
)

func TestGreetingTester(t *testing.T) {
	greetingMsg := "im ok!"

	myTester := &GreetingTester{
		ExpectedGreetingMsg: greetingMsg,
	}

	tester, err := ten.NewExtensionTester(myTester)
	if err != nil {
		t.FailNow()
	}

	tester.SetTestModeSingle("default_extension_go", "{\"greetingMsg\": \""+greetingMsg+"\"}")
	tester.Run()
}

func TestGreetingTesterEmpty(t *testing.T) {
	myTester := &GreetingTester{
		ExpectedGreetingMsg: "",
	}

	tester, err := ten.NewExtensionTester(myTester)
	if err != nil {
		t.FailNow()
	}

	tester.SetTestModeSingle("default_extension_go", "{}")
	tester.Run()
}
