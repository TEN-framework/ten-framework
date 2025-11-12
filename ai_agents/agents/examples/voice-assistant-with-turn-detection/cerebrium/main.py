from vllm import LLM, SamplingParams
from transformers import AutoTokenizer
from pydantic import BaseModel
from typing import List, Any, Optional
import time

# The Hugging Face ID for the Turn Detection model
MODEL_ID = "TEN-framework/TEN_Turn_Detection"

# --- Model Initialization (Runs once at cold start) ---

# Initialize the tokenizer for chat template formatting
tokenizer = AutoTokenizer.from_pretrained(MODEL_ID, trust_remote_code=True)

# Initialize the vLLM model globally.
# Note: trust_remote_code=True is necessary for this custom model architecture.
llm = LLM(
    model=MODEL_ID,
    trust_remote_code=True,
    dtype="auto",
    gpu_memory_utilization=0.9,
)

# --- Pydantic Models for OpenAI Compatibility ---


class Message(BaseModel):
    role: str
    content: str


class ChatCompletionResponse(BaseModel):
    id: str
    object: str
    created: int
    model: str
    choices: List[Any]
    usage: Optional[dict] = None


# --- OpenAI-Compatible Endpoint ---


def run(
    messages: list,
    model: str = MODEL_ID,
    run_id: str = None,
    temperature: float = 0.1,
    top_p: float = 0.1,
    max_tokens: int = 1,
    stream: bool = False,
    **kwargs,
) -> dict:
    """
    OpenAI-compatible Turn Detection endpoint.
    Works directly with OpenAI Python client.

    Args:
        messages: List of message dicts with 'role' and 'content'
        model: Model identifier
        run_id: Unique request ID (auto-provided by Cerebrium)
        temperature: Sampling temperature
        top_p: Top-p sampling
        max_tokens: Max tokens (always 1 for classification)
        stream: Streaming mode (not supported)
        **kwargs: Additional OpenAI parameters (ignored)

    Returns:
        OpenAI-compatible chat completion response
    """

    # Extract system prompt and user content
    system_prompt = ""
    user_content = ""

    for msg in messages:
        message = Message(**msg)
        if message.role == "system":
            system_prompt = message.content
        elif message.role == "user":
            user_content = message.content

    if not user_content:
        return {
            "error": {
                "message": "At least one user message is required",
                "type": "invalid_request_error",
            }
        }

    # Format using chat template
    formatted_messages = [
        {"role": "system", "content": system_prompt},
        {"role": "user", "content": user_content},
    ]

    formatted_prompt = tokenizer.apply_chat_template(
        formatted_messages, add_generation_prompt=True, tokenize=False
    )

    # Configure sampling for single-token classification
    sampling_params = SamplingParams(
        temperature=temperature,
        top_p=top_p,
        max_tokens=1,  # Single token: finished/unfinished/wait
    )

    # Generate output
    outputs = llm.generate([formatted_prompt], sampling_params)
    turn_state = outputs[0].outputs[0].text.strip()

    # Build OpenAI-compatible response
    response = ChatCompletionResponse(
        id=run_id or f"chatcmpl-{int(time.time())}",
        object="chat.completion",
        created=int(time.time()),
        model=model,
        choices=[
            {
                "index": 0,
                "message": {"role": "assistant", "content": turn_state},
                "finish_reason": "stop",
            }
        ],
        usage={
            "prompt_tokens": len(user_content.split()),
            "completion_tokens": 1,
            "total_tokens": len(user_content.split()) + 1,
        },
    )

    return response.model_dump()
