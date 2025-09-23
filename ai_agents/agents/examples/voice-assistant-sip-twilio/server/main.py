#!/usr/bin/env python3
"""
Voice Assistant SIP Twilio Server
Standalone Twilio server application
"""

import asyncio
import os
import sys
import logging
from typing import Optional

# Add current directory to Python path for imports
current_dir = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, current_dir)

from twilio_server import TwilioServer, TwilioServerConfig


def setup_logging():
    """Setup logging configuration"""
    logging.basicConfig(
        level=logging.INFO,
        format='%(asctime)s - %(name)s - %(levelname)s - %(message)s',
        handlers=[
            logging.StreamHandler(sys.stdout),
            logging.FileHandler('/tmp/twilio_server.log')
        ]
    )


def load_config() -> TwilioServerConfig:
    """Load configuration from environment variables"""
    return TwilioServerConfig(
        twilio_account_sid=os.getenv("TWILIO_ACCOUNT_SID", ""),
        twilio_auth_token=os.getenv("TWILIO_AUTH_TOKEN", ""),
        twilio_from_number=os.getenv("TWILIO_FROM_NUMBER", ""),
        twilio_server_webhook_http_port=int(os.getenv("TWILIO_HTTP_PORT", "8080")),
    )


async def main():
    """Main function"""
    setup_logging()
    logger = logging.getLogger(__name__)

    logger.info("Starting Voice Assistant SIP Twilio Server...")

    # Load configuration
    config = load_config()
    logger.info(f"Configuration loaded: HTTP port={config.twilio_server_webhook_http_port}")

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
