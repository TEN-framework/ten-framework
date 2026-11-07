#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
"""Property resolution and validation (design D3).

Pure: no TEN or SpatialReal imports, so the rules are unit-tested on any
laptop. The extension reads two top-level properties, ``channel`` (owned by
the TEN server, which injects the per-call channel into the graph node) and
the ``params`` object (everything the operator sets), and hands them here.

Every failure is a ``ValueError`` whose message names the offending key the
way it appears in the node property (``params.<key>``), so the ``-1012``
ModuleError the glue emits tells the operator what to fix.
"""

from dataclasses import dataclass
from typing import Any, Mapping

from .audio import ACCEPTED_RATES

DEFAULT_ENVIRONMENT = "us-west"
DEFAULT_SAMPLE_RATE = 24000
DEFAULT_EGRESS_IDLE_RECONNECT_MS = 1_080_000

CHANNEL_HINT = (
    'channel is empty: add "channel": "" to the avatar node\'s property so '
    "the TEN server can inject it, or set params.agora_channel"
)


@dataclass
class ResolvedConfig:
    """Validated settings; the glue maps them 1:1 onto ``SessionSettings``."""

    api_key: str
    avatar_id: str
    app_id: str
    environment: str
    console_endpoint_url: str
    ingress_endpoint_url: str
    sample_rate: int
    channel: str
    uid: int
    agora_appid: str
    agora_appcert: str
    agora_token: str
    egress_idle_reconnect_ms: int
    source_timeline_only: bool


def _string(params: Mapping[str, Any], key: str, default: str = "") -> str:
    """A string property; missing or null means ``default``."""
    value = params.get(key)
    if value is None:
        return default
    if not isinstance(value, str):
        raise ValueError(
            f"params.{key} must be a string, got {type(value).__name__}"
        )
    return value


def _required(params: Mapping[str, Any], key: str) -> str:
    value = _string(params, key).strip()
    if not value:
        raise ValueError(f"params.{key} is required")
    return value


def _integer(params: Mapping[str, Any], key: str, default: int) -> int:
    """An integer property given as an int or a decimal string."""
    value = params.get(key)
    if value is None or value == "":
        return default
    if isinstance(value, bool):
        raise ValueError(f"params.{key} must be an integer, got a boolean")
    if isinstance(value, int):
        return value
    if isinstance(value, str):
        try:
            return int(value.strip(), 10)
        except ValueError:
            pass
    raise ValueError(f"params.{key} must be an integer, got {value!r}")


def _boolean(params: Mapping[str, Any], key: str, default: bool) -> bool:
    value = params.get(key, default)
    if not isinstance(value, bool):
        raise ValueError(f"params.{key} must be a boolean")
    return value


def resolve_config(channel: str, params: Mapping[str, Any]) -> ResolvedConfig:
    """Validate the node properties and return the session settings.

    Raises ``ValueError`` naming the first invalid key. Order: the two
    SpatialReal keys, the channel, the uid, the sample rate, the Agora
    credentials, then the optional knobs.
    """
    api_key = _required(params, "spatialreal_api_key")
    avatar_id = _required(params, "spatialreal_avatar_id")

    explicit_channel = _string(params, "agora_channel").strip()
    channel = explicit_channel or (channel or "").strip()
    if not channel:
        raise ValueError(CHANNEL_HINT)

    uid_raw = params.get("agora_uid")
    try:
        uid = _integer(params, "agora_uid", 0)
    except ValueError:
        uid = 0
    if uid == 0:
        raise ValueError(
            f"params.agora_uid must be a non-zero integer, got {uid_raw!r}"
        )

    rate_raw = params.get("sample_rate")
    try:
        sample_rate = _integer(params, "sample_rate", DEFAULT_SAMPLE_RATE)
    except ValueError:
        sample_rate = 0
    if sample_rate not in ACCEPTED_RATES:
        accepted = ", ".join(str(r) for r in ACCEPTED_RATES)
        raise ValueError(
            f"params.sample_rate must be one of {accepted}, got {rate_raw!r}"
        )

    agora_token = _string(params, "agora_token").strip()
    agora_appid = _string(params, "agora_appid").strip()
    if not agora_appid and not agora_token:
        raise ValueError(
            "params.agora_appid is required unless params.agora_token is set"
        )

    idle_ms = _integer(
        params, "egress_idle_reconnect_ms", DEFAULT_EGRESS_IDLE_RECONNECT_MS
    )

    return ResolvedConfig(
        api_key=api_key,
        avatar_id=avatar_id,
        app_id=_string(params, "spatialreal_app_id").strip(),
        environment=_string(params, "environment", DEFAULT_ENVIRONMENT).strip()
        or DEFAULT_ENVIRONMENT,
        console_endpoint_url=_string(params, "console_endpoint_url").strip(),
        ingress_endpoint_url=_string(params, "ingress_endpoint_url").strip(),
        sample_rate=sample_rate,
        channel=channel,
        uid=uid,
        agora_appid=agora_appid,
        agora_appcert=_string(params, "agora_appcert").strip(),
        agora_token=agora_token,
        egress_idle_reconnect_ms=idle_ms,
        source_timeline_only=_boolean(params, "source_timeline_only", False),
    )
