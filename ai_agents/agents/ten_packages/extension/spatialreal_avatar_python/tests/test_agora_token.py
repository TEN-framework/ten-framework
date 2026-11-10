#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
"""Unit tests for the Agora token rule.

The rule: an explicit token wins; otherwise a certificate signs a publisher
token for the channel and uid; otherwise the App ID itself is the token
(project without certificate). Nothing at all is a configuration error.
"""

import pytest
from agora_token_builder.AccessToken import (
    AccessToken,
    kJoinChannel,
    kPublishAudioStream,
    kPublishDataStream,
    kPublishVideoStream,
)

from spatialreal_avatar_python.agora_token import build_agora_token

# AccessToken.fromString slices a fixed-width App ID, so the fake one has
# the real width. It is a test literal, not a credential.
APP_ID = "a" * 32
APP_CERT = "c"
CHANNEL = "room"
UID = 12345
NOW_S = 1_700_000_000


def test_explicit_token_wins():
    assert (
        build_agora_token(
            APP_ID, APP_CERT, CHANNEL, UID, explicit_token="tok", now_s=NOW_S
        )
        == "tok"
    )
    # Even with nothing else configured the explicit token is used verbatim.
    assert build_agora_token("", "", "", 0, explicit_token="tok") == "tok"


def test_cert_signs_publisher_token():
    token = build_agora_token(APP_ID, APP_CERT, CHANNEL, UID, now_s=NOW_S)

    assert token
    assert token != APP_ID
    assert token.startswith("006" + APP_ID)

    # The builder salts every token, so two mints differ even for a fixed
    # ``now_s``; what is deterministic is the content: publisher privileges
    # that all expire at ``now_s + ttl_s``.
    parsed = AccessToken()
    assert parsed.fromString(token)
    assert parsed.messages == {
        kJoinChannel: NOW_S + 86400,
        kPublishVideoStream: NOW_S + 86400,
        kPublishAudioStream: NOW_S + 86400,
        kPublishDataStream: NOW_S + 86400,
    }


def test_cert_token_honours_ttl():
    token = build_agora_token(
        APP_ID, APP_CERT, CHANNEL, UID, ttl_s=600, now_s=NOW_S
    )
    parsed = AccessToken()
    assert parsed.fromString(token)
    assert parsed.messages[kJoinChannel] == NOW_S + 600


def test_cert_token_without_now_uses_wall_clock():
    # ``now_s=None`` must not raise; the privilege expiry is in the future.
    token = build_agora_token(APP_ID, APP_CERT, CHANNEL, UID)
    parsed = AccessToken()
    assert parsed.fromString(token)
    assert parsed.messages[kJoinChannel] > NOW_S


def test_no_cert_returns_app_id():
    assert build_agora_token(APP_ID, "", CHANNEL, UID, now_s=NOW_S) == APP_ID


def test_nothing_raises():
    with pytest.raises(ValueError):
        build_agora_token("", "", CHANNEL, UID, now_s=NOW_S)
    with pytest.raises(ValueError):
        build_agora_token("", "c", CHANNEL, UID, now_s=NOW_S)
