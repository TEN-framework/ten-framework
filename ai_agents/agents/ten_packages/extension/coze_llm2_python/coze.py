# ------------------------------
# Config (parity with your pattern)
# ------------------------------
from dataclasses import dataclass
import json
import traceback
from typing import Any, AsyncGenerator, List, Optional
import aiohttp
from cozepy import Chat, ChatEvent, ChatEventType, Message
from pydantic import BaseModel

from ten_ai_base.struct import (
    LLMMessageContent,
    LLMRequest,
    LLMResponse,
    LLMResponseMessageDelta,
    LLMResponseMessageDone,
)
from ten_runtime import AsyncTenEnv


@dataclass
class CozeLLM2Config(BaseModel):
    base_url: str = "https://api.acoze.com"
    bot_id: str = ""
    token: str = ""
    user_id: str = "TenAgent"
    connect_timeout_s: float = 15.0
    total_timeout_s: float = 90.0
    auto_save_history: bool = True


# ------------------------------
# Streaming client
# ------------------------------
class CozeChatClient:
    def __init__(self, ten_env: AsyncTenEnv, config: CozeLLM2Config):
        self.ten_env = ten_env
        self.config = config
        self._session: Optional[aiohttp.ClientSession] = None

    async def _ensure_session(self):
        if self._session is None or self._session.closed:
            timeout = aiohttp.ClientTimeout(
                connect=self.config.connect_timeout_s,
                total=self.config.total_timeout_s,
            )
            self._session = aiohttp.ClientSession(timeout=timeout)

    async def aclose(self):
        if self._session and not self._session.closed:
            await self._session.close()
        self._session = None

    def _headers(self):
        return {"Authorization": f"Bearer {self.config.token}"}

    def _url(self, path: str) -> str:
        return f"{self.config.base_url.rstrip('/')}/{path.lstrip('/')}"

    def _build_additional_messages(self, req: LLMRequest) -> List[dict]:
        """Map LLMRequest.messages -> Coze 'additional_messages'."""
        additionals: List[dict] = []
        for m in req.messages or []:
            if not isinstance(m, LLMMessageContent):
                continue
            role = m.role
            content = m.content
            # We only map simple text (parity with your old Coze code)
            if isinstance(content, str):
                if role == "user":
                    additionals.append(
                        Message.build_user_question_text(content).model_dump()
                    )
                elif role == "assistant":
                    additionals.append(
                        Message.build_assistant_answer(content).model_dump()
                    )
            elif isinstance(content, list):
                # If multi-part, concatenate text chunks as a simple fallback
                text_parts = [
                    getattr(x, "text", "")
                    for x in content
                    if hasattr(x, "text")
                ]
                text = "\n".join([t for t in text_parts if t])
                if not text:
                    continue
                if role == "user":
                    additionals.append(
                        Message.build_user_question_text(text).model_dump()
                    )
                elif role == "assistant":
                    additionals.append(
                        Message.build_assistant_answer(text).model_dump()
                    )
        return additionals

    def _event_to_chatevent(self, event: str, event_data: Any) -> ChatEvent:
        """Translate SSE event/data to ChatEvent via cozepy models."""
        if event == ChatEventType.DONE:
            # upstream will stop on 'done'
            raise StopAsyncIteration

        if event == ChatEventType.ERROR:
            raise RuntimeError(f"[Coze] error event: {event_data}")

        if event in [
            ChatEventType.CONVERSATION_MESSAGE_DELTA,
            ChatEventType.CONVERSATION_MESSAGE_COMPLETED,
        ]:
            return ChatEvent(
                event=event, message=Message.model_validate_json(event_data)
            )

        if event in [
            ChatEventType.CONVERSATION_CHAT_CREATED,
            ChatEventType.CONVERSATION_CHAT_IN_PROGRESS,
            ChatEventType.CONVERSATION_CHAT_COMPLETED,
            ChatEventType.CONVERSATION_CHAT_FAILED,
            ChatEventType.CONVERSATION_CHAT_REQUIRES_ACTION,
        ]:
            return ChatEvent(
                event=event, chat=Chat.model_validate_json(event_data)
            )

        # Unknown event
        raise ValueError(f"[Coze] invalid chat.event: {event}, {event_data}")

    async def get_chat_completions(
        self, req: LLMRequest
    ) -> AsyncGenerator[LLMResponse, None]:
        """
        Map LLMRequest -> Coze /v3/chat streaming.
        Emits LLMResponseMessageDelta and LLMResponseMessageDone (no tool calls).
        """
        await self._ensure_session()
        assert self._session is not None

        additional_messages = self._build_additional_messages(req)
        payload = {
            "bot_id": self.config.bot_id,
            "user_id": self.config.user_id,
            "additional_messages": additional_messages,
            "stream": True,
            "auto_save_history": self.config.auto_save_history,
        }

        url = self._url("v3/chat")
        self.ten_env.log_info(f"[Coze] POST {url} payload={payload}")

        full_content = ""
        event = ""

        async with self._session.post(
            url, json=payload, headers=self._headers()
        ) as resp:
            if resp.status != 200:
                try:
                    err = await resp.json()
                except Exception:
                    err = {"status": resp.status, "text": await resp.text()}
                raise RuntimeError(f"[Coze] chat failed: {err}")

            async for raw in resp.content:
                if not raw:
                    continue
                decoded = raw.decode("utf-8").strip()
                if not decoded:
                    continue

                try:
                    if decoded.startswith("event:"):
                        event = decoded[6:].strip()
                        self.ten_env.log_debug(f"[Coze] event: {event}")
                        if event == "done":
                            break
                        continue

                    if decoded.startswith("data:"):
                        data_str = decoded[5:].strip()
                        # Coze returns JSON in data line; feed to model parser
                        chat_event = self._event_to_chatevent(
                            event=event, event_data=data_str
                        )

                        if (
                            chat_event.event
                            == ChatEventType.CONVERSATION_MESSAGE_DELTA
                        ):
                            delta = chat_event.message.content or ""
                            if not delta:
                                continue
                            full_content += delta
                            yield LLMResponseMessageDelta(
                                response_id=str(
                                    getattr(chat_event.message, "id", "") or ""
                                ),
                                role="assistant",
                                content=full_content,
                                delta=delta,
                                created=0,
                            )

                        elif (
                            chat_event.event
                            == ChatEventType.CONVERSATION_MESSAGE_COMPLETED
                        ):
                            # No-op here; we emit DONE after stream ends.
                            pass

                        elif (
                            chat_event.event
                            == ChatEventType.CONVERSATION_CHAT_FAILED
                        ):
                            last_error = chat_event.chat.last_error
                            if (
                                last_error
                                and getattr(last_error, "code", None) == 4011
                            ):
                                raise RuntimeError(
                                    "The Coze token has been depleted. Please check your token usage."
                                )
                            msg = getattr(
                                last_error, "msg", "Unknown Coze chat failure"
                            )
                            raise RuntimeError(msg)

                        # Other chat lifecycle events are informational.
                        continue

                    # Non-SSE JSON (error envelope)
                    obj = json.loads(decoded)
                    code = obj.get("code", 0)
                    if code == 4000:
                        raise RuntimeError("Coze bot is not published.")
                    raise RuntimeError(f"[Coze] stream error envelope: {obj}")

                except StopAsyncIteration:
                    break
                except Exception as e:
                    # Escalate to caller; upper layer should handle & notify
                    self.ten_env.log_error(
                        f"[Coze] error processing event: {traceback.format_exc()}"
                    )
                    raise e

        # Emit terminal message
        yield LLMResponseMessageDone(
            response_id="",
            role="assistant",
            content=full_content,
            created=0,
        )
