import json
import time
from ten_runtime import AsyncTenEnv, Cmd, Data


async def _send_cmd(ten_env: AsyncTenEnv, name: str, dest: str, params: dict = None):
    """Send a command to an extension"""
    cmd = Cmd.create(name)
    if params:
        for key, value in params.items():
            cmd.set_property_string(key, str(value))
    await ten_env.send_cmd(cmd, dest)


async def _send_data(
    ten_env: AsyncTenEnv,
    name: str,
    dest: str,
    data: dict,
    src: str = None,
    src_group: str = None,
):
    """Send data to an extension"""
    data_obj = Data.create(name)
    data_obj.set_json(json.dumps(data))
    await ten_env.send_data(data_obj, dest, src, src_group)


def parse_sentences(fragment: str, text: str):
    """Parse sentences from text, maintaining any fragment"""
    if not text:
        return [], fragment

    # Simple sentence parsing - split on common sentence endings
    # This is a simplified version; a more robust implementation might use NLP
    sentences = []
    current = fragment + text

    # Split on common sentence endings
    import re

    # Look for sentence-ending punctuation
    parts = re.split(r"(?<=[.!?])\s+", current)

    for i, part in enumerate(parts[:-1]):
        sentences.append(part)

    fragment = parts[-1] if parts else ""

    return sentences, fragment