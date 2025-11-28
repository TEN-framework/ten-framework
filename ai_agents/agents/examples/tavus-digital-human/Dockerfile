FROM ghcr.io/ten-framework/ten_agent_build:0.7.9 AS builder

ARG USE_AGENT=agents/examples/tavus-digital-human

WORKDIR /app

COPY .env.example .env
COPY server/ server/
COPY agents/scripts/ agents/scripts/
COPY agents/ten_packages/ agents/ten_packages/
COPY playground/ playground/

# Copy tenapp files explicitly to avoid symlink issues
COPY ${USE_AGENT}/tenapp/manifest* ${USE_AGENT}/tenapp/
COPY ${USE_AGENT}/tenapp/property.json ${USE_AGENT}/tenapp/

# Copy example files
COPY ${USE_AGENT}/README.md ${USE_AGENT}/
COPY ${USE_AGENT}/Taskfile*.yml ${USE_AGENT}/

RUN cd /app/${USE_AGENT} && \
  task install && task release

# Build frontend in builder stage
WORKDIR /app/playground
RUN NEXT_PUBLIC_EDIT_GRAPH_MODE=false bun run build
WORKDIR /app

FROM ubuntu:22.04

ARG USE_AGENT=agents/examples/tavus-digital-human

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
ENV PATH="/root/.bun/bin:$PATH"

# Install Task
RUN sh -c "$(curl --location https://taskfile.dev/install.sh)" -- -d -b /usr/local/bin

WORKDIR /app

COPY --from=builder /app/${USE_AGENT}/tenapp/.release/ /app/agents/
COPY --from=builder /app/server/bin/api /app/server/bin/api
COPY --from=builder /usr/local/lib /usr/local/lib
COPY --from=builder /usr/lib/python3 /usr/lib/python3
COPY --from=builder /usr/local/bin/tman /usr/local/bin/tman

# Copy built frontend from builder stage
COPY --from=builder /app/playground/ /app/playground/

# Copy Docker Taskfile
COPY --from=builder /app/${USE_AGENT}/Taskfile.docker.yml /app/Taskfile.docker.yml

EXPOSE 8080 3000

ENTRYPOINT ["task", "-t", "Taskfile.docker.yml", "run-prod"]
