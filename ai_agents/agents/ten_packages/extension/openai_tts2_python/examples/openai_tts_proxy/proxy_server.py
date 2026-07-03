"""
OpenAI TTS Proxy Server

A simple HTTP proxy server that forwards requests from openai_tts to OpenAI API
and streams responses back. Designed to run independently in the same container.
"""

import os
import logging
import json
from contextlib import asynccontextmanager
from fastapi import FastAPI, Request, Response
from fastapi.responses import StreamingResponse
import httpx

# Configure logging
logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s - %(name)s - %(levelname)s - %(message)s",
)
logger = logging.getLogger(__name__)

# Create a single httpx client for connection pooling
http_client = None


@asynccontextmanager
async def lifespan(app: FastAPI):
    """Lifespan context manager for FastAPI app."""
    # Startup
    global http_client
    http_client = httpx.AsyncClient(
        timeout=httpx.Timeout(timeout=60.0),
        limits=httpx.Limits(
            max_connections=100,
            max_keepalive_connections=20,
            keepalive_expiry=600.0,
        ),
        http2=True,
    )
    logger.info("HTTP client initialized")
    yield
    # Shutdown
    if http_client:
        await http_client.aclose()
        logger.info("HTTP client closed")
    logger.info("Proxy server shutdown complete")


app = FastAPI(title="OpenAI TTS Proxy", lifespan=lifespan)

# Configuration
PROXY_HOST = os.getenv("PROXY_HOST", "0.0.0.0")
PROXY_PORT = int(os.getenv("PROXY_PORT", "8081"))
OPENAI_BASE_URL = os.getenv("OPENAI_BASE_URL", "https://api.openai.com/v1")
OPENAI_API_KEY = os.getenv("OPENAI_API_KEY", "")


async def _proxy_to_openai(request: Request):
    """
    Internal function to proxy requests to OpenAI /audio/speech endpoint.

    Forwards the request body and headers (especially Authorization) to OpenAI,
    then streams the response back to the client.
    """
    try:
        # Read request body
        body = await request.body()

        # Get request headers
        headers = dict(request.headers)

        # Log incoming request details
        logger.info("=" * 80)
        logger.info("INCOMING REQUEST:")
        logger.info(f"  Method: {request.method}")
        logger.info(f"  URL: {request.url}")
        logger.info(f"  Headers: {dict(headers)}")

        # Try to parse and log request body as JSON
        try:
            body_json = json.loads(body.decode("utf-8"))
            # Mask sensitive fields
            body_json_log = body_json.copy()
            if "api_key" in body_json_log:
                body_json_log["api_key"] = "***MASKED***"
            logger.info(f"  Body (JSON): {json.dumps(body_json_log, indent=2)}")
        except Exception:
            logger.info(f"  Body (raw): {body[:200]}... (first 200 bytes)")

        # Remove headers that shouldn't be forwarded
        headers.pop("host", None)
        headers.pop("content-length", None)  # Let httpx calculate it
        headers.pop("connection", None)
        headers.pop("upgrade", None)

        # Use OpenAI API key from environment if Authorization header is not present
        if "authorization" not in headers and OPENAI_API_KEY:
            headers["authorization"] = f"Bearer {OPENAI_API_KEY}"

        # Log Authorization header (masked)
        if "authorization" in headers:
            auth_header = headers["authorization"]
            if len(auth_header) > 20:
                masked_auth = (
                    auth_header[:10] + "***MASKED***" + auth_header[-10:]
                )
            else:
                masked_auth = "***MASKED***"
            logger.info(f"  Authorization: {masked_auth}")

        # Build target URL
        target_url = f"{OPENAI_BASE_URL}/audio/speech"

        logger.info(f"FORWARDING TO: {target_url}")
        logger.info(f"  Forwarding headers: {dict(headers)}")
        logger.info("=" * 80)

        # Forward request to OpenAI with streaming
        # Note: We need to keep the stream context alive during the entire streaming process
        stream_context = http_client.stream(
            "POST",
            target_url,
            headers=headers,
            content=body,
        )
        response = await stream_context.__aenter__()

        try:
            # Log response details
            logger.info("=" * 80)
            logger.info("OPENAI RESPONSE:")
            logger.info(f"  Status Code: {response.status_code}")
            logger.info(f"  Response Headers: {dict(response.headers)}")

            # Check for errors
            if response.status_code != 200:
                error_body = await response.aread()
                await stream_context.__aexit__(None, None, None)
                logger.error(
                    f"OpenAI API error: {response.status_code}, body: {error_body.decode('utf-8', errors='replace')[:200]}"
                )
                return Response(
                    content=error_body,
                    status_code=response.status_code,
                    headers={
                        key: value
                        for key, value in response.headers.items()
                        if key.lower() in ["content-type"]
                    },
                )

            # Prepare essential headers
            headers_to_exclude = [
                "content-encoding",
                "transfer-encoding",
                "connection",
                "date",
                "set-cookie",
                "server",
                "cf-ray",
                "cf-cache-status",
                "alt-svc",
                "strict-transport-security",
                "x-content-type-options",
                "access-control-expose-headers",
            ]

            essential_headers = {}
            for key, value in response.headers.items():
                key_lower = key.lower()
                if key_lower not in headers_to_exclude:
                    if (
                        key_lower in ["content-type"]
                        or key_lower.startswith("openai-")
                        or key_lower.startswith("x-")
                    ):
                        essential_headers[key] = value

            if "content-type" not in essential_headers:
                essential_headers["content-type"] = response.headers.get(
                    "content-type", "audio/pcm"
                )

            # Stream the response back to client
            total_bytes = 0
            chunk_count = 0

            async def generate():
                nonlocal total_bytes, chunk_count
                try:
                    logger.info("Starting to stream response chunks...")
                    async for raw_chunk in response.aiter_raw():
                        if raw_chunk:
                            chunk_count += 1
                            total_bytes += len(raw_chunk)
                            if chunk_count <= 5 or chunk_count % 10 == 0:
                                logger.info(
                                    f"  Chunk #{chunk_count}: {len(raw_chunk)} bytes (total: {total_bytes} bytes)"
                                )
                            yield raw_chunk
                    logger.info(
                        f"Streaming complete: {chunk_count} chunks, {total_bytes} total bytes"
                    )
                except Exception as e:
                    error_msg = str(e)
                    error_type = type(e).__name__
                    logger.warning(
                        f"Streaming interrupted: {error_type}: {error_msg}"
                    )
                    logger.warning(
                        f"  Streamed {chunk_count} chunks, {total_bytes} bytes before interruption"
                    )

                    if (
                        "StreamClosed" in error_type
                        or "stream has been closed" in error_msg.lower()
                        or "streamclosed" in error_msg.lower()
                    ):
                        logger.debug(
                            "Client disconnected early, stopping stream gracefully"
                        )
                    return
                finally:
                    # Clean up the stream context when done
                    try:
                        await stream_context.__aexit__(None, None, None)
                    except Exception:
                        pass

            logger.info(
                f"Returning StreamingResponse with headers: {essential_headers}"
            )
            logger.info("=" * 80)

            return StreamingResponse(
                generate(),
                status_code=response.status_code,
                headers=essential_headers,
            )
        except Exception as e:
            # Clean up on error
            try:
                await stream_context.__aexit__(None, None, None)
            except Exception:
                pass
            raise

    except httpx.HTTPStatusError as e:
        logger.error(
            f"HTTP status error while proxying: {e.response.status_code} - {e}"
        )
        return Response(
            content=f"Proxy error: {str(e)}",
            status_code=(
                e.response.status_code if hasattr(e, "response") else 500
            ),
        )
    except httpx.HTTPError as e:
        logger.error(f"HTTP error while proxying: {e}")
        return Response(
            content=f"Proxy error: {str(e)}",
            status_code=500,
        )
    except Exception as e:
        logger.error(f"Unexpected error while proxying: {e}", exc_info=True)
        return Response(
            content=f"Proxy error: {str(e)}",
            status_code=500,
        )


@app.post("/audio/speech")
async def proxy_audio_speech(request: Request):
    """
    Proxy POST requests to OpenAI /audio/speech endpoint.
    """
    return await _proxy_to_openai(request)


@app.post("/")
async def proxy_root(request: Request):
    """
    Proxy POST requests to root path, forwarding to OpenAI /audio/speech endpoint.

    This allows clients to use http://localhost:8081 as the base URL
    without needing to specify /audio/speech in the path.
    """
    return await _proxy_to_openai(request)


@app.get("/health")
async def health_check():
    """Health check endpoint."""
    return {"status": "ok", "service": "openai-tts-proxy"}


if __name__ == "__main__":
    import uvicorn

    logger.info(f"Starting OpenAI TTS Proxy on {PROXY_HOST}:{PROXY_PORT}")
    logger.info(f"OpenAI Base URL: {OPENAI_BASE_URL}")

    uvicorn.run(
        app,
        host=PROXY_HOST,
        port=PROXY_PORT,
        log_level="info",
    )
