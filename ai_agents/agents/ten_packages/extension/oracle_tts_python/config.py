#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
import json
from typing import Dict, Any
from pydantic import BaseModel, Field
from ten_ai_base.utils import encrypt


class OracleTTSConfig(BaseModel):
    """Oracle Cloud Infrastructure Speech TTS Configuration"""

    dump: bool = False
    dump_path: str = "/tmp"

    params: Dict[str, Any] = Field(default_factory=dict)

    def to_json(self, sensitive_handling: bool = False) -> str:
        config_dict = self.model_dump()
        if sensitive_handling and config_dict["params"]:
            sensitive_keys = ["fingerprint", "key_file", "tenancy", "user"]
            for key in sensitive_keys:
                if key in config_dict["params"] and config_dict["params"][key]:
                    config_dict["params"][key] = encrypt(
                        config_dict["params"][key]
                    )
        return json.dumps(config_dict)

    def validate_params(self) -> None:
        required_keys = [
            "tenancy",
            "user",
            "fingerprint",
            "key_file",
            "compartment_id",
        ]
        missing = [
            k
            for k in required_keys
            if not self.params.get(k)
        ]
        if missing:
            raise ValueError(
                f"Missing required OCI parameters: {', '.join(missing)}"
            )
