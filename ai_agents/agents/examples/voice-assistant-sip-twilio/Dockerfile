FROM ghcr.io/ten-framework/ten_agent_build:0.7.9 AS builder

ARG USE_AGENT=agents/examples/voice-assistant-sip-twilio

WORKDIR /app

COPY .env.example .env
COPY agents/scripts/ agents/scripts/
COPY agents/ten_packages/ agents/ten_packages/

# Copy tenapp files explicitly to avoid symlink issues
COPY ${USE_AGENT}/tenapp/go.* ${USE_AGENT}/tenapp/
COPY ${USE_AGENT}/tenapp/main.go ${USE_AGENT}/tenapp/
COPY ${USE_AGENT}/tenapp/manifest* ${USE_AGENT}/tenapp/
COPY ${USE_AGENT}/tenapp/property.json ${USE_AGENT}/tenapp/
COPY ${USE_AGENT}/tenapp/scripts/ ${USE_AGENT}/tenapp/scripts/

# Copy extension directories that are actual directories (not symlinks)
COPY ${USE_AGENT}/tenapp/ten_packages/extension/main_python/ ${USE_AGENT}/tenapp/ten_packages/extension/main_python/

# Copy other example files
COPY ${USE_AGENT}/README.md ${USE_AGENT}/
COPY ${USE_AGENT}/Taskfile*.yml ${USE_AGENT}/
COPY ${USE_AGENT}/frontend/ ${USE_AGENT}/frontend/
COPY ${USE_AGENT}/server/ ${USE_AGENT}/server/
COPY ${USE_AGENT}/ngrok.yml ${USE_AGENT}/
COPY ${USE_AGENT}/start-with-ngrok.sh ${USE_AGENT}/

RUN cd /app/${USE_AGENT} && \
    task install && task release

# Build custom frontend
WORKDIR /app/${USE_AGENT}/frontend
RUN bun install && bun run build
WORKDIR /app

FROM ubuntu:22.04

ARG USE_AGENT=agents/examples/voice-assistant-sip-twilio

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
    unzip \
    && apt-get clean && rm -rf /var/lib/apt/lists/* && rm -rf /tmp/*

# Install Node.js 20
RUN curl -fsSL https://deb.nodesource.com/setup_20.x | bash - \
    && apt-get install -y nodejs

# Install Bun using the official install script
RUN curl -fsSL https://bun.com/install | bash
# Add Bun to the PATH (if not already added by the install script)
ENV PATH="/root/.bun/bin:$PATH"

# Install Task
RUN sh -c "$(curl --location https://taskfile.dev/install.sh)" -- -d -b /usr/local/bin

# Install ngrok for WebSocket tunneling
RUN curl -s https://ngrok-agent.s3.amazonaws.com/ngrok.asc | gpg --dearmor > /etc/apt/trusted.gpg.d/ngrok.gpg \
    && echo "deb https://ngrok-agent.s3.amazonaws.com buster main" > /etc/apt/sources.list.d/ngrok.list \
    && apt-get update && apt-get install -y ngrok

WORKDIR /app

COPY --from=builder /app/${USE_AGENT}/tenapp/.release/ /app/agents/
COPY --from=builder /usr/local/lib /usr/local/lib
COPY --from=builder /usr/lib/python3 /usr/lib/python3
COPY --from=builder /usr/local/bin/tman /usr/local/bin/tman

# Copy built frontend from builder stage
COPY --from=builder /app/${USE_AGENT}/frontend/ /app/frontend/

# Copy server files
COPY --from=builder /app/${USE_AGENT}/server/ /app/server/

# Copy additional files
COPY --from=builder /app/${USE_AGENT}/ngrok.yml /app/
COPY --from=builder /app/${USE_AGENT}/start-with-ngrok.sh /app/
RUN chmod +x /app/start-with-ngrok.sh

# Copy Docker Taskfile
COPY --from=builder /app/${USE_AGENT}/Taskfile.docker.yml /app/Taskfile.docker.yml

EXPOSE 8080 3000 9000

ENTRYPOINT ["task", "-t", "Taskfile.docker.yml", "run-prod"]
