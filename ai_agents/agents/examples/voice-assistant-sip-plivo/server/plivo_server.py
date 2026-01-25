#!/usr/bin/env python3
"""
Plivo Server - Configuration and health check only
All call logic is handled by tenapp application
"""
import asyncio
import logging
import os
import sys
import subprocess
import signal
import threading
from datetime import datetime
from pathlib import Path

import uvicorn
from fastapi import FastAPI
from fastapi.middleware.cors import CORSMiddleware
from fastapi.responses import JSONResponse
from pydantic import BaseModel, Field


class PlivoServerConfig(BaseModel):
    """Configuration for Plivo server"""

    # Plivo configuration
    plivo_auth_id: str = Field(
        default="", description="Plivo Auth ID"
    )
    plivo_auth_token: str = Field(default="", description="Plivo Auth Token")
    plivo_from_number: str = Field(
        default="", description="Plivo phone number to call from"
    )

    # Server configuration
    plivo_server_port: int = Field(
        default=8080, description="Port for server (process management)"
    )

    # Tenapp configuration
    tenapp_dir: str = Field(default="", description="Path to tenapp directory")

    # Public server URL configuration
    plivo_public_server_url: str = Field(
        default="",
        description="Public server URL without protocol (e.g., 'your-domain.com:9000') - used for both media stream and webhooks",
    )

    # Protocol configuration
    plivo_use_https: bool = Field(
        default=True,
        description="Use HTTPS for webhooks (True) or HTTP (False)",
    )
    plivo_use_wss: bool = Field(
        default=True,
        description="Use WSS for media stream (True) or WS (False)",
    )


class PlivoServer:
    """FastAPI server for configuration and health check"""

    def __init__(self, config: PlivoServerConfig):
        self.config = config
        self.app = FastAPI(title="Plivo Configuration Server")
        self.logger = self._setup_logging()

        # Process management
        self.tenapp_process = None
        self.shutdown_event = threading.Event()

        # Add CORS middleware
        self.app.add_middleware(
            CORSMiddleware,
            allow_origins=["*"],
            allow_credentials=True,
            allow_methods=["*"],
            allow_headers=["*"],
        )

        # Setup routes
        self._setup_routes()

    def _setup_logging(self) -> logging.Logger:
        """Setup logging configuration"""
        logger = logging.getLogger("plivo_process_manager")
        logger.setLevel(logging.INFO)

        if not logger.handlers:
            handler = logging.StreamHandler()
            formatter = logging.Formatter(
                "%(asctime)s - %(name)s - %(levelname)s - %(message)s"
            )
            handler.setFormatter(formatter)
            logger.addHandler(handler)

        return logger

    def _start_tenapp_process(self):
        """Start the tenapp process with logging"""
        try:
            # Get the tenapp directory path from config
            if self.config.tenapp_dir:
                tenapp_dir = Path(self.config.tenapp_dir)
            else:
                # Fallback to default relative path
                current_dir = Path(__file__).parent.parent
                tenapp_dir = current_dir / "tenapp"

            if not tenapp_dir.exists():
                self.logger.error(f"Tenapp directory not found: {tenapp_dir}")
                return False

            # Start tenapp process
            self.logger.info(f"Starting tenapp process from {tenapp_dir}")
            self.tenapp_process = subprocess.Popen(
                ["./scripts/start.sh"],
                cwd=tenapp_dir,
                stdout=None,  # Use parent's stdout
                stderr=subprocess.STDOUT,  # Redirect stderr to stdout
                preexec_fn=os.setsid,  # Create new process group
            )

            self.logger.info(
                f"Tenapp process started with PID: {self.tenapp_process.pid}"
            )
            self.logger.info("Tenapp output will be displayed in this console")

            # Start monitoring thread
            monitor_thread = threading.Thread(
                target=self._monitor_tenapp_process
            )
            monitor_thread.daemon = True
            monitor_thread.start()

            return True

        except Exception as e:
            self.logger.error(f"Failed to start tenapp process: {e}")
            return False

    def _monitor_tenapp_process(self):
        """Monitor tenapp process and shutdown if it exits"""
        if not self.tenapp_process:
            return

        self.logger.info("Starting tenapp process monitor")

        while not self.shutdown_event.is_set():
            if self.tenapp_process.poll() is not None:
                # Process has exited
                return_code = self.tenapp_process.returncode
                self.logger.warning(
                    f"Tenapp process exited with code: {return_code}"
                )
                self.logger.info(
                    "Shutting down Plivo server due to tenapp exit"
                )

                # Signal shutdown
                self.shutdown_event.set()

                # Exit the main process
                os.kill(os.getpid(), signal.SIGTERM)
                break

            # Check every second
            self.shutdown_event.wait(1.0)

    def _stop_tenapp_process(self):
        """Stop the tenapp process"""
        if self.tenapp_process:
            self.logger.info("Stopping tenapp process")
            try:
                # Send SIGTERM to the process group
                os.killpg(os.getpgid(self.tenapp_process.pid), signal.SIGTERM)

                # Wait for process to terminate
                try:
                    self.tenapp_process.wait(timeout=10)
                except subprocess.TimeoutExpired:
                    # Force kill if it doesn't terminate gracefully
                    self.logger.warning(
                        "Tenapp process didn't terminate gracefully, force killing"
                    )
                    os.killpg(
                        os.getpgid(self.tenapp_process.pid), signal.SIGKILL
                    )
                    self.tenapp_process.wait()

                self.logger.info("Tenapp process stopped")
            except Exception as e:
                self.logger.error(f"Error stopping tenapp process: {e}")
            finally:
                self.tenapp_process = None

    def _setup_routes(self):
        """Setup FastAPI routes for configuration and health check"""

        @self.app.get("/health")
        async def health_check():
            """Health check endpoint"""
            return JSONResponse(
                content={
                    "status": "healthy",
                    "server_time": datetime.now().isoformat(),
                }
            )

        @self.app.get("/api/config")
        async def get_config():
            """Get server configuration and tenapp server info"""
            # Extract tenapp port from public server URL
            tenapp_port = 9000  # Default port
            if self.config.plivo_public_server_url:
                if ":" in self.config.plivo_public_server_url:
                    try:
                        tenapp_port = int(
                            self.config.plivo_public_server_url.split(":")[-1]
                        )
                    except ValueError:
                        tenapp_port = 9000

            # Build URLs with configurable protocols
            media_ws_url = None
            webhook_url = None

            if self.config.plivo_public_server_url:
                ws_protocol = "wss" if self.config.plivo_use_wss else "ws"
                http_protocol = (
                    "https" if self.config.plivo_use_https else "http"
                )
                media_ws_url = f"{ws_protocol}://{self.config.plivo_public_server_url}/media"
                webhook_url = f"{http_protocol}://{self.config.plivo_public_server_url}/webhook/status"

            return JSONResponse(
                content={
                    "plivo_from_number": self.config.plivo_from_number,
                    "server_port": self.config.plivo_server_port,
                    "tenapp_port": tenapp_port,
                    "tenapp_url": f"http://localhost:{tenapp_port}",
                    "public_server_url": (
                        self.config.plivo_public_server_url
                        if self.config.plivo_public_server_url
                        else None
                    ),
                    "use_https": self.config.plivo_use_https,
                    "use_wss": self.config.plivo_use_wss,
                    "media_stream_enabled": bool(
                        self.config.plivo_public_server_url
                    ),
                    "media_ws_url": media_ws_url,
                    "webhook_enabled": bool(
                        self.config.plivo_public_server_url
                    ),
                    "webhook_url": webhook_url,
                }
            )

    async def start_server(self, host: str = "0.0.0.0", port: int = 8080):
        """Start the server and tenapp process"""
        self.logger.info(
            f"Starting Plivo Configuration Server on {host}:{port}"
        )

        # Start tenapp process first
        if not self._start_tenapp_process():
            self.logger.error("Failed to start tenapp process, exiting")
            sys.exit(1)

        # Wait a moment for tenapp to start
        await asyncio.sleep(2)

        config = uvicorn.Config(
            app=self.app, host=host, port=port, log_level="info"
        )

        server = uvicorn.Server(config)

        try:
            await server.serve()
        except Exception as e:
            self.logger.error(f"Server error: {e}")
            raise
        finally:
            self._stop_tenapp_process()

    def cleanup(self):
        """Cleanup resources"""
        self.logger.info("Cleaning up Plivo Configuration Server")
        self.shutdown_event.set()
        self._stop_tenapp_process()


async def main():
    """Main function to run the server"""
    # Load configuration from environment variables
    config = PlivoServerConfig(
        plivo_auth_id=os.getenv("PLIVO_AUTH_ID", ""),
        plivo_auth_token=os.getenv("PLIVO_AUTH_TOKEN", ""),
        plivo_from_number=os.getenv("PLIVO_FROM_NUMBER", ""),
        plivo_server_port=int(os.getenv("PLIVO_HTTP_PORT", "8080")),
        plivo_public_server_url=os.getenv("PLIVO_PUBLIC_SERVER_URL", ""),
        plivo_use_https=os.getenv("PLIVO_USE_HTTPS", "false").lower()
        == "true",
        plivo_use_wss=os.getenv("PLIVO_USE_WSS", "false").lower() == "true",
    )

    # Create and start server
    server = PlivoServer(config)

    # Setup signal handlers for graceful shutdown
    def signal_handler(signum, frame):
        print(f"Received signal {signum}, shutting down...")
        server.cleanup()
        sys.exit(0)

    signal.signal(signal.SIGINT, signal_handler)
    signal.signal(signal.SIGTERM, signal_handler)

    try:
        await server.start_server(port=config.plivo_server_port)
    except KeyboardInterrupt:
        print("Server interrupted, shutting down...")
        server.cleanup()
    except Exception as e:
        print(f"Server error: {e}")
        server.cleanup()
        sys.exit(1)


if __name__ == "__main__":
    asyncio.run(main())
