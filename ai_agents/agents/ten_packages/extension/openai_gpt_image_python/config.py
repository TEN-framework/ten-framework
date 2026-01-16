#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
from typing import Any
import copy
from pydantic import BaseModel, Field
from ten_ai_base import utils


class OpenAIGPTImageConfig(BaseModel):
    """OpenAI GPT Image 1.5 Configuration"""

    params: dict[str, Any] = Field(
        default_factory=dict, description="OpenAI Images API parameters"
    )

    dump: bool = Field(
        default=False, description="Enable response dumping for debugging"
    )
    dump_path: str = Field(
        default="./openai_image_responses.json",
        description="Path to dump responses",
    )

    def validate(self) -> None:
        """Validate required configuration"""
        if "api_key" not in self.params or not self.params["api_key"]:
            raise ValueError("API key is required (params.api_key)")

        if "model" not in self.params:
            raise ValueError("Model is required (params.model)")

        # Validate model
        valid_models = ["gpt-image-1", "gpt-image-1.5", "dall-e-3", "dall-e-2"]
        if self.params["model"] not in valid_models:
            raise ValueError(f"Invalid model. Must be one of: {valid_models}")

        # Validate size if present
        if "size" in self.params:
            valid_sizes = ["1024x1024", "1792x1024", "1024x1792"]
            if self.params["size"] not in valid_sizes:
                raise ValueError(f"Invalid size. Must be one of: {valid_sizes}")

        # Validate quality if present
        if "quality" in self.params:
            valid_quality = ["standard", "hd"]
            if self.params["quality"] not in valid_quality:
                raise ValueError(
                    f"Invalid quality. Must be one of: {valid_quality}"
                )

    def update_params(self) -> None:
        """Update/normalize parameters"""
        params = dict(self.params)

        # Set defaults for optional params
        params.setdefault("size", "1024x1024")
        params.setdefault("quality", "standard")
        params.setdefault("n", 1)
        params.setdefault("response_format", "url")

        # Remove vendor param if exists (internal only)
        params.pop("vendor", None)

        # Ensure n=1 for simplicity
        if params["n"] != 1:
            params["n"] = 1

        self.params = params

    def to_str(self, sensitive_handling: bool = True) -> str:
        """Convert config to string with optional sensitive data handling"""
        if not sensitive_handling:
            return f"{self}"

        config = copy.deepcopy(self)

        # Encrypt sensitive fields
        if config.params and "api_key" in config.params:
            config.params["api_key"] = utils.encrypt(config.params["api_key"])

        return f"{config}"
