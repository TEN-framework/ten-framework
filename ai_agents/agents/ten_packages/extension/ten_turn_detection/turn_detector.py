from .config import TENTurnDetectorConfig
from .utils import remove_punctuation, TimeHelper

from ten_runtime import AsyncTenEnv

from openai import AsyncOpenAI
import time
from typing import List, Any, Callable
import asyncio
import httpx
from enum import Enum


class SpecialToken(str, Enum):
    Unfinished = "unfinished"
    Finished = "finished"
    Wait = "wait"


class TurnDetectorDecision(str, Enum):
    Unfinished = "unfinished"
    Finished = "finished"
    Wait = "wait"


class TurnDetector:
    def __init__(
        self,
        config: TENTurnDetectorConfig,
        ten_env: AsyncTenEnv,
        pre_chat_hook: Callable[[List[Any]], None] = None,
        post_chat_hook: Callable[[str], None] = None,
        chat_usage_hook: Callable[[int, int], None] = None,
    ) -> None:
        # static vars
        self.config = config
        self.ten_env = ten_env  # for logging only

        self.pre_chat_hook = pre_chat_hook
        self.post_chat_hook = post_chat_hook
        self.chat_usage_hook = chat_usage_hook

        # Create managed httpx client with optimized connection pool
        self.http_client = httpx.AsyncClient(
            timeout=httpx.Timeout(timeout=5.0),
            limits=httpx.Limits(
                max_connections=100,
                max_keepalive_connections=20,
                keepalive_expiry=600.0,  # 10 minutes keepalive
            ),
            http2=True,  # Enable HTTP/2 if server supports it
            follow_redirects=True,
        )

        # Create OpenAI client with our managed http client
        self.client_session = AsyncOpenAI(
            api_key=self.config.api_key,
            base_url=self.config.base_url,
            http_client=self.http_client,  # Use our managed client
        )
        ten_env.log_info(
            "openai client session initialized with managed http client"
        )

        # running vars
        self.chat_task: asyncio.Task = None

    async def stop(self) -> None:
        """Stop and cleanup resources."""
        if self.chat_task:
            self.chat_task.cancel()
            try:
                await self.chat_task
            except asyncio.CancelledError:
                pass
            self.chat_task = None

        if self.http_client:
            await self.http_client.aclose()
            self.ten_env.log_info("http client closed")

    async def eval(self, text: str) -> TurnDetectorDecision:
        # prepare messages
        no_punc_text = remove_punctuation(text)
        messages = [{"role": "user", "content": no_punc_text}]

        if self.pre_chat_hook:
            self.pre_chat_hook(messages)

        # create cancellable task
        task = asyncio.create_task(
            self._openai_chat_completion(messages=messages)
        )
        self.chat_task = task
        self.ten_env.log_debug(
            f"eval task {task.get_name()} messages: {messages}"
        )

        decision = TurnDetectorDecision.Unfinished  # default to listen
        try:
            content = await asyncio.wait_for(task, timeout=5.0)

            if not content:
                return decision

            if self.post_chat_hook:
                self.post_chat_hook(content)

            messages.append(
                {"role": "assistant", "content": content}
            )  # print only
            self.ten_env.log_debug(
                f"eval task {task.get_name()}, assistant content: {content}, memory: {messages}"
            )

            # output decision
            if content.startswith(SpecialToken.Unfinished):
                pass
            elif content.startswith(SpecialToken.Wait):
                decision = TurnDetectorDecision.Wait
            else:
                # decided to chat
                decision = TurnDetectorDecision.Finished

            self.ten_env.log_debug(
                f"eval task {task.get_name()} decision made: {decision}"
            )
            return decision

        except asyncio.TimeoutError:
            self.ten_env.log_warn(f"eval task {task.get_name()} was timeout")
            return decision
        except asyncio.CancelledError:
            self.ten_env.log_warn(f"eval task {task.get_name()} was cancelled")
            return decision
        except Exception as e:
            self.ten_env.log_warn(f"eval task {task.get_name()} error {e}")
            return decision
        finally:
            self.chat_task = None

    def cancel_eval(self) -> None:
        if not self.chat_task:
            return
        self.chat_task.cancel()
        self.ten_env.log_info(f"cancel eval task {self.chat_task.get_name()}")

    async def _openai_chat_completion(self, messages) -> str:
        start_time = time.time()
        try:
            chat_completion = await self.client_session.chat.completions.create(
                messages=messages,
                model=self.config.model,
                max_tokens=1,
                temperature=self.config.temperature,
                top_p=self.config.top_p,
            )
            self.ten_env.log_debug(f"got result: {chat_completion}")

            # Handle Cerebrium's response wrapper format
            # Cerebrium wraps responses in a 'result' field
            if hasattr(chat_completion, "result") and isinstance(
                chat_completion.result, dict
            ):
                content = chat_completion.result["choices"][0]["message"][
                    "content"
                ]
            else:
                # Standard OpenAI format
                content = chat_completion.choices[0].message.content
            self.ten_env.log_debug(f"got content: {content}")

            ttfb = TimeHelper.duration_ms_since(start_time)
            self.ten_env.log_info(f"KEYPOINT [ttfb:{ttfb}ms], [text:{content}]")

            if self.chat_usage_hook:
                # Handle usage stats for both Cerebrium and standard OpenAI format
                if hasattr(chat_completion, "result") and isinstance(
                    chat_completion.result, dict
                ):
                    usage = chat_completion.result.get("usage", {})
                    self.chat_usage_hook(
                        output_tokens=usage.get("prompt_tokens", 0),
                        input_tokens=usage.get("completion_tokens", 0),
                    )
                else:
                    self.chat_usage_hook(
                        output_tokens=chat_completion.usage.prompt_tokens,
                        input_tokens=chat_completion.usage.completion_tokens,
                    )
            return content
        except Exception as e:
            self.ten_env.log_warn(
                f"eval error {e}, cost time: {TimeHelper.duration_ms_since(start_time)}ms"
            )
            return ""
