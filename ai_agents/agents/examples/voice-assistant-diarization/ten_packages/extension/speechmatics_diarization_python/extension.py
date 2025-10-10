#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#

from typing import Any

from typing_extensions import override

from ten_ai_base.const import LOG_CATEGORY_KEY_POINT
from ten_runtime import AsyncTenEnv

from ..speechmatics_asr_python.extension import SpeechmaticsASRExtension
from .config import SpeechmaticsDiarizationConfig


class SpeechmaticsDiarizationExtension(SpeechmaticsASRExtension):
    """Speechmatics extension pre-configured for speaker diarization."""

    @override
    async def on_init(self, ten_env: AsyncTenEnv) -> None:
        await super().on_init(ten_env)

        if self.config is None:
            return

        config_dict: dict[str, Any] = self.config.model_dump()
        diarization_config = SpeechmaticsDiarizationConfig(**config_dict)

        # Allow flattened diarization knobs inside params for convenience.
        for key in ("speaker_sensitivity", "prefer_current_speaker", "max_speakers"):
            if (
                diarization_config.params
                and key in diarization_config.params
                and diarization_config.params[key] is not None
            ):
                diarization_config.speaker_diarization_config[key] = (
                    diarization_config.params[key]
                )

        if not diarization_config.diarization:
            diarization_config.diarization = "speaker"

        diarization_config.apply_defaults()

        # Re-apply params mapping in case defaults added new values.
        diarization_config.update(diarization_config.params)
        self.config = diarization_config

        ten_env.log_info(
            "Speechmatics diarization config prepared",
            category=LOG_CATEGORY_KEY_POINT,
        )

    @override
    def vendor(self) -> str:
        return "speechmatics"
