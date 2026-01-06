from abc import ABC, abstractmethod
import wave
import os
import tempfile
from datetime import datetime


class BaseStorage(ABC):
    @abstractmethod
    def open(self):
        pass

    @abstractmethod
    def write(self, data: bytes):
        pass

    @abstractmethod
    def flush(self):
        pass

    @abstractmethod
    def close(self):
        pass


class LocalStorage(BaseStorage):
    def __init__(
        self,
        file_path,
        channels=1,
        sample_rate=24000,
        sample_width=2,
        filename=None,
    ):
        self.file_path = file_path
        self.channels = channels
        self.sample_rate = sample_rate
        self.sample_width = sample_width
        self.filename = filename
        self.wav_file = None
        self.actual_file_path = None

    def _resolve_file_path(self):
        """Resolve file path, using custom filename or auto-generating if not provided."""
        path = self.file_path

        # Check if path exists and is a file (not a directory)
        if os.path.isfile(path):
            # It's an existing file, use it directly
            return path

        # If the path doesn't end with .wav, treat it as a directory
        if not path.lower().endswith(".wav"):
            # Check if path exists as a file (which would be an error)
            if os.path.exists(path) and not os.path.isdir(path):
                # There's a file with this name, we can't use it as a directory
                # Use a different directory name
                path = path + "_recordings"

            # Ensure directory exists
            os.makedirs(path, exist_ok=True)

            # Use custom filename if provided, otherwise generate with timestamp
            if self.filename:
                fname = self.filename
                if not fname.lower().endswith(".wav"):
                    fname = fname + ".wav"
            else:
                timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
                fname = f"conversation_{timestamp}.wav"

            path = os.path.join(path, fname)
        else:
            # It's a file path, ensure parent directory exists
            dir_name = os.path.dirname(path)
            if dir_name:
                os.makedirs(dir_name, exist_ok=True)

        return path

    def open(self):
        self.actual_file_path = self._resolve_file_path()
        self.wav_file = wave.open(self.actual_file_path, "wb")
        self.wav_file.setnchannels(self.channels)
        self.wav_file.setsampwidth(self.sample_width)
        self.wav_file.setframerate(self.sample_rate)

    def write(self, data: bytes):
        if self.wav_file:
            self.wav_file.writeframes(data)

    def flush(self):
        if self.wav_file:
            # WAV module doesn't have explicit flush, but we can sync via the underlying file
            try:
                self.wav_file._file.flush()
                os.fsync(self.wav_file._file.fileno())
            except (AttributeError, OSError):
                pass  # Best effort flush

    def close(self):
        if self.wav_file:
            self.wav_file.close()
            self.wav_file = None


class GCSStorage(BaseStorage):
    """
    Storage backend that writes locally first, then uploads to Google Cloud Storage.
    Uses LocalStorage internally for WAV file writing.
    """

    def __init__(
        self,
        bucket_name,
        channels=1,
        sample_rate=24000,
        sample_width=2,
        filename=None,
        project_id=None,
        credentials_path=None,
        upload_prefix=None,
    ):
        self.bucket_name = bucket_name
        self.channels = channels
        self.sample_rate = sample_rate
        self.sample_width = sample_width
        self.filename = filename
        self.project_id = project_id
        self.credentials_path = credentials_path
        self.upload_prefix = upload_prefix or ""
        self.temp_dir = None
        self.local_storage = None
        self.actual_file_path = None
        self._gcs_client = None
        self._bucket = None

    def _get_gcs_client(self):
        """Lazy initialization of GCS client."""
        if self._gcs_client is None:
            from google.cloud import storage

            if self.credentials_path:
                self._gcs_client = storage.Client.from_service_account_json(
                    self.credentials_path, project=self.project_id
                )
            else:
                self._gcs_client = storage.Client(project=self.project_id)
            self._bucket = self._gcs_client.bucket(self.bucket_name)
        return self._gcs_client, self._bucket

    def _generate_filename(self):
        """Generate filename for the recording."""
        if self.filename:
            fname = self.filename
            if not fname.lower().endswith(".wav"):
                fname = fname + ".wav"
            return fname
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        return f"conversation_{timestamp}.wav"

    def open(self):
        # Create a temp directory for the local file
        self.temp_dir = tempfile.mkdtemp(prefix="gcs_recording_")
        temp_file_path = os.path.join(self.temp_dir, self._generate_filename())

        # Use LocalStorage for writing
        self.local_storage = LocalStorage(
            file_path=temp_file_path,
            channels=self.channels,
            sample_rate=self.sample_rate,
            sample_width=self.sample_width,
        )
        self.local_storage.open()
        self.actual_file_path = self.local_storage.actual_file_path

    def write(self, data: bytes):
        if self.local_storage:
            self.local_storage.write(data)

    def flush(self):
        if self.local_storage:
            self.local_storage.flush()

    def close(self):
        if self.local_storage:
            local_path = self.local_storage.actual_file_path
            self.local_storage.close()

            # Upload to GCS
            if local_path and os.path.exists(local_path):
                try:
                    _, bucket = self._get_gcs_client()
                    blob_name = self.upload_prefix.rstrip("/")
                    if blob_name:
                        blob_name = (
                            f"{blob_name}/{os.path.basename(local_path)}"
                        )
                    else:
                        blob_name = os.path.basename(local_path)

                    blob = bucket.blob(blob_name)
                    blob.upload_from_filename(local_path)
                    self.actual_file_path = (
                        f"gs://{self.bucket_name}/{blob_name}"
                    )
                finally:
                    # Cleanup temp file
                    try:
                        os.remove(local_path)
                        if self.temp_dir:
                            os.rmdir(self.temp_dir)
                    except OSError:
                        pass

            self.local_storage = None


class StorageFactory:
    @staticmethod
    def create_storage(storage_type, config):
        sample_rate = config.get("sample_rate", 24000)
        filename = config.get("filename")

        if storage_type == "local":
            return LocalStorage(
                config.get("file_path", "records/conversation.wav"),
                sample_rate=sample_rate,
                filename=filename,
            )
        elif storage_type == "gcs":
            bucket_name = config.get("gcp_bucket_name")
            if not bucket_name:
                raise ValueError("gcp_bucket_name is required for GCS storage")
            return GCSStorage(
                bucket_name=bucket_name,
                sample_rate=sample_rate,
                filename=filename,
                project_id=config.get("gcp_project_id"),
                credentials_path=config.get("gcp_credentials_path"),
                upload_prefix=config.get("gcp_upload_prefix"),
            )
        elif storage_type == "s3":
            bucket_name = config.get("s3_bucket_name")
            if not bucket_name:
                raise ValueError("s3_bucket_name is required for S3 storage")
            return S3Storage(
                bucket_name=bucket_name,
                sample_rate=sample_rate,
                filename=filename,
                access_key_id=config.get("s3_access_key_id"),
                secret_access_key=config.get("s3_secret_access_key"),
                endpoint_url=config.get("s3_endpoint_url"),
                region=config.get("s3_region"),
                upload_prefix=config.get("s3_upload_prefix"),
            )
        return None


class S3Storage(BaseStorage):
    """
    Storage backend that writes locally first, then uploads to S3-compatible storage.
    Supports AWS S3, MinIO, DigitalOcean Spaces, and other S3-compatible services.
    Uses LocalStorage internally for WAV file writing.
    """

    def __init__(
        self,
        bucket_name,
        channels=1,
        sample_rate=24000,
        sample_width=2,
        filename=None,
        access_key_id=None,
        secret_access_key=None,
        endpoint_url=None,
        region=None,
        upload_prefix=None,
    ):
        self.bucket_name = bucket_name
        self.channels = channels
        self.sample_rate = sample_rate
        self.sample_width = sample_width
        self.filename = filename
        self.access_key_id = access_key_id
        self.secret_access_key = secret_access_key
        self.endpoint_url = endpoint_url
        self.region = region
        self.upload_prefix = upload_prefix or ""
        self.temp_dir = None
        self.local_storage = None
        self.actual_file_path = None
        self._s3_client = None

    def _get_s3_client(self):
        """Lazy initialization of S3 client."""
        if self._s3_client is None:
            import boto3

            client_kwargs = {}
            if self.endpoint_url:
                client_kwargs["endpoint_url"] = self.endpoint_url
            if self.region:
                client_kwargs["region_name"] = self.region
            if self.access_key_id and self.secret_access_key:
                client_kwargs["aws_access_key_id"] = self.access_key_id
                client_kwargs["aws_secret_access_key"] = self.secret_access_key

            self._s3_client = boto3.client("s3", **client_kwargs)
        return self._s3_client

    def _generate_filename(self):
        """Generate filename for the recording."""
        if self.filename:
            fname = self.filename
            if not fname.lower().endswith(".wav"):
                fname = fname + ".wav"
            return fname
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        return f"conversation_{timestamp}.wav"

    def open(self):
        # Create a temp directory for the local file
        self.temp_dir = tempfile.mkdtemp(prefix="s3_recording_")
        temp_file_path = os.path.join(self.temp_dir, self._generate_filename())

        # Use LocalStorage for writing
        self.local_storage = LocalStorage(
            file_path=temp_file_path,
            channels=self.channels,
            sample_rate=self.sample_rate,
            sample_width=self.sample_width,
        )
        self.local_storage.open()
        self.actual_file_path = self.local_storage.actual_file_path

    def write(self, data: bytes):
        if self.local_storage:
            self.local_storage.write(data)

    def flush(self):
        if self.local_storage:
            self.local_storage.flush()

    def close(self):
        if self.local_storage:
            local_path = self.local_storage.actual_file_path
            self.local_storage.close()

            # Upload to S3
            if local_path and os.path.exists(local_path):
                try:
                    s3_client = self._get_s3_client()
                    key = self.upload_prefix.rstrip("/")
                    if key:
                        key = f"{key}/{os.path.basename(local_path)}"
                    else:
                        key = os.path.basename(local_path)

                    s3_client.upload_file(local_path, self.bucket_name, key)

                    # Build the actual file path URL
                    if self.endpoint_url:
                        self.actual_file_path = (
                            f"{self.endpoint_url}/{self.bucket_name}/{key}"
                        )
                    else:
                        self.actual_file_path = f"s3://{self.bucket_name}/{key}"
                finally:
                    # Cleanup temp file
                    try:
                        os.remove(local_path)
                        if self.temp_dir:
                            os.rmdir(self.temp_dir)
                    except OSError:
                        pass

            self.local_storage = None
