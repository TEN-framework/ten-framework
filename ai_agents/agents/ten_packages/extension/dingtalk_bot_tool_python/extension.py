"""
DingTalk Bot Extension
"""

import json
import time
import hmac
import hashlib
import base64
import urllib.parse
import requests
from dataclasses import dataclass

from ten_runtime import AsyncTenEnv, Cmd
from ten_ai_base.config import BaseConfig
from ten_ai_base.types import (
    LLMToolMetadata,
    LLMToolMetadataParameter,
    LLMToolResult,
    LLMToolResultLLMResult,
)
from ten_ai_base.llm_tool import AsyncLLMToolBaseExtension

# Tool constants
TOOL_NAME = "send_message"
TOOL_DESCRIPTION = "Send a message to DingTalk group chat. Use this when user wants to notify team members or send information to DingTalk."
TOOL_PARAMETERS = {
    "type": "object",
    "properties": {
        "content": {
            "type": "string",
            "description": "The message content to send to DingTalk group",
        }
    },
    "required": ["content"],
}


@dataclass
class DingTalkBotConfig(BaseConfig):
    """Configuration for DingTalk Bot extension"""

    access_token: str = ""
    secret: str = ""


class DingTalkBotExtension(AsyncLLMToolBaseExtension):
    """
    DingTalk Bot Extension
    """

    def __init__(self, name: str = "dingtalk_bot_tool_python"):
        super().__init__(name)
        self.config: DingTalkBotConfig = None

    async def on_init(self, ten_env: AsyncTenEnv) -> None:
        """Initialize the extension and load configuration"""
        ten_env.log_info("[DingTalkBotExtension] on_init")

    async def on_start(self, ten_env: AsyncTenEnv) -> None:
        """Start the extension and register tools"""
        ten_env.log_info(
            "[DingTalkBotExtension] ========== on_start BEGIN =========="
        )

        # Load configuration
        self.config = await DingTalkBotConfig.create_async(ten_env=ten_env)
        ten_env.log_info("[DingTalkBotExtension] Config loaded successfully")
        ten_env.log_info(
            f"[DingTalkBotExtension]   - access_token: {'SET (length=' + str(len(self.config.access_token)) + ')' if self.config.access_token else 'NOT SET'}"
        )
        ten_env.log_info(
            f"[DingTalkBotExtension]   - secret: {'SET (length=' + str(len(self.config.secret)) + ')' if self.config.secret else 'NOT SET'}"
        )

        # main_control 期望 parameters 为 LLMToolMetadataParameter 数组格式
        # 需要将其序列化为 JSON
        tool_metadata_for_registration = {
            "name": TOOL_NAME,
            "description": TOOL_DESCRIPTION,
            "parameters": [  # 数组格式，符合 LLMToolMetadata 定义
                {
                    "name": "content",
                    "type": "string",
                    "description": "The message content to send to DingTalk group",
                    "required": True,
                }
            ],
        }

        ten_env.log_info(
            "[DingTalkBotExtension] Registering tool with parameter array format..."
        )
        ten_env.log_info(
            f"[DingTalkBotExtension] Tool: {json.dumps(tool_metadata_for_registration, indent=2)}"
        )

        # 直接发送 tool_register 命令
        cmd = Cmd.create("tool_register")
        cmd.set_property_from_json(
            "tool", json.dumps(tool_metadata_for_registration)
        )

        result = await ten_env.send_cmd(cmd)
        ten_env.log_info(
            f"[DingTalkBotExtension] Tool registration result: {result}"
        )

        ten_env.log_info(
            "[DingTalkBotExtension] ========== on_start END =========="
        )

    async def on_stop(self, ten_env: AsyncTenEnv) -> None:
        """Stop the extension"""
        ten_env.log_info("[DingTalkBotExtension] on_stop")

    async def on_deinit(self, ten_env: AsyncTenEnv) -> None:
        """Deinitialize the extension"""
        ten_env.log_info("[DingTalkBotExtension] on_deinit")

    async def on_cmd(self, ten_env: AsyncTenEnv, cmd: Cmd) -> None:
        """Handle incoming commands"""
        cmd_name = cmd.get_name()
        ten_env.log_info(
            "[DingTalkBotExtension] ========== on_cmd RECEIVED =========="
        )
        ten_env.log_info(f"[DingTalkBotExtension] Command name: {cmd_name}")

        if cmd_name == "tool_call":
            try:
                tool_name = cmd.get_property_string("name")
                ten_env.log_info(
                    f"[DingTalkBotExtension] tool_call - name: {tool_name}"
                )

                # arguments 现在是 object 类型，直接获取 JSON
                args_json, err = cmd.get_property_to_json("arguments")
                if err:
                    ten_env.log_error(
                        f"[DingTalkBotExtension] Failed to get arguments: {err}"
                    )
                else:
                    ten_env.log_info(
                        f"[DingTalkBotExtension] tool_call - arguments (object): {args_json}"
                    )
                    args = json.loads(args_json)
                    ten_env.log_info(
                        f"[DingTalkBotExtension] Parsed arguments: {args}"
                    )

                    # 直接调用 run_tool
                    if tool_name == TOOL_NAME:
                        result = await self.run_tool(ten_env, tool_name, args)
                        ten_env.log_info(
                            f"[DingTalkBotExtension] Tool execution result: {result}"
                        )

                        # 返回结果
                        cmd_result = Cmd.create("tool_call_result")
                        if result:
                            cmd_result.set_property_string(
                                "content", result.content
                            )
                        await ten_env.return_result(cmd_result, cmd)
                        ten_env.log_info(
                            "[DingTalkBotExtension] ========== on_cmd END =========="
                        )
                        return

            except Exception as e:
                ten_env.log_error(
                    f"[DingTalkBotExtension] Error processing tool_call: {e}"
                )
                import traceback

                ten_env.log_error(
                    f"[DingTalkBotExtension] Traceback: {traceback.format_exc()}"
                )

        # 对于其他命令，调用基类处理
        ten_env.log_info("[DingTalkBotExtension] Calling parent on_cmd...")
        await super().on_cmd(ten_env, cmd)
        ten_env.log_info(
            "[DingTalkBotExtension] ========== on_cmd END =========="
        )

    def get_tool_metadata(self, ten_env: AsyncTenEnv) -> list[LLMToolMetadata]:
        """Return tool metadata for LLM"""
        return [
            LLMToolMetadata(
                name=TOOL_NAME,
                description=TOOL_DESCRIPTION,
                parameters=[
                    LLMToolMetadataParameter(
                        name="content",
                        type="string",
                        description="The message content to send to DingTalk group",
                        required=True,
                    )
                ],
            )
        ]

    async def run_tool(
        self, ten_env: AsyncTenEnv, name: str, args: dict
    ) -> LLMToolResult | None:
        """Execute the tool"""
        ten_env.log_info(
            "[DingTalkBotExtension] ========== run_tool CALLED =========="
        )
        ten_env.log_info(f"[DingTalkBotExtension] Tool name: {name}")
        ten_env.log_info(
            f"[DingTalkBotExtension] Tool args: {json.dumps(args, indent=2)}"
        )

        if name == TOOL_NAME:
            content = args.get("content")
            if not content:
                ten_env.log_error(
                    "[DingTalkBotExtension] Missing 'content' argument"
                )
                return LLMToolResultLLMResult(
                    type="llmresult",
                    content="Error: 'content' argument is missing",
                )

            ten_env.log_info(
                "[DingTalkBotExtension] Preparing to send message to DingTalk..."
            )
            ten_env.log_info(
                f"[DingTalkBotExtension] Message content: {content}"
            )
            ten_env.log_info(
                f"[DingTalkBotExtension] Using access_token: {self.config.access_token[:20] + '...' if len(self.config.access_token) > 20 else self.config.access_token}"
            )

            result = self._send_dingtalk_message(
                self.config.access_token, self.config.secret, content
            )
            ten_env.log_info(
                f"[DingTalkBotExtension] DingTalk API response: {json.dumps(result, indent=2)}"
            )

            if result.get("errcode") == 0:
                success_msg = (
                    f"Message sent successfully to DingTalk: {content}"
                )
                ten_env.log_info(
                    f"[DingTalkBotExtension] SUCCESS: {success_msg}"
                )
                return LLMToolResultLLMResult(
                    type="llmresult",
                    content=success_msg,
                )
            else:
                error_msg = f"Failed to send message. Error code: {result.get('errcode')}, Error message: {result.get('errmsg', 'Unknown error')}"
                ten_env.log_error(f"[DingTalkBotExtension] FAILED: {error_msg}")
                return LLMToolResultLLMResult(
                    type="llmresult",
                    content=error_msg,
                )
        else:
            ten_env.log_warn(
                f"[DingTalkBotExtension] Unknown tool name: {name}, expected: {TOOL_NAME}"
            )

        ten_env.log_info(
            "[DingTalkBotExtension] ========== run_tool END =========="
        )
        return None

    def _send_dingtalk_message(
        self, access_token: str, secret: str, content: str
    ) -> dict:
        """
        Sends a message to a DingTalk bot.
        """
        print("[DingTalkBotExtension._send_dingtalk_message] Starting...")
        print(
            f"[DingTalkBotExtension._send_dingtalk_message] access_token length: {len(access_token)}"
        )
        print(
            f"[DingTalkBotExtension._send_dingtalk_message] secret length: {len(secret) if secret else 0}"
        )
        print(
            f"[DingTalkBotExtension._send_dingtalk_message] content: {content}"
        )

        webhook_url = (
            f"https://oapi.dingtalk.com/robot/send?access_token={access_token}"
        )
        print(
            f"[DingTalkBotExtension._send_dingtalk_message] Base webhook_url: {webhook_url[:80]}..."
        )

        if secret:
            timestamp = str(round(time.time() * 1000))
            secret_enc = secret.encode("utf-8")
            string_to_sign = "{}\n{}".format(timestamp, secret)
            string_to_sign_enc = string_to_sign.encode("utf-8")
            hmac_code = hmac.new(
                secret_enc, string_to_sign_enc, digestmod=hashlib.sha256
            ).digest()
            sign = urllib.parse.quote_plus(base64.b64encode(hmac_code))
            webhook_url = f"{webhook_url}&timestamp={timestamp}&sign={sign}"
            print(
                f"[DingTalkBotExtension._send_dingtalk_message] Added signature, timestamp: {timestamp}"
            )

        headers = {"Content-Type": "application/json;charset=utf-8"}
        data = {"msgtype": "text", "text": {"content": content}}

        print(
            f"[DingTalkBotExtension._send_dingtalk_message] Request data: {json.dumps(data, indent=2)}"
        )

        try:
            print(
                "[DingTalkBotExtension._send_dingtalk_message] Sending POST request..."
            )
            response = requests.post(
                webhook_url, headers=headers, data=json.dumps(data), timeout=10
            )
            print(
                f"[DingTalkBotExtension._send_dingtalk_message] Response status code: {response.status_code}"
            )
            print(
                f"[DingTalkBotExtension._send_dingtalk_message] Response text: {response.text}"
            )
            response.raise_for_status()
            result = response.json()
            print(
                f"[DingTalkBotExtension._send_dingtalk_message] Response JSON: {json.dumps(result, indent=2)}"
            )
            return result
        except requests.exceptions.RequestException as e:
            print(
                f"[DingTalkBotExtension._send_dingtalk_message] Exception occurred: {type(e).__name__}: {str(e)}"
            )
            return {"errcode": -1, "errmsg": str(e)}
