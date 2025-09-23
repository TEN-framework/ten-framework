import os
import signal
import sys
import threading
import logging
import json
import time
import asyncio
from typing import Optional, Dict, Any, List
from contextlib import asynccontextmanager
from pathlib import Path

import uvicorn
from fastapi import FastAPI, HTTPException, Depends, status, Request
from fastapi.middleware.cors import CORSMiddleware
from fastapi.responses import JSONResponse
from fastapi.exceptions import RequestValidationError
from pydantic import ValidationError

from .config import settings, PROPERTY_JSON_FILE, MAX_GEMINI_WORKER_COUNT
from .models import (
    PingRequest, StartRequest, StopRequest, GenerateTokenRequest,
    ApiResponse, WorkerInfo, GraphInfo, AddonInfo
)
from .worker_manager import worker_manager
from .utils import process_property_json

# Configure logging
def setup_logging():
    """Setup logging configuration"""
    log_format = "%(asctime)s - %(name)s - %(levelname)s - %(message)s"

    # Configure root logger
    logging.basicConfig(
        level=getattr(logging, settings.log_level),
        format=log_format,
        handlers=[
            logging.StreamHandler() if settings.log_stdout else logging.NullHandler(),
            logging.FileHandler(settings.log_path / "server.log") if not settings.log_stdout else logging.NullHandler()
        ]
    )

    # Set specific loggers
    logging.getLogger("uvicorn").setLevel(logging.INFO)
    logging.getLogger("fastapi").setLevel(logging.INFO)

setup_logging()
logger = logging.getLogger(__name__)

@asynccontextmanager
async def lifespan(app: FastAPI):
    """Application lifespan manager"""
    # Startup
    logger.info("Starting Voice Assistant Server...")
    logger.info(f"Server configuration: {settings.dict()}")

    # Start worker cleanup task
    cleanup_task = asyncio.create_task(worker_cleanup_task())

    try:
        yield
    finally:
        # Shutdown
        logger.info("Shutting down server, cleaning up workers...")
        cleanup_task.cancel()
        try:
            await cleanup_task
        except asyncio.CancelledError:
            pass

        # Cleanup all workers
        worker_manager.cleanup_workers()
        logger.info("Server shutdown complete")

# Initialize FastAPI app with lifespan
app = FastAPI(
    title="Voice Assistant Server",
    version="0.1.0",
    description="A FastAPI server for managing voice assistant workers",
    lifespan=lifespan
)

# Add CORS middleware
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

# Exception handlers
@app.exception_handler(RequestValidationError)
async def validation_exception_handler(request: Request, exc: RequestValidationError):
    """Handle validation errors"""
    logger.error(f"Validation error: {exc}")
    return JSONResponse(
        status_code=status.HTTP_422_UNPROCESSABLE_ENTITY,
        content=ApiResponse(
            code="10000",
            msg="Invalid request parameters",
            data={"errors": exc.errors()}
        ).dict()
    )

@app.exception_handler(HTTPException)
async def http_exception_handler(request: Request, exc: HTTPException):
    """Handle HTTP exceptions"""
    logger.error(f"HTTP error: {exc}")
    return JSONResponse(
        status_code=exc.status_code,
        content=ApiResponse(
            code=str(exc.status_code),
            msg=exc.detail,
            data=None
        ).dict()
    )

@app.exception_handler(Exception)
async def general_exception_handler(request: Request, exc: Exception):
    """Handle general exceptions"""
    logger.error(f"Unexpected error: {exc}", exc_info=True)
    return JSONResponse(
        status_code=status.HTTP_500_INTERNAL_SERVER_ERROR,
        content=ApiResponse(
            code="10000",
            msg="Internal server error",
            data=None
        ).dict()
    )

# Helper functions
def create_response(code: str, msg: str, data: Any = None, status_code: int = 200) -> JSONResponse:
    """Create standardized API response"""
    return JSONResponse(
        status_code=status_code,
        content=ApiResponse(code=code, msg=msg, data=data).dict()
    )

async def worker_cleanup_task():
    """Background task for worker cleanup"""
    while True:
        try:
            await asyncio.sleep(30)  # Run every 30 seconds
            worker_manager.timeout_workers()
        except asyncio.CancelledError:
            break
        except Exception as e:
            logger.error(f"Error in worker cleanup task: {e}")

# API Endpoints
@app.get("/", response_model=ApiResponse)
@app.get("/health", response_model=ApiResponse)
async def health_check():
    """Health check endpoint"""
    return create_response("0", "Server is healthy", {
        "timestamp": time.time(),
        "workers_count": worker_manager.get_worker_count(),
        "max_workers": settings.workers_max
    })

@app.get("/list", response_model=ApiResponse)
async def list_workers():
    """List all running workers"""
    try:
        workers = []
        for channel_name, worker in worker_manager.workers.items():
            workers.append(WorkerInfo(
                channel_name=channel_name,
                create_ts=worker.create_ts,
                graph_name=worker.graph_name,
                pid=worker.pid,
                http_server_port=worker.http_server_port
            ))

        return create_response("0", "success", workers)
    except Exception as e:
        logger.error(f"Failed to list workers: {e}")
        return create_response("10000", "internal error", status_code=500)

@app.get("/graphs", response_model=ApiResponse)
async def get_graphs():
    """Get available graphs from property.json"""
    try:
        from .config import PROPERTY_JSON_FILE
        with open(PROPERTY_JSON_FILE, 'r') as f:
            property_json = json.load(f)

        ten_section = property_json.get("ten", {})
        if not isinstance(ten_section, dict):
            raise ValueError("Invalid format: ten section missing")

        predefined_graphs = ten_section.get("predefined_graphs", [])
        if not isinstance(predefined_graphs, list):
            raise ValueError("Invalid format: predefined_graphs not a list")

        graphs = []
        for graph in predefined_graphs:
            if isinstance(graph, dict) and "name" in graph:
                graphs.append(GraphInfo(
                    name=graph["name"],
                    uuid=graph.get("uuid", ""),
                    auto_start=graph.get("auto_start", False)
                ))

        return create_response("0", "success", graphs)
    except Exception as e:
        logger.error(f"Failed to get graphs: {e}")
        return create_response("10000", "internal error", status_code=500)

@app.post("/ping", response_model=ApiResponse)
async def ping_worker(req: PingRequest):
    """Ping a worker to update its timestamp"""
    try:
        if not req.channel_name or not req.channel_name.strip():
            return create_response("10004", "channel empty", status_code=400)

        if not worker_manager.contains_worker(req.channel_name):
            return create_response("10002", "channel not existed", status_code=400)

        worker = worker_manager.get_worker(req.channel_name)
        worker.update_timestamp()

        logger.info(f"Worker pinged: {req.channel_name}")
        return create_response("0", "success")
    except Exception as e:
        logger.error(f"Failed to ping worker: {e}")
        return create_response("10000", "internal error", status_code=500)

@app.post("/start", response_model=ApiResponse)
async def start_worker(req: StartRequest):
    """Start a new worker"""
    try:
        if not req.channel_name or not req.channel_name.strip():
            return create_response("10004", "channel empty", status_code=400)

        if not req.graph_name or not req.graph_name.strip():
            return create_response("10005", "graph name empty", status_code=400)

        if worker_manager.contains_worker(req.channel_name):
            return create_response("10003", "channel existed", status_code=400)

        if worker_manager.get_worker_count() >= settings.workers_max:
            return create_response("10001", "workers limit", status_code=400)

        # Process property.json
        prop_file, log_file = process_property_json(req, settings)

        # Create worker
        worker = worker_manager.create_worker(
            channel_name=req.channel_name,
            graph_name=req.graph_name,
            property_json_file=prop_file,
            log_file=log_file
        )

        if req.timeout and req.timeout > 0:
            worker.quit_timeout_seconds = req.timeout
        else:
            worker.quit_timeout_seconds = settings.worker_quit_timeout_seconds

        if not worker.start(req):
            return create_response("10101", "start worker failed", status_code=500)

        if not worker_manager.add_worker(req.channel_name, worker):
            return create_response("10003", "channel existed", status_code=400)

        logger.info(f"Worker started successfully: {req.channel_name}")
        return create_response("0", "success")
    except Exception as e:
        logger.error(f"Failed to start worker: {e}")
        return create_response("10100", "process property json failed", status_code=500)

@app.post("/stop", response_model=ApiResponse)
async def stop_worker(req: StopRequest):
    """Stop a worker"""
    try:
        if not req.channel_name or not req.channel_name.strip():
            return create_response("10004", "channel empty", status_code=400)

        if not worker_manager.contains_worker(req.channel_name):
            # Return success if channel doesn't exist (idempotent operation)
            logger.info(f"Channel {req.channel_name} not found, returning success")
            return create_response("0", "success")

        worker = worker_manager.get_worker(req.channel_name)
        if not worker.stop(req.request_id or ""):
            return create_response("10102", "stop worker failed", status_code=500)

        worker_manager.remove_worker(req.channel_name)
        logger.info(f"Worker stopped successfully: {req.channel_name}")
        return create_response("0", "success")
    except Exception as e:
        logger.error(f"Failed to stop worker: {e}")
        return create_response("10000", "internal error", status_code=500)

@app.post("/token/generate", response_model=ApiResponse)
async def generate_token(req: GenerateTokenRequest):
    """Generate Agora RTC token"""
    try:
        if not req.channel_name or not req.channel_name.strip():
            return create_response("10004", "channel empty", status_code=400)

        if not settings.agora_app_id:
            return create_response("10005", "agora app id not configured", status_code=500)

        # Generate token (simplified implementation)
        token = f"token_{req.channel_name}_{req.uid}_{int(time.time())}"

        logger.info(f"Token generated for channel: {req.channel_name}")
        return create_response("0", "success", {"token": token})
    except Exception as e:
        logger.error(f"Failed to generate token: {e}")
        return create_response("10000", "internal error", status_code=500)

@app.get("/dev-tmp/addons/default-properties", response_model=ApiResponse)
async def get_addon_default_properties():
    """Get default properties for addons"""
    try:
        # This would typically load from a configuration file
        addons = [
            AddonInfo(
                addon="agora_rtc",
                property={
                    "app_id": settings.agora_app_id or "",
                    "app_certificate": settings.agora_app_certificate or "",
                    "channel": "",
                    "uid": 0,
                    "token": ""
                }
            )
        ]

        return create_response("0", "success", addons)
    except Exception as e:
        logger.error(f"Failed to get addon properties: {e}")
        return create_response("10000", "internal error", status_code=500)

# Signal handlers for graceful shutdown
def signal_handler(signum, frame):
    """Handle shutdown signals"""
    logger.info(f"Received signal {signum}, shutting down...")
    worker_manager.cleanup_workers()
    sys.exit(0)

signal.signal(signal.SIGINT, signal_handler)
signal.signal(signal.SIGTERM, signal_handler)

if __name__ == "__main__":
    uvicorn.run(
        "server.main:app",
        host=settings.server_host,
        port=settings.server_port,
        reload=False,
        log_level=settings.log_level.lower()
    )
