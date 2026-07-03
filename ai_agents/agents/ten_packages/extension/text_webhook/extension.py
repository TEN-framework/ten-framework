#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
import base64
from ten_runtime import (
    AudioFrame,
    VideoFrame,
    AsyncExtension,
    AsyncTenEnv,
    Cmd,
    StatusCode,
    CmdResult,
    Data,
)

from ten_ai_base.config import BaseConfig
from dataclasses import dataclass
from typing import Optional, Dict
import json
import asyncio
import aiohttp
import traceback
import time
import uuid

MAX_SIZE = 800  # 1 KB limit
OVERHEAD_ESTIMATE = 200  # Estimate for the overhead of metadata in the JSON

CMD_NAME_FLUSH = "flush"
CMD_IN_ON_USER_JOINED = "on_user_joined"
CMD_IN_ON_USER_LEFT = "on_user_left"

TEXT_DATA_TEXT_FIELD = "text"
TEXT_DATA_FINAL_FIELD = "is_final"
TEXT_DATA_STREAM_ID_FIELD = "stream_id"
TEXT_DATA_END_OF_SEGMENT_FIELD = "end_of_segment"

MAX_CHUNK_SIZE_BYTES = 1024


@dataclass
class WebhookConfig(BaseConfig):
    """Configuration for Webhook extension."""

    url: str = ""
    headers: str = (
        ""  # JSON string representing headers {"Authorization": "Bearer token"}
    )
    method: str = "POST"
    timeout: int = 10
    send_final_only: bool = True  # Only send final text data when True
    data_type: str = (
        "transcribe"  # Type of data being sent (transcribe, raw, etc.)
    )
    send_on_close: bool = (
        False  # Send a conversation end message when extension closes
    )
    send_on_start: bool = (
        True  # Send a conversation start message when extension starts
    )
    direct_forward: bool = (
        False  # Directly forward message collector data without parsing
    )
    send_on_user_events: bool = (
        True  # Send webhook notifications when users join or leave
    )

    def build_parsed_headers(self) -> Dict[str, str]:
        """Parse the headers JSON string into a dictionary."""
        if not self.headers:
            return {}
        try:
            return json.loads(self.headers)
        except json.JSONDecodeError:
            return {}


class textWebhookExtension(AsyncExtension):

    def __init__(self, name: str):
        super().__init__(name)
        self.config: Optional[WebhookConfig] = None
        self.session: Optional[aiohttp.ClientSession] = None
        self.cached_text_map = {}  # Store cached text by stream_id
        self.conversation_id = str(uuid.uuid4())[
            :8
        ]  # Generate a unique ID for this conversation instance
        self.users_count = 0  # Track number of users in the channel
        self._ten_env = None
        self._shutdown_in_progress = False

    async def send_message_to_webhook(
        self,
        text: str,
        ten_env: AsyncTenEnv,
        data: Data,
        is_final: bool,
        end_of_segment: bool,
        stream_id: int,
        text_ts: int = int(time.time() * 1000),
    ) -> bool:
        """
        Send a message to the configured webhook URL.
        Returns True if successful, False otherwise.
        """
        if not self.config.url:
            ten_env.log_error("No webhook URL configured")
            return False

        if not self.session:
            ten_env.log_warn(
                "HTTP session not initialized, creating new session"
            )
            self.session = aiohttp.ClientSession()

        try:
            # Generate a unique message ID
            message_id = str(uuid.uuid4())[:8]

            # Prepare the payload with all available data
            payload = {
                "text": text,
                "is_final": is_final,
                "end_of_segment": end_of_segment,
                "stream_id": stream_id,
                "message_id": message_id,
                "conversation_id": self.conversation_id,
                "data_type": self.config.data_type,
                "text_ts": text_ts,  # Convert to milliseconds
            }

            # Add additional data from the original message if available
            try:
                for prop_name in data.get_property_names():
                    if prop_name not in payload and prop_name not in [
                        TEXT_DATA_TEXT_FIELD,
                        TEXT_DATA_FINAL_FIELD,
                        TEXT_DATA_STREAM_ID_FIELD,
                        TEXT_DATA_END_OF_SEGMENT_FIELD,
                    ]:
                        try:
                            prop_value = data.get_property_string(prop_name)
                            payload[f"original_{prop_name}"] = prop_value
                        except Exception:
                            pass
            except Exception as e:
                ten_env.log_warn(
                    f"Could not extract additional properties: {e}"
                )

            headers = self.config.build_parsed_headers()

            ten_env.log_info(
                f"Sending webhook request to {self.config.url} with method {self.config.method}"
            )
            ten_env.log_info(f"Payload: {json.dumps(payload)}")

            async with self.session.request(
                method=self.config.method,
                url=self.config.url,
                json=payload,
                headers=headers,
                timeout=self.config.timeout,
            ) as response:
                status = response.status
                if 200 <= status < 300:
                    ten_env.log_info(
                        f"Webhook request successful with status {status}"
                    )
                    return True
                else:
                    response_text = await response.text()
                    ten_env.log_error(
                        f"Webhook request failed with status {status}: {response_text}"
                    )
                    return False

        except asyncio.TimeoutError:
            ten_env.log_error(
                f"Webhook request timed out after {self.config.timeout} seconds"
            )
            return False
        except Exception as e:
            ten_env.log_error(f"send_message_to_webhook error: {e}")
            traceback.print_exc()
            return False

    async def send_conversation_start_message(
        self, ten_env: AsyncTenEnv
    ) -> bool:
        """
        Send a special message indicating the conversation has started.
        """
        if not self.config.url or not self.config.send_on_start:
            ten_env.log_info(
                "No URL configured or send_on_start is disabled, skipping start message"
            )
            return False

        # Create a new session if needed
        should_close_session = False
        if self.session is None or self.session.closed:
            ten_env.log_info(
                "Creating new HTTP session for conversation start message"
            )
            self.session = aiohttp.ClientSession()
            should_close_session = True

        try:
            # Generate a unique message ID
            message_id = str(uuid.uuid4())[:8]

            # Create a special payload for conversation start
            payload = {
                "text": "",  # Empty text for start event
                "is_final": True,
                "end_of_segment": False,
                "stream_id": 0,  # Default stream ID
                "message_id": message_id,
                "conversation_id": self.conversation_id,
                "data_type": self.config.data_type,
                "text_ts": int(time.time() * 1000),
                "conversation_start": True,  # Special flag to indicate conversation start
            }

            headers = self.config.build_parsed_headers()

            ten_env.log_info(
                f"Sending conversation start message to {self.config.url}"
            )
            async with self.session.request(
                method=self.config.method,
                url=self.config.url,
                json=payload,
                headers=headers,
                timeout=self.config.timeout,
            ) as response:
                status = response.status
                if 200 <= status < 300:
                    ten_env.log_info(
                        f"Conversation start message successful with status {status}"
                    )
                    return True
                else:
                    response_text = await response.text()
                    ten_env.log_error(
                        f"Conversation start message failed with status {status}: {response_text}"
                    )
                    return False

        except asyncio.TimeoutError:
            ten_env.log_error(
                f"Conversation start message timed out after {self.config.timeout} seconds"
            )
            return False
        except Exception as e:
            ten_env.log_error(f"send_conversation_start_message error: {e}")
            traceback.print_exc()
            return False
        finally:
            # Only close the session if we created a new one - otherwise keep it open for further messages
            if (
                should_close_session
                and self.session
                and not self.session.closed
            ):
                try:
                    await self.session.close()
                    self.session = None
                except Exception as e:
                    ten_env.log_error(f"Error closing session: {e}")

    async def send_conversation_end_message(self, ten_env: AsyncTenEnv) -> bool:
        """
        Send a special message indicating the conversation has ended.
        """
        ten_env.log_info(
            f"Attempting to send conversation end message (send_on_close={self.config.send_on_close}, url={self.config.url})"
        )

        if not self.config.url or not self.config.send_on_close:
            ten_env.log_info(
                "No URL configured or send_on_close is disabled, skipping end message"
            )
            return False

        # Create a new session if needed (always create a fresh session for the end message to ensure delivery)
        should_close_session = False
        if self.session is None or self.session.closed:
            ten_env.log_info(
                "Creating new HTTP session for conversation end message"
            )
            self.session = aiohttp.ClientSession()
            should_close_session = True
        else:
            ten_env.log_info(
                "Using existing HTTP session for conversation end message"
            )

        try:
            # Generate a unique message ID
            message_id = str(uuid.uuid4())[:8]

            # Create a special payload for conversation end
            payload = {
                "text": "",  # Empty text for close event
                "is_final": True,
                "end_of_segment": True,
                "stream_id": 0,  # Default stream ID
                "message_id": message_id,
                "conversation_id": self.conversation_id,
                "data_type": self.config.data_type,
                "text_ts": int(time.time() * 1000),
                "conversation_end": True,  # Special flag to indicate conversation end
            }

            headers = self.config.build_parsed_headers()

            ten_env.log_info(
                f"END WEBHOOK - Sending conversation end message to {self.config.url}"
            )
            ten_env.log_info(f"END WEBHOOK - Payload: {json.dumps(payload)}")
            ten_env.log_info(f"END WEBHOOK - Headers: {headers}")

            async with self.session.request(
                method=self.config.method,
                url=self.config.url,
                json=payload,
                headers=headers,
                timeout=self.config.timeout,
            ) as response:
                status = response.status
                if 200 <= status < 300:
                    ten_env.log_info(
                        f"END WEBHOOK - Conversation end message successful with status {status}"
                    )
                    return True
                else:
                    response_text = await response.text()
                    ten_env.log_error(
                        f"END WEBHOOK - Conversation end message failed with status {status}: {response_text}"
                    )
                    return False

        except asyncio.TimeoutError:
            ten_env.log_error(
                f"END WEBHOOK - Conversation end message timed out after {self.config.timeout} seconds"
            )
            return False
        except Exception as e:
            ten_env.log_error(
                f"END WEBHOOK - send_conversation_end_message error: {e}"
            )
            traceback.print_exc()
            return False
        finally:
            # Close the session if we created a new one
            if (
                should_close_session
                and self.session
                and not self.session.closed
            ):
                try:
                    ten_env.log_info("END WEBHOOK - Closing temporary session")
                    await self.session.close()
                    self.session = None
                except Exception as e:
                    ten_env.log_error(
                        f"END WEBHOOK - Error closing session: {e}"
                    )

    async def send_user_event_message(
        self, ten_env: AsyncTenEnv, event_type: str, user_id: str = ""
    ) -> bool:
        """
        Send a message indicating a user has joined or left the conversation.

        Args:
            ten_env: The TEN environment
            event_type: Either "joined" or "left"
            user_id: Optional user identifier

        Returns:
            True if successful, False otherwise
        """
        if not self.config.url or not self.config.send_on_user_events:
            ten_env.log_info(
                f"No URL configured or send_on_user_events is disabled, skipping {event_type} message"
            )
            return False

        # Create a new session if needed
        if not self.session:
            ten_env.log_info(
                "HTTP session not initialized, creating new session"
            )
            self.session = aiohttp.ClientSession()

        try:
            # Generate a unique message ID
            message_id = str(uuid.uuid4())[:8]

            # Create payload for user event
            payload = {
                "text": "",  # Empty text for event
                "is_final": True,
                "end_of_segment": False,
                "stream_id": 0,  # Default stream ID
                "message_id": message_id,
                "conversation_id": self.conversation_id,
                "data_type": self.config.data_type,
                "text_ts": int(time.time() * 1000),
                "user_event": event_type,  # joined or left
                "user_count": self.users_count,
            }

            # Add user ID if provided
            if user_id:
                payload["user_id"] = user_id

            headers = self.config.build_parsed_headers()

            ten_env.log_info(
                f"Sending user {event_type} message to {self.config.url}"
            )
            async with self.session.request(
                method=self.config.method,
                url=self.config.url,
                json=payload,
                headers=headers,
                timeout=self.config.timeout,
            ) as response:
                status = response.status
                if 200 <= status < 300:
                    ten_env.log_info(
                        f"User {event_type} message successful with status {status}"
                    )
                    return True
                else:
                    response_text = await response.text()
                    ten_env.log_error(
                        f"User {event_type} message failed with status {status}: {response_text}"
                    )
                    return False

        except asyncio.TimeoutError:
            ten_env.log_error(
                f"User {event_type} message timed out after {self.config.timeout} seconds"
            )
            return False
        except Exception as e:
            ten_env.log_error(
                f"send_user_event_message error for {event_type}: {e}"
            )
            traceback.print_exc()
            return False

    async def _ensure_conversation_end_sent(self, ten_env: AsyncTenEnv) -> None:
        """
        Ensure the conversation end message is sent if configured.
        This method is called in all lifecycle termination methods to maximize chances of sending.
        """
        if self._shutdown_in_progress:
            ten_env.log_info(
                "Shutdown already in progress, skipping duplicate end message"
            )
            return

        self._shutdown_in_progress = True
        ten_env.log_info("Ensuring conversation end message is sent")

        # Send conversation end message if configured
        if self.config and self.config.send_on_close:
            ten_env.log_info("Sending final conversation end message")
            try:
                await self.send_conversation_end_message(ten_env)
            except Exception as e:
                ten_env.log_error(f"Error sending final end message: {e}")
                traceback.print_exc()

        # Ensure session is closed
        if self.session and not self.session.closed:
            try:
                await self.session.close()
                self.session = None
                ten_env.log_info("HTTP session closed")
            except Exception as e:
                ten_env.log_error(f"Error closing session: {e}")

    async def on_init(self, ten_env: AsyncTenEnv) -> None:
        ten_env.log_debug("on_init")
        self.config = await WebhookConfig.create_async(ten_env=ten_env)
        self._ten_env = ten_env

    async def on_start(self, ten_env: AsyncTenEnv) -> None:
        ten_env.log_debug("on_start")

        if not self.config.url:
            ten_env.log_warn("No webhook URL configured")
        else:
            ten_env.log_info(f"Webhook URL: {self.config.url}")
            ten_env.log_info(f"Webhook method: {self.config.method}")
            ten_env.log_info(f"Webhook timeout: {self.config.timeout} seconds")
            ten_env.log_info(f"Send final only: {self.config.send_final_only}")
            ten_env.log_info(f"Data type: {self.config.data_type}")
            ten_env.log_info(f"Send on close: {self.config.send_on_close}")
            ten_env.log_info(f"Send on start: {self.config.send_on_start}")
            ten_env.log_info(
                f"Send on user events: {self.config.send_on_user_events}"
            )
            ten_env.log_info(
                f"Direct forward mode: {self.config.direct_forward}"
            )
            ten_env.log_info(f"Conversation ID: {self.conversation_id}")

            # Create HTTP session for reuse
            self.session = aiohttp.ClientSession()

            # Send conversation start message if configured
            if self.config.send_on_start:
                ten_env.log_info("Sending conversation start message")
                await self.send_conversation_start_message(ten_env)

    async def on_stop(self, ten_env: AsyncTenEnv) -> None:
        ten_env.log_info("====== LIFECYCLE: on_stop called ======")
        ten_env.log_debug("on_stop")

        # Ensure end message is sent
        await self._ensure_conversation_end_sent(ten_env)

    async def on_close(self, ten_env: AsyncTenEnv) -> None:
        # Added on_close to handle extension closure
        ten_env.log_info("====== LIFECYCLE: on_close called ======")
        ten_env.log_debug("on_close")

        # Ensure end message is sent
        await self._ensure_conversation_end_sent(ten_env)

        # Clear cached data
        self.cached_text_map.clear()

    async def on_deinit(self, ten_env: AsyncTenEnv) -> None:
        ten_env.log_info("====== LIFECYCLE: on_deinit called ======")
        ten_env.log_debug("on_deinit")

        # Final chance to ensure end message is sent
        await self._ensure_conversation_end_sent(ten_env)

        # Clear cached data
        self.cached_text_map.clear()

    async def on_cmd(self, ten_env: AsyncTenEnv, cmd: Cmd) -> None:
        cmd_name = cmd.get_name()
        ten_env.log_debug(f"on_cmd name {cmd_name}")

        status = StatusCode.OK

        if cmd_name == CMD_NAME_FLUSH:
            ten_env.log_info("Received flush command")

            # Close existing session if any (will be recreated when needed)
            if self.session:
                await self.session.close()
                self.session = None

            # Clear cached text
            self.cached_text_map.clear()

            # Send flush acknowledgment
            await ten_env.send_cmd(Cmd.create(CMD_NAME_FLUSH))
        elif cmd_name == CMD_IN_ON_USER_JOINED:
            # Track user count
            self.users_count += 1
            ten_env.log_info(f"User joined. Total users: {self.users_count}")

            # Extract user_id if available
            user_id = ""
            try:
                user_id = cmd.get_property_string("user_id")
            except Exception:
                pass

            # Send event notification
            await self.send_user_event_message(ten_env, "joined", user_id)
        elif cmd_name == CMD_IN_ON_USER_LEFT:
            # Update user count
            if self.users_count > 0:
                self.users_count -= 1
            ten_env.log_info(f"User left. Total users: {self.users_count}")

            # Extract user_id if available
            user_id = ""
            try:
                user_id = cmd.get_property_string("user_id")
            except Exception:
                pass

            # Send event notification
            await self.send_user_event_message(ten_env, "left", user_id)

        cmd_result = CmdResult.create(status, cmd)
        await ten_env.return_result(cmd_result)

    async def on_data(self, ten_env: AsyncTenEnv, data: Data) -> None:
        data_name = data.get_name()
        ten_env.log_info(f"on_data received: name={data_name}")

        # Direct forward mode for message collector compatibility
        if self.config.direct_forward and data_name == "data":
            try:
                raw_data = data.get_property_buf("data")
                if raw_data:
                    decoded_data = raw_data.decode("utf-8", errors="replace")
                    # decode_data is in base64 format
                    decoded_data = base64.b64decode(raw_data).decode(
                        "utf-8", errors="replace"
                    )
                    json_data = json.loads(decoded_data)
                    ten_env.log_info(
                        f"Direct forward mode: Sending raw data as text: '{decoded_data}'"
                    )

                    # Create a simple direct forward payload
                    payload = {
                        "text": decoded_data,
                        "is_final": True,
                        "message_id": str(uuid.uuid4())[:8],
                        "conversation_id": self.conversation_id,
                        "timestamp": int(time.time() * 1000),
                        "direct_forward": True,
                    }

                    # Send directly without parsing
                    if self.session is None:
                        self.session = aiohttp.ClientSession()

                    headers = self.config.build_parsed_headers()

                    ten_env.log_info(f"Direct forwarding to {self.config.url}")
                    async with self.session.request(
                        method=self.config.method,
                        url=self.config.url,
                        json=payload,
                        headers=headers,
                        timeout=self.config.timeout,
                    ) as response:
                        status = response.status
                        if 200 <= status < 300:
                            ten_env.log_info(
                                f"Direct forward successful with status {status}"
                            )
                        else:
                            response_text = await response.text()
                            ten_env.log_error(
                                f"Direct forward failed with status {status}: {response_text}"
                            )

                    # Early return to skip normal processing
                    return
            except Exception as e:
                ten_env.log_error(f"Error in direct forward mode: {e}")
                traceback.print_exc()

        # Dump all properties for debugging
        try:
            properties = {}
            try:
                properties[TEXT_DATA_TEXT_FIELD] = data.get_property_string(
                    TEXT_DATA_TEXT_FIELD
                )
            except Exception:
                pass

            try:
                properties[TEXT_DATA_FINAL_FIELD] = data.get_property_bool(
                    TEXT_DATA_FINAL_FIELD
                )
            except Exception:
                pass

            try:
                properties[TEXT_DATA_STREAM_ID_FIELD] = data.get_property_int(
                    TEXT_DATA_STREAM_ID_FIELD
                )
            except Exception:
                pass

            try:
                properties[TEXT_DATA_END_OF_SEGMENT_FIELD] = (
                    data.get_property_bool(TEXT_DATA_END_OF_SEGMENT_FIELD)
                )
            except Exception:
                pass

            # Try to get all property names
            # try:
            #     all_props = data.get_property_names()
            #     ten_env.log_info(f"All property names: {all_props}")
            # except Exception as e:
            #     ten_env.log_warn(f"Could not get all property names: {e}")

            ten_env.log_info(f"Data properties: {properties}")
        except Exception as e:
            ten_env.log_error(f"Error dumping properties: {e}")

        # Process all data types, not just "data" or "text_data"
        try:
            text = ""

            # Check for message collector style "data" property first (takes precedence)
            if data_name == "data":
                try:
                    # This is likely from message collector
                    raw_data = data.get_property_buf("data")
                    if raw_data:
                        decoded_data = raw_data.decode(
                            "utf-8", errors="replace"
                        )
                        ten_env.log_info(
                            f"Found message collector data: '{decoded_data}'"
                        )

                        # Try to parse the encoded message from message collector
                        try:
                            # Message collector sends formatted data with message_id|part_index|total_parts|content
                            parts = decoded_data.split("|", 3)
                            if len(parts) >= 4:
                                encoded_content = parts[3]
                                # Try to treat it as a JSON payload
                                encoded_content = base64.b64decode(
                                    encoded_content
                                ).decode("utf-8", errors="replace")
                                try:
                                    json_data = json.loads(encoded_content)
                                    if (
                                        isinstance(json_data, dict)
                                        and "text" in json_data
                                    ):
                                        text = json_data["text"]
                                        ten_env.log_info(
                                            f"Successfully parsed JSON from message collector data: text={text}"
                                        )

                                        # Extract other fields if available
                                        is_final = json_data.get(
                                            "is_final", True
                                        )
                                        stream_id = json_data.get(
                                            "stream_id", 0
                                        )
                                        end_of_segment = json_data.get(
                                            "end_of_segment", False
                                        )

                                        text_ts = json_data.get(
                                            "text_ts", int(time.time() * 1000)
                                        )

                                        # Check if we should send based on configuration (same logic as standard path)
                                        should_send = True
                                        if (
                                            self.config.send_final_only
                                            and not is_final
                                        ):
                                            ten_env.log_info(
                                                "Skipping non-final message collector text due to send_final_only=true"
                                            )
                                            should_send = False

                                        # Send to webhook if appropriate
                                        if should_send:
                                            ten_env.log_info(
                                                f"Sending parsed message collector data to webhook: text='{text}'"
                                            )
                                            await self.send_message_to_webhook(
                                                text,
                                                ten_env,
                                                data,
                                                is_final,
                                                end_of_segment,
                                                stream_id,
                                                text_ts,
                                            )
                                        else:
                                            ten_env.log_info(
                                                "Not sending message collector data due to configuration"
                                            )
                                        return
                                except json.JSONDecodeError:
                                    # Not JSON, use the raw content
                                    text = encoded_content
                                    ten_env.log_info(
                                        f"Using raw content from message collector: {text}"
                                    )
                        except Exception as e:
                            ten_env.log_warn(
                                f"Error parsing message collector format: {e}"
                            )
                            text = decoded_data
                except Exception as e:
                    ten_env.log_warn(f"Error processing 'data' property: {e}")

            # Fall back to standard text processing if message collector handling didn't work
            if not text:
                try:
                    text = data.get_property_string(TEXT_DATA_TEXT_FIELD)
                    ten_env.log_info(f"Found text: '{text}'")
                except Exception:
                    ten_env.log_warn(
                        f"Data does not have '{TEXT_DATA_TEXT_FIELD}' property"
                    )
                    # Try to extract text from raw data if available and we haven't already tried
                    try:
                        if data_name != "data":
                            raw_data = data.get_property_buf("data")
                            if raw_data:
                                text = raw_data.decode(
                                    "utf-8", errors="replace"
                                )
                                ten_env.log_info(
                                    f"Extracted text from raw data: '{text}'"
                                )
                    except Exception as e:
                        ten_env.log_info(f"No raw data available: {e}")

            # Get optional fields with defaults
            is_final = True  # Default to true
            end_of_segment = False
            stream_id = 0

            try:
                is_final = data.get_property_bool(TEXT_DATA_FINAL_FIELD)
            except Exception as e:
                ten_env.log_warn(f"Error getting is_final property: {e}")

            try:
                end_of_segment = data.get_property_bool(
                    TEXT_DATA_END_OF_SEGMENT_FIELD
                )
            except Exception as e:
                ten_env.log_warn(f"Error getting end_of_segment property: {e}")

            try:
                stream_id = data.get_property_int(TEXT_DATA_STREAM_ID_FIELD)
            except Exception as e:
                ten_env.log_warn(f"Error getting stream_id property: {e}")

            ten_env.log_info(
                f"Processed data: text='{text}', final={is_final}, end_of_segment={end_of_segment}, stream_id={stream_id}"
            )

            # Cache text handling following MessageCollectorExtension pattern
            if end_of_segment:
                if stream_id in self.cached_text_map:
                    ten_env.log_info(
                        f"Appending cached text for stream_id={stream_id}"
                    )
                    self.cached_text_map[stream_id] = (
                        self.cached_text_map[stream_id] + text
                    )
                    # text = (
                    #     self.cached_text_map[stream_id] + text
                    # )
                    # del self.cached_text_map[stream_id]
            else:
                if is_final:
                    if stream_id in self.cached_text_map:
                        ten_env.log_info(
                            f"Using cached text for stream_id={stream_id}"
                        )
                        text = self.cached_text_map[stream_id] + text
                        del self.cached_text_map[stream_id]
                    # self.cached_text_map[stream_id] = text
                    # ten_env.log_info(
                    #     f"Cached text for stream_id={stream_id}: '{text}'"
                    # )

            # Check if we should send this data based on configuration
            should_send = True
            if self.config.send_final_only and not is_final:
                ten_env.log_info(
                    "Skipping non-final text due to send_final_only=true"
                )
                should_send = False

            # Send the data to webhook if appropriate
            if should_send:
                ten_env.log_info(
                    f"Sending webhook: text='{text}', final={is_final}, end_of_segment={end_of_segment}, stream_id={stream_id}"
                )
                await self.send_message_to_webhook(
                    text, ten_env, data, is_final, end_of_segment, stream_id
                )
            else:
                ten_env.log_info(
                    "Not sending webhook due to configuration or empty text"
                )

        except Exception as e:
            ten_env.log_error(f"Error processing data: {e}")
            traceback.print_exc()

    async def on_audio_frame(
        self, ten_env: AsyncTenEnv, audio_frame: AudioFrame
    ) -> None:
        audio_frame_name = audio_frame.get_name()
        ten_env.log_debug(f"on_audio_frame name {audio_frame_name}")
        # Not implemented for this extension

    async def on_video_frame(
        self, ten_env: AsyncTenEnv, video_frame: VideoFrame
    ) -> None:
        video_frame_name = video_frame.get_name()
        ten_env.log_debug(f"on_video_frame name {video_frame_name}")
        # Not implemented for this extension
