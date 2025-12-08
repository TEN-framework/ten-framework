import asyncio
from typing import Callable, Any, AsyncGenerator
import inspect
import traceback

from ten_runtime import (
    AsyncTenEnvTester,
    AsyncExtensionTester,
    Cmd,
    Data,
    AudioFrame,
    VideoFrame,
    CmdResult,
    TenError,
    TenErrorCode,
    StatusCode,
)


class TenTestContext:
    """
    Context for AAA(Arrange, Act, Assert)-style testing that collects messages from the extension.

    Provides methods to send messages to the extension and wait for expected
    responses with configurable timeouts.
    """

    def __init__(self, ten_env: AsyncTenEnvTester, timeout: float = 5.0):
        self.ten_env = ten_env
        self.timeout = timeout
        self.received_cmds: asyncio.Queue[Cmd] = asyncio.Queue()
        self.received_data: asyncio.Queue[Data] = asyncio.Queue()
        self.received_audio: asyncio.Queue[AudioFrame] = asyncio.Queue()
        self.received_video: asyncio.Queue[VideoFrame] = asyncio.Queue()

    async def send_cmd(
        self, cmd: Cmd
    ) -> tuple[CmdResult | None, TenError | None]:
        """Send command and wait for result"""
        return await self.ten_env.send_cmd(cmd)

    async def send_data(self, data: Data) -> None:
        """Send data to extension"""
        await self.ten_env.send_data(data)

    async def send_audio_frame(self, frame: AudioFrame) -> None:
        """Send audio frame to extension"""
        await self.ten_env.send_audio_frame(frame)

    async def send_video_frame(self, frame: VideoFrame) -> None:
        """Send video frame to extension"""
        await self.ten_env.send_video_frame(frame)

    async def expect_data(
        self,
        name: str | None = None,
        timeout: float | None = None,
        put_back: bool = False,
    ) -> Data:
        """
        Wait for and return a data message, optionally filtering by name.

        Args:
            name: Optional data name to filter by
            timeout: Optional timeout in seconds (defaults to context timeout)
            put_back: If True, put unmatched messages back in queue.
                     If False (default), drop unmatched messages.

        Returns:
            Data

        Raises:
            TimeoutError: If timeout expires before receiving expected data
        """
        timeout = timeout or self.timeout
        end_time = asyncio.get_event_loop().time() + timeout

        while True:
            remaining = end_time - asyncio.get_event_loop().time()
            if remaining <= 0:
                raise TimeoutError(
                    f"Timeout waiting for data{f' with name={name}' if name else ''}"
                )

            try:
                attempt_timeout = min(remaining, 0.1)
                data = await asyncio.wait_for(
                    self.received_data.get(), timeout=attempt_timeout
                )
                if name and data.get_name() != name:
                    if put_back:
                        # Put back and wait before retrying
                        await self.received_data.put(data)
                        await asyncio.sleep(
                            0.05
                        )  # Small delay to avoid busy loop
                    # Otherwise drop the message
                    continue
                return data
            except asyncio.TimeoutError:
                # Continue loop to check remaining time
                continue

    async def expect_cmd(
        self,
        name: str | None = None,
        timeout: float | None = None,
        put_back: bool = False,
    ) -> Cmd:
        """
        Wait for and return a command, optionally filtering by name.

        Args:
            name: Optional command name to filter by
            timeout: Optional timeout in seconds (defaults to context timeout)
            put_back: If True, put unmatched messages back in queue.
                     If False (default), drop unmatched messages.

        Returns:
            Cmd

        Raises:
            TimeoutError: If timeout expires before receiving expected command
        """
        timeout = timeout or self.timeout
        end_time = asyncio.get_event_loop().time() + timeout

        while True:
            remaining = end_time - asyncio.get_event_loop().time()
            if remaining <= 0:
                raise TimeoutError(
                    f"Timeout waiting for cmd{f' with name={name}' if name else ''}"
                )

            try:
                attempt_timeout = min(remaining, 0.1)
                cmd = await asyncio.wait_for(
                    self.received_cmds.get(), timeout=attempt_timeout
                )
                if name and cmd.get_name() != name:
                    if put_back:
                        # Put back and wait before retrying
                        await self.received_cmds.put(cmd)
                        await asyncio.sleep(
                            0.05
                        )  # Small delay to avoid busy loop
                    # Otherwise drop the message
                    continue
                return cmd
            except asyncio.TimeoutError:
                continue

    async def expect_audio_frame(
        self, timeout: float | None = None
    ) -> AudioFrame:
        """
        Wait for and return an audio frame.

        Args:
            timeout: Optional timeout in seconds (defaults to context timeout)

        Returns:
            AudioFrame

        Raises:
            TimeoutError: If timeout expires before receiving audio frame
        """
        timeout = timeout or self.timeout
        try:
            return await asyncio.wait_for(
                self.received_audio.get(), timeout=timeout
            )
        except asyncio.TimeoutError as e:
            raise TimeoutError("Timeout waiting for audio frame") from e

    async def expect_video_frame(
        self, timeout: float | None = None
    ) -> VideoFrame:
        """
        Wait for and return a video frame.

        Args:
            timeout: Optional timeout in seconds (defaults to context timeout)

        Returns:
            VideoFrame

        Raises:
            TimeoutError: If timeout expires before receiving video frame
        """
        timeout = timeout or self.timeout
        try:
            return await asyncio.wait_for(
                self.received_video.get(), timeout=timeout
            )
        except asyncio.TimeoutError as e:
            raise TimeoutError("Timeout waiting for video frame") from e

    async def collect_all_data(
        self, name: str | None = None, timeout: float = 1.0
    ) -> list[Data]:
        """
        Collect all data messages (optionally filtered by name) until timeout.
        Drop unmatched messages.

        Args:
            name: Optional data name to filter by
            timeout: Total timeout for collecting all messages

        Returns:
            List of collected Data
        """
        collected = []
        end_time = asyncio.get_event_loop().time() + timeout

        while True:
            remaining = end_time - asyncio.get_event_loop().time()
            if remaining <= 0:
                break

            try:
                data = await self.expect_data(name, timeout=remaining)
                collected.append(data)
            except TimeoutError:
                break
        return collected

    async def collect_all_cmds(
        self, name: str | None = None, timeout: float = 1.0
    ) -> list[Cmd]:
        """
        Collect all commands (optionally filtered by name) until timeout.
        Drop unmatched messages.

        Args:
            name: Optional command name to filter by
            timeout: Total timeout for collecting all commands

        Returns:
            List of collected Cmd
        """
        collected = []
        end_time = asyncio.get_event_loop().time() + timeout

        while True:
            remaining = end_time - asyncio.get_event_loop().time()
            if remaining <= 0:
                break

            try:
                cmd = await self.expect_cmd(name, timeout=remaining)
                collected.append(cmd)
            except TimeoutError:
                break
        return collected

    async def collect_all_audio_frames(
        self, timeout: float = 1.0
    ) -> list[AudioFrame]:
        """
        Collect all audio frames until timeout.

        Args:
            timeout: Total timeout for collecting all frames

        Returns:
            List of collected AudioFrames
        """
        collected = []
        end_time = asyncio.get_event_loop().time() + timeout

        while True:
            remaining = end_time - asyncio.get_event_loop().time()
            if remaining <= 0:
                break

            try:
                frame = await self.expect_audio_frame(timeout=remaining)
                collected.append(frame)
            except TimeoutError:
                break
        return collected

    async def collect_all_video_frames(
        self, timeout: float = 1.0
    ) -> list[VideoFrame]:
        """
        Collect all video frames until timeout.

        Args:
            timeout: Total timeout for collecting all frames

        Returns:
            List of collected VideoFrames
        """
        collected = []
        end_time = asyncio.get_event_loop().time() + timeout

        while True:
            remaining = end_time - asyncio.get_event_loop().time()
            if remaining <= 0:
                break

            try:
                frame = await self.expect_video_frame(timeout=remaining)
                collected.append(frame)
            except TimeoutError:
                break
        return collected

    async def assert_no_data(
        self, name: str | None = None, timeout: float = 1.0
    ) -> None:
        """
        Assert that no data with given name arrives within timeout.

        Args:
            name: Optional data name to filter by
            timeout: How long to wait for unwanted data

        Raises:
            AssertionError: If data is received
        """
        try:
            data = await self.expect_data(name, timeout=timeout)
            raise AssertionError(
                f"Expected no data{f' with name={name}' if name else ''}, "
                f"but received: {data.get_name()}"
            )
        except TimeoutError:
            # This is expected - no data received
            pass

    async def assert_no_cmd(
        self, name: str | None = None, timeout: float = 1.0
    ) -> None:
        """
        Assert that no command with given name arrives within timeout.

        Args:
            name: Optional command name to filter by
            timeout: How long to wait for unwanted command

        Raises:
            AssertionError: If command is received
        """
        try:
            cmd = await self.expect_cmd(name, timeout=timeout)
            raise AssertionError(
                f"Expected no cmd{f' with name={name}' if name else ''}, "
                f"but received: {cmd.get_name()}"
            )
        except TimeoutError:
            # This is expected - no command received
            pass

    async def assert_no_audio_frame(self, timeout: float = 1.0) -> None:
        """
        Assert that no audio frame arrives within timeout.

        Args:
            timeout: How long to wait for unwanted audio frame

        Raises:
            AssertionError: If audio frame is received
        """
        try:
            frame = await self.expect_audio_frame(timeout=timeout)
            raise AssertionError(
                f"Expected no audio frame, but received one with "
                f"timestamp: {frame.get_timestamp()}"
            )
        except TimeoutError:
            # This is expected - no audio frame received
            pass

    async def assert_no_video_frame(self, timeout: float = 1.0) -> None:
        """
        Assert that no video frame arrives within timeout.

        Args:
            timeout: How long to wait for unwanted video frame

        Raises:
            AssertionError: If video frame is received
        """
        try:
            frame = await self.expect_video_frame(timeout=timeout)
            raise AssertionError(
                f"Expected no video frame, but received one with "
                f"timestamp: {frame.get_timestamp()}"
            )
        except TimeoutError:
            # This is expected - no video frame received
            pass


class _TenTestRunner(AsyncExtensionTester):
    """
    Internal runner for general TEN tests.

    Wraps AsyncExtensionTester to provide AAA(Arrange, Act, Assert)-style testing with proper
    error handling and TenError reporting.
    """

    def __init__(
        self,
        test_func: Callable[[TenTestContext], Any],
        timeout: float,
        mock_cmd_response: dict[str, Callable[[Cmd], CmdResult]] | None = None,
        mock_cmd_stream_response: (
            dict[str, Callable[[Cmd], AsyncGenerator[CmdResult, None]]] | None
        ) = None,
        mock_data_response: dict[str, Callable[[Data], Data]] | None = None,
    ):
        super().__init__()
        self._test_func = test_func
        self._test_task: asyncio.Task | None = None
        self._test_timeout = timeout
        self._ctx: TenTestContext | None = None
        self._mock_cmd_response = mock_cmd_response
        self._mock_cmd_stream_response = mock_cmd_stream_response
        self._mock_data_response = mock_data_response
        self._pytest_fixtures: dict[str, Any] = {}  # Store fixtures from pytest

    async def on_init(self, ten_env: AsyncTenEnvTester) -> None:
        self._ctx = TenTestContext(ten_env, timeout=self._test_timeout)

    async def on_start(self, ten_env: AsyncTenEnvTester) -> None:
        self._test_task = asyncio.create_task(self._run_test(ten_env))

    async def on_deinit(self, ten_env: AsyncTenEnvTester) -> None:
        if self._test_task:
            self._test_task.cancel()
            try:
                await self._test_task
            except asyncio.CancelledError:
                pass

    async def _run_test(self, ten_env: AsyncTenEnvTester) -> None:
        try:
            assert self._ctx is not None
            # Call test function with ctx as first arg, then pytest fixtures
            await self._test_func(self._ctx, **self._pytest_fixtures)

            ten_env.stop_test()

        except AssertionError as e:
            error_msg = f"Assertion failed: {e}\n{traceback.format_exc()}"
            error = TenError.create(TenErrorCode.ErrorCodeGeneric, error_msg)
            ten_env.stop_test(error)

        except TimeoutError as e:
            error_msg = f"Expectation timeout: {e}\n{traceback.format_exc()}"
            error = TenError.create(TenErrorCode.ErrorCodeTimeout, error_msg)
            ten_env.stop_test(error)

        except Exception as e:
            # Unexpected error
            error_msg = f"Test error: {e}\n{traceback.format_exc()}"
            error = TenError.create(TenErrorCode.ErrorCodeGeneric, error_msg)
            ten_env.stop_test(error)

    async def on_cmd(self, ten_env: AsyncTenEnvTester, cmd: Cmd) -> None:
        assert self._ctx is not None

        await self._ctx.received_cmds.put(cmd.clone())
        cmd_name = cmd.get_name()

        stream_mocks = self._mock_cmd_stream_response or {}
        if cmd_name in stream_mocks:
            generator = stream_mocks[cmd_name](cmd)
            async for result in generator:
                await ten_env.return_result(result)
            return

        mock_responses = self._mock_cmd_response or {}
        if cmd_name in mock_responses:
            result = mock_responses[cmd_name](cmd)
            await ten_env.return_result(result)
            return

        result = CmdResult.create(StatusCode.OK, cmd)
        await ten_env.return_result(result)

    async def on_data(self, ten_env: AsyncTenEnvTester, data: Data) -> None:
        assert self._ctx is not None

        await self._ctx.received_data.put(data.clone())
        data_name = data.get_name()

        mock_responses = self._mock_data_response or {}
        if data_name in mock_responses:
            response_data = mock_responses[data_name](data)
            await ten_env.send_data(response_data)

    async def on_audio_frame(
        self, ten_env: AsyncTenEnvTester, audio_frame: AudioFrame
    ) -> None:
        assert self._ctx is not None

        await self._ctx.received_audio.put(audio_frame)

    async def on_video_frame(
        self, ten_env: AsyncTenEnvTester, video_frame: VideoFrame
    ) -> None:
        assert self._ctx is not None

        await self._ctx.received_video.put(video_frame)


def ten_test(
    addon_name: str,
    property_json_str: str | None = None,
    timeout: float = 5.0,
    *,
    mock_cmd_response: dict[str, Callable[[Cmd], CmdResult]] | None = None,
    mock_cmd_stream_response: (
        dict[str, Callable[[Cmd], AsyncGenerator[CmdResult, None]]] | None
    ) = None,
    mock_data_response: dict[str, Callable[[Data], Data]] | None = None,
):
    """
    Decorator for AAA(Arrange, Act, Assert)-style TEN extension tests.

    Creates a test runner that executes the test scenario with proper error
    handling and message collection. Auto-runs when used with pytest.

    Test Function Signature:
        The first parameter MUST be 'ctx: TenTestContext' (injected by TEN runtime).
        Subsequent parameters can be pytest fixtures.

    Args:
        addon_name: Name of the extension to test
        property_json_str: Optional JSON string of extension properties
        timeout: Default timeout for expect operations (seconds)
        mock_cmd_response: Map from cmd name to response function.
            When SUT sends a command to tester, if cmd name is in this map,
            the lambda is called to generate the CmdResult to return to SUT.
        mock_cmd_stream_response: Map from cmd name to async generator function.
            When SUT sends a command to tester, if cmd name is in this map,
            the lambda is called to generate an AsyncGenerator that yields
            CmdResult objects to stream back to SUT (e.g., for LLM responses).
        mock_data_response: Map from data name to response function.
            When SUT sends data to tester, if data name is in this map,
            the lambda is called to generate Data to send back to SUT.

    Returns:
        Wrapper function that pytest can discover and execute

    Example - Basic test:
        @ten_test("my_extension", json.dumps(config), timeout=10.0)
        async def test_my_extension(ctx: TenTestContext):
            # Arrange
            input_data = Data.create("input")
            input_data.set_property_string("text", "Hello")

            # Act
            await ctx.send_data(input_data)

            # Assert
            output = await ctx.expect_data("output", timeout=2.0)
            text, _ = output.get_property_string("text")
            assert text == "Hello World"

    Example - With pytest fixtures:
        @pytest.fixture
        def sample_text():
            return "Hello from fixture"

        @ten_test("my_extension", timeout=10.0)
        async def test_with_fixture(ctx: TenTestContext, sample_text):
            # ctx is injected by TEN runtime (first parameter)
            # sample_text is a pytest fixture (subsequent parameters)
            data = Data.create("input")
            data.set_property_string("text", sample_text)
            await ctx.send_data(data)

            output = await ctx.expect_data("output")
            text, _ = output.get_property_string("text")
            assert text == "Hello from fixture"

    Example - With mocks:
        def mock_query_cmd(cmd: Cmd) -> CmdResult:
            result = CmdResult.create(StatusCode.OK, cmd)
            result.set_property_string("answer", "42")
            return result

        @ten_test(
            "my_extension",
            timeout=10.0,
            mock_cmd_response={"query": mock_query_cmd}
        )
        async def test_with_mocks(ctx: TenTestContext):
            # Trigger SUT to send "query" cmd to tester
            trigger = Data.create("trigger_query")
            await ctx.send_data(trigger)

            # SUT should receive mocked response and send result back
            result = await ctx.expect_data("query_result")
            answer, _ = result.get_property_string("answer")
            assert answer == "42"
    """

    def decorator(test_func: Callable[[TenTestContext], Any]):

        # Get the original signature and extract fixture parameters (skip first 'ctx' param)
        sig = inspect.signature(test_func)
        params = list(sig.parameters.values())[1:]  # Skip 'ctx' parameter

        # Create wrapper function that accepts pytest fixtures
        def wrapper(**kwargs) -> None:
            tester = _TenTestRunner(
                test_func,
                timeout=timeout,
                mock_cmd_response=mock_cmd_response,
                mock_cmd_stream_response=mock_cmd_stream_response,
                mock_data_response=mock_data_response,
            )
            tester.set_test_mode_single(addon_name, property_json_str)

            # Store fixtures for the test runner to pass to test_func
            tester._pytest_fixtures = kwargs
            err = tester.run()
            assert err is None, f"Test failed: {err.error_message()}"

        # Preserve metadata and signature for pytest
        wrapper.__name__ = test_func.__name__
        wrapper.__doc__ = test_func.__doc__
        wrapper.__signature__ = inspect.Signature(parameters=params)

        return wrapper

    return decorator
