# syntax=docker/dockerfile:1

# Build stage
FROM golang:1.22-alpine AS builder

# Install build dependencies
RUN apk add --no-cache git ca-certificates tzdata

# Set working directory
WORKDIR /app

# Copy go mod and sum files
COPY tenapp/go.mod tenapp/go.sum ./

# Download dependencies
RUN go mod download && go mod verify

# Copy source code
COPY tenapp/ ./tenapp/

# Build the application
RUN cd tenapp && CGO_ENABLED=0 GOOS=linux go build -a -installsuffix cgo -o main .

# Runtime stage
FROM python:3.11-slim AS runtime

# Install required packages
RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# Set working directory
WORKDIR /app

# Copy Python dependencies
COPY server/requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt

# Copy built Go application
COPY --from=builder /app/tenapp/main /app/tenapp/main
COPY --from=builder /app/tenapp/ten_packages ./tenapp/ten_packages
COPY --from=builder /app/tenapp/manifest.json ./tenapp/manifest.json
COPY --from=builder /app/tenapp/property.json ./tenapp/property.json

# Create scripts directory and copy scripts
COPY --from=builder /app/tenapp/scripts ./tenapp/scripts
RUN chmod +x ./tenapp/scripts/*.sh

# Copy server files
COPY server/ ./server/

# Copy frontend
COPY --from=builder /app/tenapp/frontend ./frontend

# Set environment variables
ENV PYTHONUNBUFFERED=1 \
    PYTHONDONTWRITEBYTECODE=1 \
    TELNYX_SERVER_PORT=9000

# Expose ports
EXPOSE 3000 9000 49483

# Health check
HEALTHCHECK --interval=30s --timeout=10s --start-period=5s --retries=3 \
    CMD python -c "import urllib.request; urllib.request.urlopen('http://localhost:9000/health')" || exit 1

# Set default command
CMD ["/app/tenapp/scripts/start.sh"]