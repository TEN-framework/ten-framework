FROM ghcr.io/ten-framework/ten_agent_build:0.7.3 AS builder

ARG USE_AGENT=agents/examples/voice-assistant

WORKDIR /app

COPY .env.example .env
COPY agents/examples/voice-assistant agents/examples/voice-assistant
COPY server/ server/
COPY agents/scripts/ agents/scripts/
COPY agents/ten_packages/ agents/ten_packages/
COPY playground/ playground/

RUN cd /app/${USE_AGENT} && \
  task install && task release

RUN mv /app/${USE_AGENT}/tenapp/.release/ agents/.release/

# Build frontend in builder stage (npm install already done by task install)
WORKDIR /app/playground
RUN NEXT_PUBLIC_EDIT_GRAPH_MODE=false npm run build
WORKDIR /app

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
  curl \
  && apt-get clean && rm -rf /var/lib/apt/lists/* && rm -rf /tmp/*

# Install Node.js 20
RUN curl -fsSL https://deb.nodesource.com/setup_20.x | bash - \
  && apt-get install -y nodejs

# Install Task
RUN sh -c "$(curl --location https://taskfile.dev/install.sh)" -- -d -b /usr/local/bin

WORKDIR /app

COPY --from=builder /app/agents/.release/ /app/agents/
COPY --from=builder /app/server/bin/api /app/server/bin/api
COPY --from=builder /usr/local/lib /usr/local/lib
COPY --from=builder /usr/lib/python3 /usr/lib/python3
COPY --from=builder /usr/local/bin/tman /usr/local/bin/tman

# Copy built frontend from builder stage
COPY --from=builder /app/playground/ /app/playground/

# Copy Docker Taskfile
COPY agents/examples/voice-assistant/Taskfile.docker.yml /app/Taskfile.docker.yml

EXPOSE 8080 3000

ENTRYPOINT ["task", "-t", "Taskfile.docker.yml", "run-prod"]
