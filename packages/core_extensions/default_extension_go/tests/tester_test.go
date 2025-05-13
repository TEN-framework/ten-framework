package tests

import (
	"fmt"
	"os"
	ten "ten_framework/ten_runtime"
	"testing"

	_ "default_extension_go"
)

var globalApp ten.App

type fakeApp struct {
	ten.DefaultApp

	initDoneChan chan bool
}

func (p *fakeApp) OnInit(tenEnv ten.TenEnv) {
	tenEnv.OnInitDone()
	p.initDoneChan <- true
}

func setup() {
	fakeApp := &fakeApp{
		initDoneChan: make(chan bool),
	}

	var err error

	globalApp, err = ten.NewApp(fakeApp)
	if err != nil {
		fmt.Println("Failed to create global app.")
	}

	globalApp.Run(true)

	<-fakeApp.initDoneChan
}

func teardown() {
	globalApp.Close()
	globalApp.Wait()
}

func TestMain(m *testing.M) {
	setup()
	code := m.Run()
	teardown()
	os.Exit(code)
}

func TestMyExtensionTester(t *testing.T) {
	myTester := &MyExtensionTester{}

	tester, err := ten.NewExtensionTester(myTester)
	if err != nil {
		t.FailNow()
	}

	tester.SetTestModeSingle("default_extension_go", "{}")
	tester.Run()
}

func TestMyExtensionTester2(t *testing.T) {
	myTester := &MyExtensionTester{}

	tester, err := ten.NewExtensionTester(myTester)
	if err != nil {
		t.FailNow()
	}

	tester.SetTestModeSingle("default_extension_go", "{}")
	tester.Run()
}
