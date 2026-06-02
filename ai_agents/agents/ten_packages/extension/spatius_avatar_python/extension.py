#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
from dataclasses import dataclass
from datetime import datetime, timedelta, timezone
from time import time

from agora_token_builder import RtcTokenBuilder
from ten_runtime import AsyncTenEnv
from ten_ai_base.config import BaseConfig
from spatius import new_avatar_session, AgoraEgressConfig

from .avatar_base import AsyncAvatarBaseExtension


@dataclass
class SpatiusConfig(BaseConfig):
    """Configuration for Spatius Avatar Extension."""

    spatius_api_key: str = ""
    spatius_app_id: str = ""
    spatius_avatar_id: str = ""
    region: str = ""
    agora_avatar_uid: str = ""
    agora_appid: str = ""
    agora_appcert: str = ""
    channel: str = ""
    sample_rate: int = 24000
    session_expire_minutes: int = 30
    dump: bool = False
    dump_path: str = ""


class SpatiusAvatarExtension(AsyncAvatarBaseExtension):
    """
    Spatius Avatar Extension.

    Implements 7 required methods from AsyncAvatarBaseExtension.
    All lifecycle management is handled by the base class.
    Uses spatius SDK for communication with Spatius avatar service.
    """

    def __init__(self, name: str):
        super().__init__(name)
        self.config: SpatiusConfig | None = None
        self.session = None
        self.ten_env: AsyncTenEnv | None = None

    def _on_frame_received(self, frame_data: bytes, is_last: bool) -> None:
        """Handle animation frames received from avatar service."""
        if self.ten_env:
            self.ten_env.log_debug(
                f"[Spatius] Frame received: {len(frame_data)} bytes, is_last={is_last}"
            )

    def _on_error(self, error: Exception) -> None:
        """Handle errors from avatar service."""
        if self.ten_env:
            self.ten_env.log_error(f"[Spatius] Session error: {error}")

    def _on_close(self) -> None:
        """Handle session close from avatar service."""
        if self.ten_env:
            self.ten_env.log_info("[Spatius] Session closed by server")

    # ========================================================================
    # REQUIRED METHODS - 7 methods to implement
    # ========================================================================

    async def validate_config(self, ten_env: AsyncTenEnv) -> bool:
        """Validate Spatius configuration."""
        try:
            self.config = await SpatiusConfig.create_async(ten_env)
            self.ten_env = ten_env

            # Validate required fields
            required_fields = {
                "spatius_api_key": self.config.spatius_api_key,
                "spatius_app_id": self.config.spatius_app_id,
                "spatius_avatar_id": self.config.spatius_avatar_id,
                "agora_avatar_uid": self.config.agora_avatar_uid,
                "agora_appid": self.config.agora_appid,
                "channel": self.config.channel,
            }

            missing_fields = [k for k, v in required_fields.items() if not v]
            if missing_fields:
                ten_env.log_error(
                    f"[Spatius] Missing required fields: {', '.join(missing_fields)}"
                )
                return False

            if self.config.sample_rate <= 0:
                ten_env.log_error(
                    f"[Spatius] Invalid sample_rate: {self.config.sample_rate}"
                )
                return False

            ten_env.log_info(
                f"[Spatius] Config loaded: "
                "api_key="
                f"{self._masked_api_key()}, "
                f"app_id={self.config.spatius_app_id}, "
                f"avatar_id={self.config.spatius_avatar_id}, "
                f"region={self._region() or '(sdk default)'}, "
                f"agora_avatar_uid={self.config.agora_avatar_uid}, "
                f"agora_appid={self.config.agora_appid}, "
                f"channel={self.config.channel}, "
                f"sample_rate={self.config.sample_rate}, "
                f"session_expire_minutes={self.config.session_expire_minutes}"
            )
            return True

        except Exception as e:
            ten_env.log_error(f"[Spatius] Config validation failed: {e}")
            return False

    def _masked_api_key(self) -> str:
        """Return a redacted Spatius API key for logs."""
        if len(self.config.spatius_api_key) <= 4:
            return "(short)"
        return f"***{self.config.spatius_api_key[-4:]}"

    def _region(self) -> str:
        """Return the configured Spatius region."""
        return (self.config.region or "").strip()

    def _generate_agora_token(self, uid: int) -> str:
        """Generate the Agora RTC token used by avatar egress."""
        if not self.config.agora_appcert:
            return self.config.agora_appid

        expire_time = 3600
        privilege_expired_ts = int(time()) + expire_time
        return RtcTokenBuilder.buildTokenWithUid(
            self.config.agora_appid,
            self.config.agora_appcert,
            self.config.channel,
            uid,
            1,
            privilege_expired_ts,
        )

    def get_target_sample_rate(self) -> list[int]:
        """Return the configured sample rate expected by spatius SDK."""
        return [self.config.sample_rate]

    async def connect_to_avatar(self, ten_env: AsyncTenEnv) -> None:
        """Connect to Spatius avatar service using spatius SDK."""
        ten_env.log_info(
            f"[Spatius] Connecting (avatar_id={self.config.spatius_avatar_id})"
        )

        # Create avatar session using spatius with Agora egress.
        avatar_uid = int(self.config.agora_avatar_uid)
        agora_egress = AgoraEgressConfig(
            channel_name=self.config.channel,
            token=self._generate_agora_token(avatar_uid),
            uid=avatar_uid,
            publisher_id=self.config.agora_avatar_uid,
        )

        session_kwargs = {
            "api_key": self.config.spatius_api_key,
            "app_id": self.config.spatius_app_id,
            "avatar_id": self.config.spatius_avatar_id,
            "expire_at": datetime.now(timezone.utc)
            + timedelta(minutes=self.config.session_expire_minutes),
            "sample_rate": self.config.sample_rate,
            "agora_egress": agora_egress,
            "transport_frames": self._on_frame_received,
            "on_error": self._on_error,
            "on_close": self._on_close,
        }
        region = self._region()
        if region:
            session_kwargs["region"] = region

        self.session = new_avatar_session(**session_kwargs)

        # Initialize session (obtains authentication token)
        await self.session.init()

        # Establish WebSocket connection
        connection_id = await self.session.start()
        ten_env.log_info(
            f"[Spatius] Connected successfully (connection_id={connection_id})"
        )

    async def disconnect_from_avatar(self, ten_env: AsyncTenEnv) -> None:
        """Disconnect from Spatius avatar service."""
        if self.session:
            try:
                await self.session.close()
            except Exception as e:
                ten_env.log_warn(f"[Spatius] Error during disconnect: {e}")
            finally:
                self.session = None

        ten_env.log_info("[Spatius] Disconnected")

    async def send_audio_to_avatar(self, audio_data: bytes) -> None:
        """Send audio to Spatius"""
        if self.session:
            if self.ten_env:
                self.ten_env.log_debug(
                    f"[Spatius] Sending audio: {len(audio_data)} bytes"
                )
            await self.session.send_audio(bytes(audio_data), end=False)

    async def send_eof_to_avatar(self) -> None:
        """Send EOF marker to Spatius avatar to signal end of audio stream."""
        if self.session:
            if self.ten_env:
                self.ten_env.log_info("[Spatius] Sending EOF")
            await self.session.send_audio(b"", end=True)

    async def interrupt_avatar(self) -> None:
        """Interrupt current Spatius avatar processing."""
        if self.session:
            if self.ten_env:
                self.ten_env.log_info("[Spatius] Interrupting avatar")
            try:
                await self.session.interrupt()
            except Exception as e:
                if self.ten_env:
                    self.ten_env.log_warn(f"[Spatius] Interrupt failed: {e}")

    # ========================================================================
    # OPTIONAL METHODS
    # ========================================================================

    def get_dump_config(self) -> tuple[bool, str]:
        """Return audio dump configuration from config."""
        if self.config:
            return (self.config.dump, self.config.dump_path)
        return (False, "")
