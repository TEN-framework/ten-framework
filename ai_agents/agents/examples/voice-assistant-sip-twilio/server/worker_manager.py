import os
import time
import signal
import subprocess
import threading
import logging
import uuid
import json
import re
from typing import Dict, Optional, Any, TYPE_CHECKING

if TYPE_CHECKING:
    from .models import StartRequest
from urllib.parse import quote
import requests
from concurrent.futures import ThreadPoolExecutor

logger = logging.getLogger(__name__)

class Worker:
    """Worker class to manage individual agent processes"""

    def __init__(self, channel_name: str, graph_name: str, log_file: str, property_json_file: str):
        self.channel_name = channel_name
        self.graph_name = graph_name
        self.log_file = log_file
        self.property_json_file = property_json_file
        self.pid: Optional[int] = None
        self.http_server_port: Optional[int] = None
        self.quit_timeout_seconds = 60
        self.create_ts = int(time.time())
        self.update_ts = int(time.time())
        self.process: Optional[subprocess.Popen] = None
        self.log_file_handle: Optional[Any] = None

    def start(self, req: 'StartRequest') -> bool:
        """Start the worker process"""
        try:
            # Build command
            shell_cmd = f"tman run start -- --property {self.property_json_file}"
            logger.info(f"Worker start: {shell_cmd}")

            # Start process
            self.process = subprocess.Popen(
                shell_cmd,
                shell=True,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                preexec_fn=os.setsid,  # Start new process group
                text=True
            )

            self.pid = self.process.pid

            # Set up logging
            # Log to file
            self._setup_file_logging()

            # Start monitoring thread
            threading.Thread(target=self._monitor_process, daemon=True).start()

            logger.info(f"Worker started successfully: PID={self.pid}")
            return True

        except Exception as e:
            logger.error(f"Failed to start worker: {e}")
            return False

    def _setup_stdout_logging(self):
        """Set up stdout logging"""
        def log_output(pipe, prefix):
            for line in iter(pipe.readline, ''):
                if line:
                    print(f"[{prefix}] {line.strip()}")
            pipe.close()

        if self.process:
            threading.Thread(target=log_output, args=(self.process.stdout, self.channel_name), daemon=True).start()
            threading.Thread(target=log_output, args=(self.process.stderr, self.channel_name), daemon=True).start()

    def _setup_file_logging(self):
        """Set up file logging"""
        try:
            self.log_file_handle = open(self.log_file, 'a')

            def log_output(pipe, prefix):
                for line in iter(pipe.readline, ''):
                    if line and self.log_file_handle:
                        self.log_file_handle.write(f"[{prefix}] {line}")
                        self.log_file_handle.flush()
                pipe.close()

            if self.process:
                threading.Thread(target=log_output, args=(self.process.stdout, self.channel_name), daemon=True).start()
                threading.Thread(target=log_output, args=(self.process.stderr, self.channel_name), daemon=True).start()

        except Exception as e:
            logger.error(f"Failed to setup file logging: {e}")

    def _monitor_process(self):
        """Monitor the worker process"""
        if self.process:
            self.process.wait()
            logger.info(f"Worker process completed: {self.channel_name}")

            # Clean up
            if self.log_file_handle:
                self.log_file_handle.close()

            # Remove from worker manager
            WorkerManager.remove_worker(self.channel_name)

    def stop(self, request_id: str) -> bool:
        """Stop the worker process"""
        try:
            logger.info(f"Worker stop start: {self.channel_name}, PID={self.pid}")

            if self.process and self.pid:
                # Kill the entire process group
                os.killpg(os.getpgid(self.pid), signal.SIGKILL)
                self.process = None
                self.pid = None

            logger.info(f"Worker stop end: {self.channel_name}")
            return True

        except Exception as e:
            logger.error(f"Failed to stop worker {self.channel_name}: {e}")
            return False

    def update(self, req: 'WorkerUpdateRequest') -> bool:
        """Update worker with new configuration"""
        try:
            logger.info(f"Worker update start: {self.channel_name}")

            if not self.http_server_port:
                logger.error(f"No HTTP server port for worker: {self.channel_name}")
                return False

            # Send update request to worker's HTTP server
            url = f"http://127.0.0.1:{self.http_server_port}/cmd"
            response = requests.post(
                url,
                json=req.dict(),
                headers={"Content-Type": "application/json"},
                timeout=10
            )

            if response.status_code != 200:
                logger.error(f"Worker update failed: {response.status_code}")
                return False

            logger.info(f"Worker update end: {self.channel_name}")
            return True

        except Exception as e:
            logger.error(f"Worker update error: {e}")
            return False

class WorkerManager:
    """Manager for all workers"""

    def __init__(self):
        self._workers: Dict[str, Worker] = {}
        self._http_server_port = 10000
        self._http_server_port_min = 10000
        self._http_server_port_max = 30000
        self._lock = threading.Lock()

    @property
    def workers(self) -> Dict[str, Worker]:
        """Get workers dictionary"""
        return self._workers

    @classmethod
    def get_http_server_port(cls) -> int:
        """Get next available HTTP server port"""
        with cls._lock:
            if cls._http_server_port > cls._http_server_port_max:
                cls._http_server_port = cls._http_server_port_min
            cls._http_server_port += 1
            return cls._http_server_port

    @classmethod
    def create_worker(cls, channel_name: str, graph_name: str,
                     property_json_file: str, log_file: str) -> Worker:
        """Create a new worker instance"""
        return Worker(
            channel_name=channel_name,
            graph_name=graph_name,
            log_file=log_file,
            property_json_file=property_json_file
        )

    @classmethod
    def add_worker(cls, channel_name: str, worker: Worker) -> bool:
        """Add a worker"""
        with cls._lock:
            if channel_name in cls._workers:
                return False
            cls._workers[channel_name] = worker
            return True

    @classmethod
    def remove_worker(cls, channel_name: str) -> bool:
        """Remove a worker"""
        with cls._lock:
            return cls._workers.pop(channel_name, None) is not None

    @classmethod
    def get_worker(cls, channel_name: str) -> Optional[Worker]:
        """Get a worker by channel name"""
        with cls._lock:
            return cls._workers.get(channel_name)

    @classmethod
    def contains_worker(cls, channel_name: str) -> bool:
        """Check if worker exists"""
        with cls._lock:
            return channel_name in cls._workers

    @classmethod
    def get_worker_count(cls) -> int:
        """Get total worker count"""
        with cls._lock:
            return len(cls._workers)

    @classmethod
    def get_worker_list(cls) -> list:
        """Get list of all workers"""
        with cls._lock:
            return [
                {
                    "channel_name": worker.channel_name,
                    "create_ts": worker.create_ts,
                    "graph_name": worker.graph_name,
                    "pid": worker.pid,
                    "http_server_port": worker.http_server_port
                }
                for worker in cls._workers.values()
            ]

    @classmethod
    def get_workers_by_graph_name(cls, graph_name: str) -> list:
        """Get workers by graph name"""
        with cls._lock:
            return [
                worker for worker in cls._workers.values()
                if worker.graph_name == graph_name
            ]

    @classmethod
    def cleanup_workers(cls):
        """Clean up all workers"""
        with cls._lock:
            for channel_name, worker in list(cls._workers.items()):
                try:
                    worker.stop(str(uuid.uuid4()))
                except Exception as e:
                    logger.error(f"Failed to cleanup worker {channel_name}: {e}")
            cls._workers.clear()

    @classmethod
    def timeout_workers(cls, config):
        """Check for timed out workers and clean them up"""
        while True:
            try:
                current_time = int(time.time())
                workers_to_remove = []

                with cls._lock:
                    for channel_name, worker in cls._workers.items():
                        # Skip workers with infinite timeout
                        if worker.quit_timeout_seconds == WORKER_TIMEOUT_INFINITY:
                            continue

                        if worker.update_ts + worker.quit_timeout_seconds < current_time:
                            workers_to_remove.append(channel_name)

                # Remove timed out workers
                for channel_name in workers_to_remove:
                    worker = cls.get_worker(channel_name)
                    if worker:
                        logger.info(f"Timeout worker stop: {channel_name}")
                        worker.stop(str(uuid.uuid4()))
                        cls.remove_worker(channel_name)

                time.sleep(5)  # Check every 5 seconds

            except Exception as e:
                logger.error(f"Error in timeout_workers: {e}")
                time.sleep(5)

# Constants
WORKER_TIMEOUT_INFINITY = -1

# Global worker manager instance
worker_manager = WorkerManager()
