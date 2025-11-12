#!/usr/bin/env python3
"""
Voice Assistant SIP Twilio Server
Standalone Twilio server application
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

from twilio_server import TwilioServer, TwilioServerConfig


def parse_arguments():
    """Parse command line arguments"""
    parser = argparse.ArgumentParser(
        description="Voice Assistant SIP Twilio Server",
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
        help="Port for the Twilio server (default: 8080)",
    )

    return parser.parse_args()


def setup_logging():
    """Setup logging configuration"""
    logging.basicConfig(
        level=logging.INFO,
        format="%(asctime)s - %(name)s - %(levelname)s - %(message)s",
        handlers=[
            logging.StreamHandler(sys.stdout),
            logging.FileHandler("/tmp/twilio_server.log"),
        ],
    )


def load_config(args) -> TwilioServerConfig:
    """Load configuration from environment variables and command line arguments"""
    return TwilioServerConfig(
        twilio_account_sid=os.getenv("TWILIO_ACCOUNT_SID", ""),
        twilio_auth_token=os.getenv("TWILIO_AUTH_TOKEN", ""),
        twilio_from_number=os.getenv("TWILIO_FROM_NUMBER", ""),
        twilio_server_port=args.port,
        twilio_public_server_url=os.getenv("TWILIO_PUBLIC_SERVER_URL", ""),
        twilio_use_https=os.getenv("TWILIO_USE_HTTPS", "false").lower()
        == "true",
        twilio_use_wss=os.getenv("TWILIO_USE_WSS", "false").lower() == "true",
        tenapp_dir=args.tenapp_dir,
    )


async def main():
    """Main function"""
    # Parse command line arguments
    args = parse_arguments()

    setup_logging()
    logger = logging.getLogger(__name__)

    logger.info("Starting Voice Assistant SIP Twilio Server...")
    logger.info(f"Tenapp directory: {args.tenapp_dir or 'default (../tenapp)'}")
    logger.info(f"Server port: {args.port}")

    # Load configuration
    config = load_config(args)
    logger.info(f"Configuration loaded: HTTP port={config.twilio_server_port}")

    # Create Twilio server
    twilio_server = TwilioServer(config)

    # Start HTTP server
    logger.info("Starting HTTP server...")

    # Start the server
    await twilio_server.start_server()


if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("\nShutting down server...")
    except Exception as e:
        print(f"Server error: {e}")
        sys.exit(1)
