#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
"""
Apollo API Client for Thymia depression/anxiety detection.

API Flow:
1. Create model run → get upload URLs
2. Upload mood audio to presigned URL
3. Upload read aloud audio to presigned URL
4. Poll for results until complete
"""

import asyncio
import aiohttp
import io
import wave
from typing import Optional, Tuple
from dataclasses import dataclass


@dataclass
class ApolloResult:
    """Apollo API result with depression and anxiety metrics."""

    status: str  # COMPLETE_OK, COMPLETE_ERROR, FAILED, PROCESSING
    depression_probability: Optional[float] = None  # 0.0 - 1.0
    depression_severity: Optional[str] = None  # NONE, MILD, MODERATE, SEVERE
    anxiety_probability: Optional[float] = None  # 0.0 - 1.0
    anxiety_severity: Optional[str] = None  # NONE, MILD, MODERATE, SEVERE
    error_message: Optional[str] = None


class ApolloAPI:
    """Client for Thymia Apollo API (depression/anxiety detection)."""

    def __init__(self, api_key: str, base_url: str = "https://api.thymia.ai"):
        self.api_key = api_key
        self.base_url = base_url
        self.session: Optional[aiohttp.ClientSession] = None

    async def _ensure_session(self):
        """Ensure aiohttp session exists."""
        if self.session is None or self.session.closed:
            self.session = aiohttp.ClientSession()

    async def close(self):
        """Close the aiohttp session."""
        if self.session and not self.session.closed:
            await self.session.close()

    def _pcm_to_wav_bytes(
        self, pcm_data: bytes, sample_rate: int = 16000, channels: int = 1
    ) -> bytes:
        """Convert PCM audio data to WAV format bytes."""
        wav_buffer = io.BytesIO()
        # pylint: disable=no-member  # wave.open in 'wb' mode returns Wave_write, not Wave_read
        with wave.open(wav_buffer, "wb") as wav_file:
            wav_file.setnchannels(channels)
            wav_file.setsampwidth(2)  # 16-bit = 2 bytes
            wav_file.setframerate(sample_rate)
            wav_file.writeframes(pcm_data)
        return wav_buffer.getvalue()

    async def create_model_run(
        self,
        user_label: str,
        date_of_birth: str,  # YYYY-MM-DD
        birth_sex: str,  # MALE, FEMALE, OTHER
        language: str = "en-GB",
        delete_data: bool = True,
    ) -> Tuple[str, str, str]:
        """
        Create Apollo model run and get upload URLs.

        Args:
            user_label: User identifier
            date_of_birth: Date of birth in YYYY-MM-DD format
            birth_sex: MALE, FEMALE, or OTHER
            language: Language code (default: en-GB)
            delete_data: Whether to delete data after processing

        Returns:
            Tuple of (model_run_id, mood_upload_url, read_aloud_upload_url)

        Raises:
            Exception: If API call fails
        """
        await self._ensure_session()

        url = f"{self.base_url}/v1/models/apollo"
        headers = {
            "x-api-key": self.api_key,
            "Content-Type": "application/json",
        }
        payload = {
            "user": {
                "userLabel": user_label,
                "dateOfBirth": date_of_birth,
                "birthSex": birth_sex,
            },
            "language": language,
            "deleteData": delete_data,
        }

        async with self.session.post(
            url, json=payload, headers=headers
        ) as response:
            if response.status != 200:
                error_text = await response.text()
                raise RuntimeError(
                    f"Apollo create model run failed: {response.status} - {error_text}"
                )

            data = await response.json()
            model_run_id = data["id"]
            mood_url = data["uploadUrls"]["moodQuestionUploadUrl"]
            read_url = data["uploadUrls"]["readAloudUploadUrl"]

            return model_run_id, mood_url, read_url

    async def upload_audio(
        self, upload_url: str, pcm_data: bytes, sample_rate: int = 16000
    ):
        """
        Upload audio to presigned URL.

        Args:
            upload_url: Presigned S3 upload URL
            pcm_data: PCM audio data (16-bit, mono)
            sample_rate: Sample rate in Hz (default: 16000)

        Raises:
            Exception: If upload fails
        """
        await self._ensure_session()

        # Convert PCM to WAV format
        wav_data = self._pcm_to_wav_bytes(pcm_data, sample_rate)

        headers = {"Content-Type": "audio/wav"}

        async with self.session.put(
            upload_url, data=wav_data, headers=headers
        ) as response:
            if response.status not in (200, 201, 204):
                error_text = await response.text()
                raise RuntimeError(
                    f"Apollo audio upload failed: {response.status} - {error_text}"
                )

    async def poll_results(
        self,
        model_run_id: str,
        max_attempts: int = 30,
        poll_interval: float = 2.0,
    ) -> ApolloResult:
        """
        Poll for Apollo results.

        Args:
            model_run_id: Model run ID from create_model_run
            max_attempts: Maximum number of polling attempts (default: 60)
            poll_interval: Seconds between polls (default: 2.0)

        Returns:
            ApolloResult with depression and anxiety metrics

        Raises:
            Exception: If polling times out or fails
        """
        await self._ensure_session()

        url = f"{self.base_url}/v1/models/apollo/{model_run_id}"
        headers = {"x-api-key": self.api_key}

        for _attempt in range(max_attempts):
            async with self.session.get(url, headers=headers) as response:
                if response.status != 200:
                    error_text = await response.text()
                    return ApolloResult(
                        status="FAILED",
                        error_message=f"Poll failed: {response.status} - {error_text}",
                    )

                data = await response.json()
                status = data.get("status")

                if status == "COMPLETE_OK":
                    results = data.get("results", {}).get("disorders", {})
                    depression = results.get("depression", {})
                    anxiety = results.get("anxiety", {})

                    return ApolloResult(
                        status="COMPLETE_OK",
                        depression_probability=depression.get("probability"),
                        depression_severity=depression.get("severity"),
                        anxiety_probability=anxiety.get("probability"),
                        anxiety_severity=anxiety.get("severity"),
                    )

                elif status in ("COMPLETE_ERROR", "FAILED"):
                    return ApolloResult(
                        status=status,
                        error_message=data.get("error", "Unknown error"),
                    )

                # Still processing, continue polling
                await asyncio.sleep(poll_interval)

        # Timeout
        return ApolloResult(
            status="FAILED",
            error_message=f"Polling timeout after {max_attempts * poll_interval} seconds",
        )

    async def analyze(
        self,
        mood_audio_pcm: bytes,
        read_aloud_audio_pcm: bytes,
        user_label: str,
        date_of_birth: str,
        birth_sex: str,
        sample_rate: int = 16000,
        language: str = "en-GB",
    ) -> ApolloResult:
        """
        High-level method to run complete Apollo analysis.

        Args:
            mood_audio_pcm: PCM audio data for mood question response
            read_aloud_audio_pcm: PCM audio data for read aloud task
            user_label: User identifier
            date_of_birth: Date of birth (YYYY-MM-DD)
            birth_sex: MALE, FEMALE, or OTHER
            sample_rate: Sample rate in Hz (default: 16000)
            language: Language code (default: en-GB)

        Returns:
            ApolloResult with depression and anxiety metrics
        """
        try:
            # Step 1: Create model run
            model_run_id, mood_url, read_url = await self.create_model_run(
                user_label=user_label,
                date_of_birth=date_of_birth,
                birth_sex=birth_sex,
                language=language,
            )

            # Step 2: Upload both audio files
            await self.upload_audio(mood_url, mood_audio_pcm, sample_rate)
            await self.upload_audio(read_url, read_aloud_audio_pcm, sample_rate)

            # Step 3: Poll for results
            result = await self.poll_results(model_run_id)

            return result

        except Exception as e:
            return ApolloResult(status="FAILED", error_message=str(e))
