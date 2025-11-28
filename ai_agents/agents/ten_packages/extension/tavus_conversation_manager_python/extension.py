from ten_runtime import AsyncExtension, AsyncTenEnv, Cmd, CmdResult, StatusCode
import httpx
import asyncio


class TavusConversationManagerExtension(AsyncExtension):
    def __init__(self, name: str):
        super().__init__(name)
        self.api_key = None
        self.persona_id = None
        self.default_greeting = None
        self.conversations = {}  # Store active conversations

    async def on_init(self, ten_env: AsyncTenEnv):
        self.api_key, _ = await ten_env.get_property_string("tavus_api_key")
        self.persona_id, _ = await ten_env.get_property_string("persona_id")
        self.default_greeting, _ = await ten_env.get_property_string("default_greeting")

        ten_env.log_info(f"Tavus manager initialized")
        if self.persona_id:
            ten_env.log_info(f"Using persona: {self.persona_id}")
        else:
            ten_env.log_info("Using default Tavus persona")

    async def on_cmd(self, ten_env: AsyncTenEnv, cmd: Cmd):
        cmd_name = cmd.get_name()

        if cmd_name == "create_conversation":
            await self._create_conversation(ten_env, cmd)
        elif cmd_name == "end_conversation":
            await self._end_conversation(ten_env, cmd)
        else:
            ten_env.log_warn(f"Unknown command: {cmd_name}")
            cmd_result = CmdResult.create(StatusCode.ERROR)
            cmd_result.set_property_string("error", f"Unknown command: {cmd_name}")
            await ten_env.return_result(cmd_result, cmd)

    async def _create_conversation(self, ten_env: AsyncTenEnv, cmd: Cmd):
        try:
            # Prepare request payload
            payload = {
                "conversational_context": "You are a helpful AI assistant.",
                "custom_greeting": self.default_greeting
            }

            # Add persona_id if provided
            if self.persona_id:
                payload["persona_id"] = self.persona_id

            # Call Tavus API
            async with httpx.AsyncClient() as client:
                response = await client.post(
                    "https://tavusapi.com/v2/conversations",
                    headers={
                        "x-api-key": self.api_key,
                        "Content-Type": "application/json"
                    },
                    json=payload,
                    timeout=30.0
                )
                response.raise_for_status()
                data = response.json()

                conversation_id = data["conversation_id"]
                conversation_url = data["conversation_url"]

                # Store conversation
                self.conversations[conversation_id] = data

                # Return success result
                cmd_result = CmdResult.create(StatusCode.OK)
                cmd_result.set_property_string("conversation_id", conversation_id)
                cmd_result.set_property_string("conversation_url", conversation_url)
                await ten_env.return_result(cmd_result, cmd)

                ten_env.log_info(f"Created conversation: {conversation_id}")
                ten_env.log_info(f"Conversation URL: {conversation_url}")

        except httpx.HTTPStatusError as e:
            error_msg = f"Tavus API error: {e.response.status_code} - {e.response.text}"
            ten_env.log_error(error_msg)
            cmd_result = CmdResult.create(StatusCode.ERROR)
            cmd_result.set_property_string("error", error_msg)
            await ten_env.return_result(cmd_result, cmd)
        except Exception as e:
            error_msg = f"Failed to create conversation: {str(e)}"
            ten_env.log_error(error_msg)
            cmd_result = CmdResult.create(StatusCode.ERROR)
            cmd_result.set_property_string("error", error_msg)
            await ten_env.return_result(cmd_result, cmd)

    async def _end_conversation(self, ten_env: AsyncTenEnv, cmd: Cmd):
        conversation_id, _ = cmd.get_property_string("conversation_id")

        if conversation_id in self.conversations:
            del self.conversations[conversation_id]
            ten_env.log_info(f"Ended conversation: {conversation_id}")
        else:
            ten_env.log_warn(f"Conversation not found: {conversation_id}")

        cmd_result = CmdResult.create(StatusCode.OK)
        await ten_env.return_result(cmd_result, cmd)
