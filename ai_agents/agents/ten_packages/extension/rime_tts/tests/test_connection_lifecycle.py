#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
import asyncio

from rime_tts.rime_tts import RimeTTSClient, RimeTTSynthesizer


def test_replacement_disconnect_disables_old_connection_callbacks() -> None:
    disconnects: list[tuple[int, str, str, str]] = []

    async def on_disconnected(*, code, message, vendor_info) -> None:
        disconnects.append(
            (code, message, vendor_info.code, vendor_info.message)
        )

    synthesizer = RimeTTSynthesizer.__new__(RimeTTSynthesizer)
    synthesizer.ws = object()
    synthesizer.on_connection_connecting = object()
    synthesizer.on_connection_connected = object()
    synthesizer.on_connection_disconnected = on_disconnected
    synthesizer.vendor = "rime"

    asyncio.run(synthesizer.suppress_connection_callbacks_for_replacement())

    assert disconnects == [(0, "closed", "0", "closed")]
    assert synthesizer.on_connection_connecting is None
    assert synthesizer.on_connection_connected is None
    assert synthesizer.on_connection_disconnected is None


def test_replacement_continues_when_disconnect_report_fails() -> None:
    async def on_disconnected(**_kwargs) -> None:
        raise RuntimeError("report failed")

    synthesizer = RimeTTSynthesizer.__new__(RimeTTSynthesizer)
    synthesizer.ws = object()
    synthesizer.on_connection_connecting = object()
    synthesizer.on_connection_connected = object()
    synthesizer.on_connection_disconnected = on_disconnected
    synthesizer.vendor = "rime"
    synthesizer.ten_env = type(
        "TenEnvStub",
        (),
        {"log_error": staticmethod(lambda _message: None)},
    )()

    asyncio.run(synthesizer.suppress_connection_callbacks_for_replacement())

    assert synthesizer.on_connection_connecting is None
    assert synthesizer.on_connection_connected is None
    assert synthesizer.on_connection_disconnected is None


def test_reset_reports_disconnect_before_starting_replacement() -> None:
    events: list[str] = []

    class OldSynthesizer:
        async def suppress_connection_callbacks_for_replacement(self) -> None:
            events.append("disconnected")

        def cancel(self) -> None:
            events.append("cancelled")

    client = RimeTTSClient.__new__(RimeTTSClient)
    client.ten_env = type(
        "TenEnvStub",
        (),
        {"log_debug": staticmethod(lambda _message: None)},
    )()
    client.synthesizer = OldSynthesizer()
    client.cancelled_synthesizers = []

    def create_replacement() -> object:
        events.append("replacement_created")
        return object()

    client._create_synthesizer = create_replacement

    asyncio.run(client.reset_synthesizer())

    assert events == ["disconnected", "cancelled", "replacement_created"]


def test_concurrent_cancel_keeps_its_replacement() -> None:
    async def run_scenario() -> None:
        reporting_started = asyncio.Event()
        finish_reporting = asyncio.Event()

        class Synthesizer:
            def __init__(self) -> None:
                self.cancelled = False

            async def suppress_connection_callbacks_for_replacement(
                self,
            ) -> None:
                reporting_started.set()
                await finish_reporting.wait()

            def cancel(self) -> None:
                self.cancelled = True

        old_synthesizer = Synthesizer()
        replacement = Synthesizer()
        replacements = [replacement]

        client = RimeTTSClient.__new__(RimeTTSClient)
        client.ten_env = type(
            "TenEnvStub",
            (),
            {"log_debug": staticmethod(lambda _message: None)},
        )()
        client.response_msgs = None
        client.synthesizer = old_synthesizer
        client.cancelled_synthesizers = []
        client._create_synthesizer = replacements.pop

        reset_task = asyncio.create_task(client.reset_synthesizer())
        await reporting_started.wait()

        client.cancel()
        finish_reporting.set()
        await reset_task

        assert client.synthesizer is replacement
        assert replacement.cancelled is False

    asyncio.run(run_scenario())
