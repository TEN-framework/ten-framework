import asyncio

from conversation_recorder.extension import ConversationRecorderExtension


class FakeTenEnv:
    def log_info(self, _message: str) -> None:
        pass

    def log_error(self, _message: str) -> None:
        pass


class FakeStorage:
    actual_file_path = "/tmp/test.wav"

    def __init__(self) -> None:
        self.opened = False
        self.closed = False

    def open(self) -> None:
        self.opened = True

    def close(self) -> None:
        self.closed = True


def test_start_recording_before_on_start_initializes_event_loop() -> None:
    async def run_scenario() -> None:
        extension = ConversationRecorderExtension.__new__(
            ConversationRecorderExtension
        )
        storage = FakeStorage()
        extension.is_recording = False
        extension.recording_task = None
        extension.loop = None
        extension.storage = storage
        env = FakeTenEnv()

        assert extension.loop is None

        await extension.start_recording(env)
        await extension.stop_recording(env)

        assert extension.loop is asyncio.get_running_loop()
        assert storage.opened is True
        assert storage.closed is True

    asyncio.run(run_scenario())
