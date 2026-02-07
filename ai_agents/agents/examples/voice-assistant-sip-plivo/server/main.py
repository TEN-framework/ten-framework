#!/usr/bin/env python3
"""
Voice Assistant SIP Plivo Server
Standalone Plivo server application
"""

import asyncio
import os
import sys
import logging
import argparse
from typing import Optional

# Add current directory to Python path for imports
current_dir = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, current_dir)

from plivo_server import PlivoServer, PlivoServerConfig


def parse_arguments():
    """Parse command line arguments"""
    parser = argparse.ArgumentParser(
        description="Voice Assistant SIP Plivo Server",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  python main.py --tenapp-dir /path/to/tenapp
  python main.py --tenapp-dir ../tenapp --port 9000
        """,
    )

    parser.add_argument(
        "--tenapp-dir",
        type=str,
        default="",
        help="Path to tenapp directory (default: ../tenapp relative to server directory)",
    )

    parser.add_argument(
        "--port",
        type=int,
        default=8080,
        help="Port for the Plivo server (default: 8080)",
    )

    return parser.parse_args()


def setup_logging():
    """Setup logging configuration"""
    logging.basicConfig(
        level=logging.INFO,
        format="%(asctime)s - %(name)s - %(levelname)s - %(message)s",
        handlers=[
            logging.StreamHandler(sys.stdout),
            logging.FileHandler("/tmp/plivo_server.log"),
        ],
    )


def load_config(args) -> PlivoServerConfig:
    """Load configuration from environment variables and command line arguments"""
    return PlivoServerConfig(
        plivo_auth_id=os.getenv("PLIVO_AUTH_ID", ""),
        plivo_auth_token=os.getenv("PLIVO_AUTH_TOKEN", ""),
        plivo_from_number=os.getenv("PLIVO_FROM_NUMBER", ""),
        plivo_server_port=args.port,
        plivo_public_server_url=os.getenv("PLIVO_PUBLIC_SERVER_URL", ""),
        plivo_use_https=os.getenv("PLIVO_USE_HTTPS", "false").lower()
        == "true",
        plivo_use_wss=os.getenv("PLIVO_USE_WSS", "false").lower() == "true",
        tenapp_dir=args.tenapp_dir,
    )


async def main():
    """Main function"""
    # Parse command line arguments
    args = parse_arguments()

    setup_logging()
    logger = logging.getLogger(__name__)

    logger.info("Starting Voice Assistant SIP Plivo Server...")
    logger.info(f"Tenapp directory: {args.tenapp_dir or 'default (../tenapp)'}")
    logger.info(f"Server port: {args.port}")

    # Load configuration
    config = load_config(args)
    logger.info(f"Configuration loaded: HTTP port={config.plivo_server_port}")

    # Create Plivo server
    plivo_server = PlivoServer(config)

    # Start HTTP server
    logger.info("Starting HTTP server...")

    # Start the server
    await plivo_server.start_server()


if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("\nShutting down server...")
    except Exception as e:
        print(f"Server error: {e}")
        sys.exit(1)
