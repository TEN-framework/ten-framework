#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
from typing import Optional
import json
from openai import AsyncOpenAI, AsyncAzureOpenAI
from ten_runtime import AsyncTenEnv
from ten_ai_base.const import LOG_CATEGORY_VENDOR
from .config import OpenAIGPTImageConfig


# Custom exceptions for better error handling
class ContentPolicyError(Exception):
    """Raised when content violates OpenAI's usage policies"""


class InvalidAPIKeyError(Exception):
    """Raised when API key is invalid or unauthorized"""


class ModelNotFoundError(Exception):
    """Raised when requested model is not available"""


class OpenAIImageClient:
    """
    Client for OpenAI Images API (GPT Image 1.5 / DALL-E)

    Handles image generation requests with proper error handling
    and fallback support.
    """

    def __init__(self, config: OpenAIGPTImageConfig, ten_env: AsyncTenEnv):
        self.config = config
        self.ten_env = ten_env
        self.client: AsyncOpenAI | AsyncAzureOpenAI = None
        self.current_model = config.params["model"]

        # Initialize appropriate client
        vendor = config.params.get("vendor", "openai")

        if vendor == "azure":
            # Azure OpenAI client
            azure_endpoint = config.params.get("azure_endpoint")
            azure_api_version = config.params.get("azure_api_version")

            if not azure_endpoint or not azure_api_version:
                raise ValueError(
                    "Azure vendor requires azure_endpoint and azure_api_version"
                )

            self.client = AsyncAzureOpenAI(
                api_key=config.params["api_key"],
                api_version=azure_api_version,
                azure_endpoint=azure_endpoint,
            )
            ten_env.log_info(
                f"Using Azure OpenAI: {azure_endpoint} (v{azure_api_version})"
            )
        else:
            # Standard OpenAI client
            client_kwargs = {"api_key": config.params["api_key"]}

            # Optional custom base_url
            base_url = config.params.get("base_url")
            if base_url:
                client_kwargs["base_url"] = base_url
                ten_env.log_info(f"Using custom base_url: {base_url}")

            self.client = AsyncOpenAI(**client_kwargs)
            ten_env.log_info("Using standard OpenAI API")

    async def generate_image(
        self,
        prompt: str,
        quality: Optional[str] = None,
        model_override: Optional[str] = None,
    ) -> str:
        """
        Generate image from text prompt

        Args:
            prompt: Text description of desired image
            quality: Optional quality override ('standard' or 'hd')
            model_override: Optional model override (for fallback)

        Returns:
            Image URL

        Raises:
            ContentPolicyError: Content violates policies
            InvalidAPIKeyError: API key is invalid
            ModelNotFoundError: Model not available
            Exception: Other API errors
        """
        # Build request parameters
        model = model_override or self.current_model
        is_gpt_image_model = model.startswith("gpt-image")

        # GPT Image models use different quality values than DALL-E
        # DALL-E: 'standard', 'hd'
        # GPT Image: 'low', 'medium', 'high', 'auto'
        requested_quality = quality or self.config.params.get(
            "quality", "standard"
        )
        if is_gpt_image_model:
            # Map DALL-E quality values to GPT Image values
            quality_map = {
                "standard": "auto",
                "hd": "high",
            }
            requested_quality = quality_map.get(
                requested_quality, requested_quality
            )

        request_params = {
            "model": model,
            "prompt": prompt,
            "size": self.config.params.get("size", "1024x1024"),
            "quality": requested_quality,
            "n": 1,  # Always generate 1 image
        }

        # GPT Image models (gpt-image-1, gpt-image-1.5) don't support response_format
        # Only add it for DALL-E models
        response_format = self.config.params.get("response_format", "url")
        if response_format and not is_gpt_image_model:
            request_params["response_format"] = response_format

        self.ten_env.log_info(
            f"Requesting image generation: model={model}, "
            f"size={request_params['size']}, quality={request_params['quality']}",
            category=LOG_CATEGORY_VENDOR,
        )

        try:
            # Call OpenAI Images API
            response = await self.client.images.generate(**request_params)

            # Extract image URL or base64 data
            # GPT Image models ONLY return base64 data, not URLs
            if is_gpt_image_model or response_format == "b64_json":
                # GPT Image models always return base64
                image_data = response.data[0].b64_json
                # Convert to data URL
                image_url = f"data:image/png;base64,{image_data}"
            else:
                # DALL-E models support URL response
                image_url = response.data[0].url

            # Optional: Save response for debugging
            if self.config.dump:
                self._dump_response(prompt, image_url)

            self.ten_env.log_info(
                f"Image generated successfully: {image_url[:100]}...",
                category=LOG_CATEGORY_VENDOR,
            )

            return image_url

        except Exception as e:
            error_message = str(e)
            self.ten_env.log_error(
                f"Image generation error: {error_message}",
                category=LOG_CATEGORY_VENDOR,
            )

            # Classify error for appropriate handling
            if "content_policy_violation" in error_message.lower():
                raise ContentPolicyError(error_message) from e

            elif "401" in error_message or "invalid_api_key" in error_message:
                raise InvalidAPIKeyError(error_message) from e

            elif "404" in error_message or "model_not_found" in error_message:
                raise ModelNotFoundError(error_message) from e

            elif "429" in error_message:
                # Rate limit - re-raise as generic exception
                raise RuntimeError(
                    "Rate limit exceeded. Please try again later."
                ) from e

            else:
                # Generic error
                raise

    def _dump_response(self, prompt: str, image_url: str) -> None:
        """Dump response to file for debugging"""
        try:
            with open(self.config.dump_path, "a", encoding="utf-8") as f:
                json.dump(
                    {
                        "prompt": prompt,
                        "image_url": image_url,
                        "model": self.current_model,
                    },
                    f,
                )
                f.write("\n")
        except Exception as e:
            self.ten_env.log_warn(f"Failed to dump response: {e}")

    async def cleanup(self) -> None:
        """Cleanup resources"""
        if self.client:
            await self.client.close()
            self.ten_env.log_info("OpenAI client closed")
