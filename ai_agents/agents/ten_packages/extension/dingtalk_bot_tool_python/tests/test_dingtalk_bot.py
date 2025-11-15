import pytest
import json
import os
import threading
from ten_runtime.test import ExtensionTester, TenEnvTester
from ten_runtime.cmd import Cmd
from ten_runtime.cmd_result import CmdResult
from ten_runtime.error import TenError


class TestDingTalkBot(ExtensionTester):
    def __init__(self):
        super().__init__()
        self.test_event = threading.Event()
        self.test_result = None
        self.test_error = None
        print("[TEST_RUNNER] TestDingTalkBot instance created.")

    def on_init(self, ten_env_tester: TenEnvTester) -> None:
        """
        This method is called when the extension is initialized.
        We send the test command here.
        """
        print("[TEST_RUNNER] on_init called.")

        # This is how the extension expects the arguments to be structured.
        arguments = {
            "content": "This is a test message from the ExtensionTester."
        }

        print("[TEST_RUNNER] Creating command...")
        cmd = Cmd.create("tool_call")

        # Set properties instead of using cmd.args directly
        cmd.set_property_string("name", "send_message")
        cmd.set_property_string("arguments", json.dumps(arguments))

        print("[TEST_RUNNER] Sending 'tool_call' command to the extension...")
        ten_env_tester.send_cmd(cmd, self.result_handler)
        print("[TEST_RUNNER] send_cmd returned.")

    def result_handler(
        self,
        ten_env: TenEnvTester,
        result: CmdResult | None,
        error: TenError | None,
    ):
        """
        This method is the callback for send_cmd.
        It receives the result of the command execution.
        """
        print(
            f"[TEST_RUNNER] Result handler called. Received result: {result}, error: {error}"
        )
        self.test_result = result
        self.test_error = error
        self.test_event.set()  # Signal that the test is complete
        print("[TEST_RUNNER] Test event set.")

    def wait_for_result(self, timeout=10):
        """
        Waits for the test_event to be set by the result_handler.
        """
        return self.test_event.wait(timeout)


def test_send_message_via_tester():
    """
    The main test function that uses the ExtensionTester.
    """
    print("\n[PYTEST] Starting test_send_message_via_tester...")
    tester = TestDingTalkBot()

    print("[PYTEST] Reading property.json...")
    # Get the path to property.json
    current_dir = os.path.dirname(os.path.abspath(__file__))
    property_json_path = os.path.join(current_dir, "..", "property.json")

    with open(property_json_path, "r") as f:
        property_json_content = f.read()

    print("[PYTEST] Setting up test mode...")
    # Set up the tester to run the 'dingtalk_bot_tool_python' extension
    tester.set_test_mode_single(
        "dingtalk_bot_tool_python", property_json_content
    )

    print("[PYTEST] Running the tester...")
    # Run the test
    run_error = tester.run()
    print(f"[PYTEST] Tester run finished. Error: {run_error}")
    assert run_error is None, f"Tester failed to run: {run_error}"

    print("[PYTEST] Waiting for result...")
    # Wait for the async result
    completed = tester.wait_for_result()
    print(f"[PYTEST] Wait completed: {completed}")
    assert (
        completed
    ), "Test timed out, did not receive a result from the extension."

    print("[PYTEST] Asserting results...")
    # Assertions on the result
    assert (
        tester.test_error is None
    ), f"Command execution returned an error: {tester.test_error}"
    assert tester.test_result is not None, "Command did not return a result."

    # The extension should return a CmdResult with the name 'tool_call_result'
    # The new extension logic returns OK/ERROR status and a 'result' property.
    assert tester.test_result.get_status_code() == StatusCode.OK

    result_str, err = tester.test_result.get_property_string("result")
    assert err is None

    print(f"Result property string: {result_str}")
    result_json = json.loads(result_str)

    # The actual content is inside the 'content' field of the result property
    assert "Message sent successfully" in result_json.get("content")

    # The DingTalk API response is also embedded in the content
    assert '"errcode": 0' in result_json.get("content")

    print("Test passed successfully!")
