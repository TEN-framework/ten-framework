import asyncio
from unittest.mock import AsyncMock, MagicMock, patch

from ten_ai_base.message import ModuleErrorCode

from xai_asr_python.extension import XAIASRExtension
from xai_asr_python.reconnect_manager import ReconnectManager


def test_reconnect_manager_escalates_after_max_attempts():
    async def _run():
        errors = []
        manager = ReconnectManager(
            base_delay=0,
            max_delay=0,
            max_attempts=4,
            logger=MagicMock(),
        )

        async def failing_connect():
            raise RuntimeError("disconnect")

        async def error_handler(error, vendor_info=None):
            errors.append((error.code, vendor_info))

        for _ in range(4):
            await manager.handle_reconnect(
                failing_connect,
                error_handler,
                vendor_name="xai",
                vendor_code="connect_failed",
            )

        assert [code for code, _ in errors] == [
            int(ModuleErrorCode.NON_FATAL_ERROR.value),
            int(ModuleErrorCode.NON_FATAL_ERROR.value),
            int(ModuleErrorCode.NON_FATAL_ERROR.value),
            int(ModuleErrorCode.FATAL_ERROR.value),
        ]
        assert all(vendor_info is not None for _, vendor_info in errors)
        assert all(vendor_info.vendor == "xai" for _, vendor_info in errors)
        assert all(
            vendor_info.code == "connect_failed" for _, vendor_info in errors
        )

    asyncio.run(_run())


def test_reconnect_counter_resets_after_success():
    manager = ReconnectManager(base_delay=0, max_delay=0, max_attempts=4)
    manager.attempts = 3
    manager.mark_connection_successful()
    assert manager.attempts == 0


def test_on_close_expected_after_finalize_reconnects_fresh_socket():
    """Regression: xAI STT is a single-utterance protocol — the server
    closes the socket after transcript.done. Previously on_close returned
    early when _close_expected was True, leaving recognition=None and
    dropping every subsequent turn. Verify on_close now schedules a
    fresh _connect_recognition() so the next utterance can be captured.
    """

    async def _run():
        extension = XAIASRExtension("xai_asr_python")
        extension.ten_env = MagicMock()
        extension.reconnect_manager = MagicMock()
        extension._connect_recognition = AsyncMock()
        extension.send_asr_error = AsyncMock()

        # State right after finalize() ran and the vendor closed the socket.
        extension._stop_requested = False
        extension._close_expected = True
        extension.recognition = MagicMock()

        await extension.on_close()
        assert extension._finalize_reconnect_task is not None
        # The reconnect is scheduled as a task; wait for it to complete.
        pending = [
            t for t in asyncio.all_tasks() if t is not asyncio.current_task()
        ]
        if pending:
            await asyncio.gather(*pending, return_exceptions=True)

        # _close_expected reset, recognition cleared, fresh connect attempted.
        assert extension._close_expected is False
        assert extension.recognition is None
        assert extension._finalize_reconnect_task is None
        extension._connect_recognition.assert_awaited_once()
        # The backoff reconnect path was NOT used — this is a planned cycle.
        extension.send_asr_error.assert_not_awaited()

    asyncio.run(_run())


def test_on_close_expected_falls_back_to_backoff_if_reconnect_fails():
    """If the planned reconnect after finalize raises, the extension
    should fall back to the bounded reconnect-manager backoff path
    instead of silently leaving the session dead."""

    async def _run():
        extension = XAIASRExtension("xai_asr_python")
        extension.ten_env = MagicMock()
        extension.reconnect_manager = ReconnectManager(
            base_delay=0,
            max_delay=0,
            max_attempts=2,
            logger=MagicMock(),
        )
        extension.send_asr_error = AsyncMock()
        extension._connect_recognition = AsyncMock(
            side_effect=RuntimeError("xai down")
        )

        extension._stop_requested = False
        extension._close_expected = True
        extension.recognition = MagicMock()

        await extension.on_close()
        assert extension._finalize_reconnect_task is not None
        pending = [
            t for t in asyncio.all_tasks() if t is not asyncio.current_task()
        ]
        if pending:
            await asyncio.gather(*pending, return_exceptions=True)

        # First attempt is the planned reconnect; on failure the backoff
        # path runs max_attempts=2 times.
        assert extension._finalize_reconnect_task is None
        assert extension._connect_recognition.await_count >= 3
        assert extension.send_asr_error.await_count >= 1

    asyncio.run(_run())


def test_stop_connection_cancels_pending_finalize_reconnect_task():
    async def _run():
        extension = XAIASRExtension("xai_asr_python")
        extension.ten_env = MagicMock()
        recognition = MagicMock()
        recognition.close = AsyncMock()
        extension.recognition = recognition

        blocker = asyncio.Event()

        async def pending_reconnect():
            await blocker.wait()

        task = asyncio.create_task(pending_reconnect())
        extension._finalize_reconnect_task = task

        await extension.stop_connection()
        await asyncio.gather(task, return_exceptions=True)

        assert extension._stop_requested is True
        recognition.close.assert_awaited_once()
        assert extension.recognition is None
        assert extension._finalize_reconnect_task is None
        assert task.cancelled()

    asyncio.run(_run())


def test_stop_connection_cancels_inflight_connect_recognition():
    """Race coverage: stop_connection() is called while a finalize-reconnect
    task is mid-flight inside _connect_recognition() (recognition partially
    created, recognition.start() still awaiting). Verify the in-flight task is
    cancelled cleanly — recognition ends up None, no second reconnect is
    attempted, and the cancelled task leaks no exception."""

    async def _run():
        extension = XAIASRExtension("xai_asr_python")
        extension.ten_env = MagicMock()
        extension.reconnect_manager = MagicMock()
        extension.send_asr_error = AsyncMock()

        recognition = MagicMock()
        recognition.close = AsyncMock()

        connect_started = asyncio.Event()
        connect_calls = 0

        async def inflight_connect():
            nonlocal connect_calls
            connect_calls += 1
            # Mimic _connect_recognition having partially created the
            # recognition object before recognition.start() returns.
            extension.recognition = recognition
            connect_started.set()
            # Block here as if awaiting recognition.start(); only a cancel
            # should ever unblock this.
            await asyncio.Event().wait()

        extension._connect_recognition = inflight_connect

        # State right after the vendor closed the socket post-finalize.
        extension._stop_requested = False
        extension._close_expected = True
        extension.recognition = MagicMock()

        await extension.on_close()
        task = extension._finalize_reconnect_task
        assert task is not None

        # Let the reconnect task actually reach the in-flight await point.
        await connect_started.wait()
        assert extension.recognition is recognition

        # Now race a stop in while the connect is still suspended.
        await extension.stop_connection()

        # The cancelled task must settle without leaking an exception.
        result = await asyncio.gather(task, return_exceptions=True)
        assert task.cancelled()
        assert isinstance(result[0], asyncio.CancelledError)

        assert extension._stop_requested is True
        assert extension.recognition is None
        recognition.close.assert_awaited_once()
        assert extension._finalize_reconnect_task is None
        # No second reconnect: connect ran exactly once and the backoff
        # error path was never entered.
        assert connect_calls == 1
        extension.send_asr_error.assert_not_awaited()

    asyncio.run(_run())


def test_stop_connection_cancels_real_connect_recognition_at_start():
    """Same race as above, but drives the *real* _connect_recognition body so
    we exercise the production line that assigns self.recognition before
    recognition.start() returns. recognition.start() is held open; a
    concurrent stop_connection() must cancel cleanly, close the half-started
    recognition, and leave no second reconnect or leaked exception."""

    async def _run():
        extension = XAIASRExtension("xai_asr_python")
        extension.ten_env = MagicMock()
        extension.reconnect_manager = MagicMock()
        extension.send_asr_error = AsyncMock()
        extension.audio_timeline = MagicMock()

        extension.config = MagicMock()
        extension.config.params = {"api_key": "test-key"}

        start_entered = asyncio.Event()

        recognition_instance = MagicMock()
        recognition_instance.is_connected.return_value = False
        recognition_instance.close = AsyncMock()

        async def blocking_start(timeout=None):
            # We are now mid recognition.start(); self.recognition has already
            # been assigned by the real _connect_recognition.
            start_entered.set()
            await asyncio.Event().wait()

        recognition_instance.start = AsyncMock(side_effect=blocking_start)

        extension._stop_requested = False
        extension._close_expected = True
        extension.recognition = None

        with patch(
            "xai_asr_python.extension.XAIASRRecognition",
            return_value=recognition_instance,
        ) as recognition_cls:
            extension._schedule_finalize_reconnect()
            task = extension._finalize_reconnect_task
            assert task is not None

            # Let the real _connect_recognition run up to recognition.start().
            await start_entered.wait()
            assert extension.recognition is recognition_instance

            await extension.stop_connection()

            result = await asyncio.gather(task, return_exceptions=True)
            assert task.cancelled()
            assert isinstance(result[0], asyncio.CancelledError)

            assert extension._stop_requested is True
            assert extension.recognition is None
            recognition_instance.close.assert_awaited_once()
            assert extension._finalize_reconnect_task is None
            # Exactly one recognition was constructed: no second reconnect.
            assert recognition_cls.call_count == 1
            extension.send_asr_error.assert_not_awaited()

    asyncio.run(_run())


def test_on_close_retries_until_retry_ceiling():
    async def _run():
        extension = XAIASRExtension("xai_asr_python")
        extension.ten_env = MagicMock()
        extension.reconnect_manager = ReconnectManager(
            base_delay=0,
            max_delay=0,
            max_attempts=4,
            logger=MagicMock(),
        )
        extension.send_asr_error = AsyncMock()
        extension._connect_recognition = AsyncMock(
            side_effect=RuntimeError("disconnect")
        )

        await extension.on_close()

        assert extension._connect_recognition.await_count == 4
        observed_codes = [
            call.args[0].code
            for call in extension.send_asr_error.await_args_list
        ]
        assert observed_codes == [
            int(ModuleErrorCode.NON_FATAL_ERROR.value),
            int(ModuleErrorCode.NON_FATAL_ERROR.value),
            int(ModuleErrorCode.NON_FATAL_ERROR.value),
            int(ModuleErrorCode.FATAL_ERROR.value),
        ]
        observed_vendor_infos = [
            call.args[1] for call in extension.send_asr_error.await_args_list
        ]
        assert all(
            vendor_info.vendor == "xai" for vendor_info in observed_vendor_infos
        )
        assert all(
            vendor_info.code == "connect_failed"
            for vendor_info in observed_vendor_infos
        )

    asyncio.run(_run())
