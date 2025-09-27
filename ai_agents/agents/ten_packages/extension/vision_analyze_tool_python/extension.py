#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
import json
import uuid
from ten_ai_base.struct import (
    EventType,
    LLMMessage,
    LLMMessageContent,
    LLMRequest,
    parse_llm_response,
)
from ten_runtime import (
    AudioFrame,
    StatusCode,
    VideoFrame,
    AsyncTenEnv,
    Cmd,
    Data,
)
from PIL import Image
from io import BytesIO
from base64 import b64encode

from ten_ai_base.types import (
    LLMToolMetadata,
    LLMToolMetadataParameter,
    LLMToolResult,
    LLMToolResultLLMResult,
)
from ten_ai_base.llm_tool import AsyncLLMToolBaseExtension


def rgb2base64jpeg(rgb_data, width, height):
    """
    Convert RGB/RGBA image data to base64 JPEG format.
    Automatically detects the image format based on data length.
    """
    if not rgb_data or width <= 0 or height <= 0:
        raise ValueError("Invalid image data or dimensions")

    # Convert to bytes if not already
    if not isinstance(rgb_data, bytes):
        rgb_data = bytes(rgb_data)

    expected_pixels = width * height
    data_length = len(rgb_data)

    # Determine image format based on data length
    if data_length == expected_pixels * 4:
        # RGBA format (4 bytes per pixel)
        mode = "RGBA"
    elif data_length == expected_pixels * 3:
        # RGB format (3 bytes per pixel)
        mode = "RGB"
    elif data_length == expected_pixels:
        # Grayscale format (1 byte per pixel)
        mode = "L"
    else:
        raise ValueError(
            f"Unsupported image format. Expected {expected_pixels * 4} (RGBA), {expected_pixels * 3} (RGB), or {expected_pixels} (L) bytes, got {data_length}"
        )

    try:
        # Convert the image data to a PIL Image
        pil_image = Image.frombytes(mode, (width, height), rgb_data)

        # Convert to RGB if needed (JPEG doesn't support RGBA or L directly)
        if mode in ["RGBA", "L"]:
            pil_image = pil_image.convert("RGB")

        # Resize the image while maintaining its aspect ratio
        pil_image = resize_image_keep_aspect(pil_image, 512)

        # Save the image to a BytesIO object in JPEG format
        buffered = BytesIO()
        pil_image.save(buffered, format="JPEG", quality=85)

        # Get the byte data of the JPEG image
        jpeg_image_data = buffered.getvalue()

        # Convert the JPEG byte data to a Base64 encoded string
        base64_encoded_image = b64encode(jpeg_image_data).decode("utf-8")

        # Create the data URL
        mime_type = "image/jpeg"
        base64_url = f"data:{mime_type};base64,{base64_encoded_image}"
        return base64_url

    except Exception as e:
        raise ValueError(f"Failed to process image data: {str(e)}") from e


def resize_image_keep_aspect(image, max_size=512):
    """
    Resize an image while maintaining its aspect ratio, ensuring the larger dimension is max_size.
    If both dimensions are smaller than max_size, the image is not resized.

    :param image: A PIL Image object
    :param max_size: The maximum size for the larger dimension (width or height)
    :return: A PIL Image object (resized or original)
    """
    # Get current width and height
    width, height = image.size

    # If both dimensions are already smaller than max_size, return the original image
    if width <= max_size and height <= max_size:
        return image

    # Calculate the aspect ratio
    aspect_ratio = width / height

    # Determine the new dimensions
    if width > height:
        new_width = max_size
        new_height = int(max_size / aspect_ratio)
    else:
        new_height = max_size
        new_width = int(max_size * aspect_ratio)

    # Resize the image with the new dimensions
    resized_image = image.resize((new_width, new_height))

    return resized_image


class VisionAnalyzeToolExtension(AsyncLLMToolBaseExtension):
    image_data = None
    image_width = 0
    image_height = 0

    async def on_init(self, ten_env: AsyncTenEnv) -> None:
        ten_env.log_debug("on_init")

    async def on_start(self, ten_env: AsyncTenEnv) -> None:
        ten_env.log_debug("on_start")
        await super().on_start(ten_env)

    async def on_stop(self, ten_env: AsyncTenEnv) -> None:
        ten_env.log_debug("on_stop")

        # TODO: clean up resources

    async def on_deinit(self, ten_env: AsyncTenEnv) -> None:
        ten_env.log_debug("on_deinit")

    async def on_cmd(self, ten_env: AsyncTenEnv, cmd: Cmd) -> None:
        cmd_name = cmd.get_name()
        ten_env.log_debug("on_cmd name {}".format(cmd_name))

        await super().on_cmd(ten_env, cmd)

    async def on_data(self, ten_env: AsyncTenEnv, data: Data) -> None:
        data_name = data.get_name()
        ten_env.log_debug("on_data name {}".format(data_name))

    async def on_audio_frame(
        self, ten_env: AsyncTenEnv, audio_frame: AudioFrame
    ) -> None:
        audio_frame_name = audio_frame.get_name()
        ten_env.log_debug("on_audio_frame name {}".format(audio_frame_name))

    async def on_video_frame(
        self, ten_env: AsyncTenEnv, video_frame: VideoFrame
    ) -> None:
        video_frame_name = video_frame.get_name()
        ten_env.log_debug("on_video_frame name {}".format(video_frame_name))

        self.image_data = video_frame.get_buf()
        self.image_width = video_frame.get_width()
        self.image_height = video_frame.get_height()

    def get_tool_metadata(self, ten_env: AsyncTenEnv) -> list[LLMToolMetadata]:
        return [
            LLMToolMetadata(
                name="get_vision_chat_completion",
                description="Get the image analyze result from camera. Call this whenever you need to understand the input camera image like you have vision capability, for example when user asks 'What can you see in my camera?' or 'Can you see me?'",
                parameters=[
                    LLMToolMetadataParameter(
                        name="query",
                        type="string",
                        description="The vision completion query.",
                        required=True,
                    ),
                ],
            ),
        ]

    async def run_tool(
        self, ten_env: AsyncTenEnv, name: str, args: dict
    ) -> LLMToolResult | None:
        if name == "get_vision_chat_completion":
            if self.image_data is None:
                ten_env.log_error("No image data available")
                raise ValueError("No image data available")

            if "query" not in args:
                ten_env.log_error("Missing query parameter")
                raise ValueError("Failed to get property")

            query = args["query"]
            ten_env.log_info(f"Processing vision query: {query}")
            ten_env.log_info(
                f"Image dimensions: {self.image_width}x{self.image_height}, data length: {len(self.image_data) if self.image_data else 0}"
            )

            try:
                base64_image = rgb2base64jpeg(
                    self.image_data, self.image_width, self.image_height
                )
                ten_env.log_info("Successfully converted image to base64")
            except Exception as e:
                ten_env.log_error(
                    f"Failed to convert image to base64: {str(e)}"
                )
                raise ValueError(f"Image processing failed: {str(e)}") from e
            # return LLMToolResult(message=LLMCompletionArgsMessage(role="user", content=[result]))
            # cmd: Cmd = Cmd.create(CMD_CHAT_COMPLETION_CALL)
            # message: LLMChatCompletionUserMessageParam = (
            #     LLMChatCompletionUserMessageParam(
            #         role="user",
            #         content=[
            #             {"type": "text", "text": query},
            #             {
            #                 "type": "image_url",
            #                 "image_url": {"url": base64_image},
            #             },
            #         ],
            #     )
            # )
            # cmd.set_property_from_json(
            #     "arguments", json.dumps({"messages": [message]})
            # )
            # ten_env.log_info("send_cmd {}".format(message))
            # [cmd_result, _] = await ten_env.send_cmd(cmd)
            # result, _ = cmd_result.get_property_to_json("response")

            request_id = str(uuid.uuid4())
            messages: list[LLMMessage] = []
            messages.append(
                LLMMessageContent(
                    role="user",
                    content=[
                        {"type": "text", "text": query},
                        {
                            "type": "image_url",
                            "image_url": {"url": base64_image},
                        },
                    ],
                )
            )
            llm_input = LLMRequest(
                request_id=request_id,
                messages=messages,
                model="gpt-4o",
                streaming=True,
                parameters={"temperature": 0.7},
                tools=[],
            )
            input_json = llm_input.model_dump()
            cmd = Cmd.create("chat_completion")
            cmd.set_property_from_json(None, json.dumps(input_json))
            response = ten_env.send_cmd_ex(cmd)

            # response = _send_cmd_ex(ten_env, "chat_completion", "llm", input_json)

            result = ""

            async for cmd_result, _ in response:
                if cmd_result and cmd_result.is_final() is False:
                    if cmd_result.get_status_code() == StatusCode.OK:
                        response_json, _ = cmd_result.get_property_to_json(None)
                        ten_env.log_info(f"tool: response_json {response_json}")
                        completion = parse_llm_response(response_json)
                        if completion.type == EventType.MESSAGE_CONTENT_DONE:
                            result = completion.content
                            break

            return LLMToolResultLLMResult(
                type="llmresult",
                content=json.dumps(result),
            )
