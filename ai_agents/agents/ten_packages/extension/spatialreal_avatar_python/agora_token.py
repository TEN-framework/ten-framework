#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
"""The Agora token rule for the avatar's egress identity.

Pure: no TEN or SpatialReal imports. The session client calls this on every
connect so a reconnect always joins the channel with a fresh token.

Token rule:

1. an explicit ``agora_token`` is used verbatim;
2. otherwise, with an App Certificate, a publisher token is signed for the
   channel and uid, valid for ``ttl_s`` (24 h by default);
3. otherwise (project without certificate) the App ID itself is the token.

Nothing to return from is a configuration error, raised as ``ValueError``.
"""

import time

from agora_token_builder import RtcTokenBuilder

ROLE_PUBLISHER = 1
DEFAULT_TTL_S = 24 * 3600


def build_agora_token(
    app_id: str,
    app_cert: str,
    channel: str,
    uid: int,
    explicit_token: str = "",
    ttl_s: int = DEFAULT_TTL_S,
    now_s: int | None = None,
) -> str:
    """Return the token the avatar joins ``channel`` with as ``uid``."""
    if explicit_token:
        return explicit_token
    if not app_id:
        raise ValueError("agora_appid is required unless agora_token is set")
    if not app_cert:
        return app_id
    if now_s is None:
        now_s = int(time.time())
    return RtcTokenBuilder.buildTokenWithUid(
        app_id, app_cert, channel, uid, ROLE_PUBLISHER, now_s + ttl_s
    )
