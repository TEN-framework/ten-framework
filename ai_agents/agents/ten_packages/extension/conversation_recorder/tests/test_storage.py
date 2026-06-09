import importlib.util
import os
import shutil
from pathlib import Path

import pytest


storage_spec = importlib.util.spec_from_file_location(
    "conversation_recorder_storage",
    Path(__file__).parent.parent / "storage.py",
)
storage_module = importlib.util.module_from_spec(storage_spec)
storage_spec.loader.exec_module(storage_module)
GCSStorage = storage_module.GCSStorage


class FakeBlob:
    def __init__(self, error=None):
        self.error = error
        self.uploaded_path = None

    def upload_from_filename(self, path):
        self.uploaded_path = path
        if self.error:
            raise self.error


class FakeBucket:
    def __init__(self, blob):
        self.test_blob = blob
        self.requested_name = None

    def blob(self, name):
        self.requested_name = name
        return self.test_blob


def create_storage(bucket):
    storage = GCSStorage(
        bucket_name="recordings-bucket",
        filename="audio.wav",
        upload_prefix="sessions/test/",
    )
    storage._get_gcs_client = lambda: (None, bucket)
    storage.open()
    storage.write(b"\x00\x00")
    return storage


def test_gcs_close_reports_remote_path_and_removes_uploaded_temp_file():
    blob = FakeBlob()
    bucket = FakeBucket(blob)
    storage = create_storage(bucket)
    local_path = storage.actual_file_path

    storage.close()

    assert bucket.requested_name == "sessions/test/audio.wav"
    assert blob.uploaded_path == local_path
    assert storage.actual_file_path == (
        "gs://recordings-bucket/sessions/test/audio.wav"
    )
    assert not os.path.exists(local_path)


def test_gcs_close_keeps_local_file_when_upload_fails():
    bucket = FakeBucket(FakeBlob(RuntimeError("upload rejected")))
    storage = create_storage(bucket)
    local_path = storage.actual_file_path

    try:
        with pytest.raises(RuntimeError, match="upload rejected"):
            storage.close()

        assert storage.actual_file_path == local_path
        assert os.path.exists(local_path)
    finally:
        shutil.rmtree(storage.temp_dir)
