# =============================================================================
# Stage 1: build-time image
#
# Uses the pre-built ten_agent_build image (which already contains task,
# rustup, go, uv, node, python build deps, etc.) to compile the agent.
# This image is heavy and should never be shipped to production; only its
# build output is copied into the runtime stage below.
#
# See issue #917 for the build-vs-runtime split rationale.
# =============================================================================
FROM ghcr.io/ten-framework/ten_agent_build:0.7.14 AS builder

ARG SESSION_CONTROL_CONF=session_control.conf
# Add a new argument for USE_AGENT (defaulting to 'agents/examples/default')
ARG USE_AGENT=agents/examples/voice-assistant

WORKDIR /app

COPY . .
# COPY agents/${SESSION_CONTROL_CONF} agents/session_control.conf

RUN cd ${USE_AGENT} && \
  task install && task release

RUN mv ${USE_AGENT}/tenapp/.release/ agents/

# =============================================================================
# Stage 2: runtime image
#
# Minimal ubuntu:22.04 base with only the shared libraries and Python
# runtime needed to execute the compiled `api` binary and any Python
# extensions. Build toolchains (task, rustup, go, cargo, gcc, cmake, node,
# npm, uv) are intentionally NOT installed here. See issue #917.
# =============================================================================
FROM ubuntu:22.04

RUN apt-get clean && apt-get update && apt-get install -y --no-install-recommends \
  libasound2 \
  libgstreamer1.0-dev \
  libunwind-dev \
  libc++1 \
  libssl-dev \
  python3 \
  python3-venv \
  python3-pip \
  python3-dev \
  jq vim \
  ca-certificates \
  && apt-get clean && rm -rf /var/lib/apt/lists/* && rm -rf /tmp/*

WORKDIR /app

COPY --from=builder /app/agents/.release/ agents/
COPY --from=builder /app/server/bin/api /app/server/bin/api
COPY --from=builder /usr/local/lib /usr/local/lib
COPY --from=builder /usr/lib/python3 /usr/lib/python3

EXPOSE 8080

ENTRYPOINT ["/app/server/bin/api"]
