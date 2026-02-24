#!/usr/bin/env python3
"""
Voice Assistant SIP Telnyx Server
Standalone Telnyx server application
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

from telnyx_server import TelnyxServer, TelnyxServerConfig


def parse_arguments():
    """Parse command line arguments"""
    parser = argparse.ArgumentParser(
        description="Voice Assistant SIP Telnyx Server",
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
        help="Port for the Telnyx server (default: 8080)",
    )

    return parser.parse_args()


def setup_logging():
    """Setup logging configuration"""
    logging.basicConfig(
        level=logging.INFO,
        format="%(asctime)s - %(name)s - %(levelname)s - %(message)s",
        handlers=[
            logging.StreamHandler(sys.stdout),
            logging.FileHandler("/tmp/telnyx_server.log"),
        ],
    )


def load_config(args) -> TelnyxServerConfig:
    """Load configuration from environment variables and command line arguments"""
    return TelnyxServerConfig(
        telnyx_api_key=os.getenv("TELNYX_API_KEY", ""),
        telnyx_connection_id=os.getenv("TELNYX_CONNECTION_ID", ""),
        telnyx_from_number=os.getenv("TELNYX_FROM_NUMBER", ""),
        telnyx_server_port=args.port,
        telnyx_public_server_url=os.getenv("TELNYX_PUBLIC_SERVER_URL", ""),
        telnyx_use_https=os.getenv("TELNYX_USE_HTTPS", "false").lower()
        == "true",
        telnyx_use_wss=os.getenv("TELNYX_USE_WSS", "false").lower() == "true",
        tenapp_dir=args.tenapp_dir,
    )


async def main():
    """Main function"""
    # Parse command line arguments
    args = parse_arguments()

    setup_logging()
    logger = logging.getLogger(__name__)

    logger.info("Starting Voice Assistant SIP Telnyx Server...")
    logger.info(f"Tenapp directory: {args.tenapp_dir or 'default (../tenapp)'}")
    logger.info(f"Server port: {args.port}")

    # Load configuration
    config = load_config(args)
    logger.info(f"Configuration loaded: HTTP port={config.telnyx_server_port}")

    # Create Telnyx server
    telnyx_server = TelnyxServer(config)

    # Start HTTP server
    logger.info("Starting HTTP server...")

    # Start the server
    await telnyx_server.start_server()


if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("\nShutting down server...")
    except Exception as e:
        print(f"Server error: {e}")
        sys.exit(1)