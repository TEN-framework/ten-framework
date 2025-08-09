#
#
# Agora Real Time Engagement
# Created by Wei Hu in 2024-08.
# Copyright (c) 2024 Agora IO. All rights reserved.
#
#

import base64
from ten_runtime import AsyncTenEnv

MAX_CHUNK_SIZE_BYTES = 1024

def _text_to_base64_chunks(_: AsyncTenEnv, text: str, msg_id: str) -> list[str]:
    # Ensure msg_id does not exceed 50 characters
    if len(msg_id) > 36:
        raise ValueError("msg_id cannot exceed 36 characters.")

    # Convert text to bytearray
    byte_array = bytearray(text, "utf-8")

    # Encode the bytearray into base64
    base64_encoded = base64.b64encode(byte_array).decode("utf-8")

    # Initialize list to hold the final chunks
    chunks = []

    # We'll split the base64 string dynamically based on the final byte size
    part_index = 0
    total_parts = None  # We'll calculate total parts once we know how many chunks we create

    # Process the base64-encoded content in chunks
    current_position = 0
    total_length = len(base64_encoded)

    while current_position < total_length:
        part_index += 1

        # Start guessing the chunk size by limiting the base64 content part
        estimated_chunk_size = (
            MAX_CHUNK_SIZE_BYTES  # We'll reduce this dynamically
        )
        content_chunk = ""
        count = 0
        while True:
            # Create the content part of the chunk
            content_chunk = base64_encoded[
                current_position : current_position + estimated_chunk_size
            ]

            # Format the chunk
            formatted_chunk = f"{msg_id}|{part_index}|{total_parts if total_parts else '???'}|{content_chunk}"

            # Check if the byte length of the formatted chunk exceeds the max allowed size
            if len(bytearray(formatted_chunk, "utf-8")) <= MAX_CHUNK_SIZE_BYTES:
                break
            else:
                # Reduce the estimated chunk size if the formatted chunk is too large
                estimated_chunk_size -= 100  # Reduce content size gradually
                count += 1

        # ten_env.log_debug(f"chunk estimate guess: {count}")

        # Add the current chunk to the list
        chunks.append(formatted_chunk)
        # Move to the next part of the content
        current_position += estimated_chunk_size

    # Now that we know the total number of parts, update the chunks with correct total_parts
    total_parts = len(chunks)
    updated_chunks = [
        chunk.replace("???", str(total_parts)) for chunk in chunks
    ]

    return updated_chunks
