#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
import asyncio
from typing import AsyncIterator
import numpy as np

from ten_runtime import AsyncTenEnv
from ten_ai_base.const import LOG_CATEGORY_VENDOR

from .config import Qwen3TTSConfig


class Qwen3TTSClient:
    """Client for Qwen3-TTS model inference"""

    def __init__(self, config: Qwen3TTSConfig, ten_env: AsyncTenEnv) -> None:
        self.config = config
        self.ten_env = ten_env
        self.model = None
        self.voice_clone_prompt = None
        self._is_initialized = False
        self._init_lock = asyncio.Lock()

    async def initialize(self) -> None:
        """Initialize the Qwen3 TTS model"""
        async with self._init_lock:
            if self._is_initialized:
                return

            self.ten_env.log_info(
                f"vendor_status: Initializing Qwen3 TTS model: {self.config.model}",
                category=LOG_CATEGORY_VENDOR,
            )

            try:
                # Run model loading in executor to avoid blocking
                loop = asyncio.get_event_loop()
                await loop.run_in_executor(None, self._load_model)
                self._is_initialized = True
                self.ten_env.log_info(
                    "vendor_status: Qwen3 TTS model initialized successfully",
                    category=LOG_CATEGORY_VENDOR,
                )
            except Exception as e:
                self.ten_env.log_error(
                    f"Failed to initialize Qwen3 TTS model: {e}"
                )
                raise

    def _load_model(self) -> None:
        """Load the Qwen3 TTS model (runs in executor)"""
        import torch  # pylint: disable=import-error
        from qwen_tts import Qwen3TTSModel  # pylint: disable=import-error

        # Determine dtype
        if self.config.dtype == "bfloat16":
            dtype = torch.bfloat16
        elif self.config.dtype == "float16":
            dtype = torch.float16
        else:
            dtype = torch.bfloat16

        # Load model with specified configuration
        model_kwargs = {
            "device_map": self.config.device,
            "dtype": dtype,
        }

        # Only add flash attention if available and requested
        if self.config.attn_implementation:
            model_kwargs["attn_implementation"] = (
                self.config.attn_implementation
            )

        self.model = Qwen3TTSModel.from_pretrained(
            self.config.model, **model_kwargs
        )

        # Pre-create voice clone prompt if using voice clone mode with reference audio
        if (
            self.config.mode == "voice_clone"
            and self.config.ref_audio_path
            and self.config.ref_text
        ):
            self.voice_clone_prompt = self.model.create_voice_clone_prompt(
                ref_audio=self.config.ref_audio_path,
                ref_text=self.config.ref_text,
                x_vector_only_mode=False,
            )

    async def synthesize(self, text: str) -> AsyncIterator[bytes]:
        """
        Synthesize speech from text.

        Args:
            text: The text to synthesize

        Yields:
            Audio data chunks as bytes (PCM 16-bit)
        """
        if not self._is_initialized:
            await self.initialize()

        self.ten_env.log_debug(
            f"vendor_status: Synthesizing text: {text[:50]}...",
            category=LOG_CATEGORY_VENDOR,
        )

        loop = asyncio.get_event_loop()

        # Run synthesis in executor
        wavs, sr = await loop.run_in_executor(None, self._generate_audio, text)

        # Convert numpy array to PCM bytes and yield in chunks
        if wavs is not None and len(wavs) > 0:
            # Handle both single and batch outputs
            audio_data = wavs[0] if isinstance(wavs, list) else wavs

            # Convert to numpy if tensor
            if hasattr(audio_data, "cpu"):
                audio_data = audio_data.cpu().numpy()

            # Ensure it's a 1D array
            if audio_data.ndim > 1:
                audio_data = audio_data.squeeze()

            # Resample if needed
            if sr != self.config.sample_rate:
                audio_data = self._resample(
                    audio_data, sr, self.config.sample_rate
                )

            # Convert to 16-bit PCM
            audio_data = np.clip(audio_data, -1.0, 1.0)
            pcm_data = (audio_data * 32767).astype(np.int16)
            audio_bytes = pcm_data.tobytes()

            # Yield in chunks (3200 bytes = 100ms at 16kHz mono 16-bit)
            chunk_size = int(self.config.sample_rate * 2 * 0.1)  # 100ms chunks
            for i in range(0, len(audio_bytes), chunk_size):
                yield audio_bytes[i : i + chunk_size]

    def _generate_audio(self, text: str):
        """Generate audio from text (runs in executor)"""
        mode = self.config.mode

        if mode == "custom_voice":
            # Use custom voice with preset speakers
            wavs, sr = self.model.generate_custom_voice(
                text=text,
                language=self.config.language,
                speaker=self.config.speaker,
                instruct=self.config.instruct if self.config.instruct else None,
            )
        elif mode == "voice_clone":
            # Use voice cloning
            if self.voice_clone_prompt is not None:
                # Use pre-created prompt for efficiency
                wavs, sr = self.model.generate_voice_clone(
                    text=text,
                    language=self.config.language,
                    voice_clone_prompt=self.voice_clone_prompt,
                )
            elif self.config.ref_audio_path and self.config.ref_text:
                # Generate with reference audio directly
                wavs, sr = self.model.generate_voice_clone(
                    text=text,
                    language=self.config.language,
                    ref_audio=self.config.ref_audio_path,
                    ref_text=self.config.ref_text,
                )
            else:
                raise ValueError(
                    "Voice clone mode requires ref_audio_path and ref_text"
                )
        elif mode == "voice_design":
            # Use voice design with natural language description
            if not self.config.voice_description:
                raise ValueError("Voice design mode requires voice_description")
            wavs, sr = self.model.generate_voice_design(
                text=text,
                language=self.config.language,
                instruct=self.config.voice_description,
            )
        else:
            raise ValueError(f"Unknown mode: {mode}")

        return wavs, sr

    def _resample(
        self, audio: np.ndarray, orig_sr: int, target_sr: int
    ) -> np.ndarray:
        """Resample audio to target sample rate"""
        if orig_sr == target_sr:
            return audio

        # Simple linear interpolation resampling
        duration = len(audio) / orig_sr
        target_length = int(duration * target_sr)
        indices = np.linspace(0, len(audio) - 1, target_length)
        return np.interp(indices, np.arange(len(audio)), audio)

    async def close(self) -> None:
        """Clean up resources"""
        self.ten_env.log_info("Closing Qwen3 TTS client")

        # Clear model from memory
        if self.model is not None:
            del self.model
            self.model = None

        self.voice_clone_prompt = None
        self._is_initialized = False

        # Try to free GPU memory
        try:
            import torch  # pylint: disable=import-error

            if torch.cuda.is_available():
                torch.cuda.empty_cache()
        except Exception:
            pass
