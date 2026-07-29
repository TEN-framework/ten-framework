"""The beta handshake must not come back.

`OpenAI-Beta: realtime=v1` has to be absent on the GA interface, and the
default model has to be a realtime model — `gpt-4o` is not one, so a
deployment that never overrides the property would fail to connect.

`connection.py` imports the native `ten_runtime` module, so these checks read
it as source rather than importing it. That keeps them runnable without a
built runtime, which is the point of confining the wire format to a layer that
depends on nothing.
"""

import ast
from pathlib import Path

CONNECTION_PY = (
    Path(__file__).resolve().parent.parent / "realtime" / "connection.py"
)


def _module() -> ast.Module:
    return ast.parse(CONNECTION_PY.read_text(encoding="utf-8"))


def _constant(name: str) -> str:
    for node in _module().body:
        if isinstance(node, ast.Assign) and any(
            isinstance(t, ast.Name) and t.id == name for t in node.targets
        ):
            return ast.literal_eval(node.value)
    raise AssertionError(f"{name} not found in connection.py")


def test_default_model_is_a_realtime_model() -> None:
    assert _constant("DEFAULT_VIRTUAL_MODEL").startswith("gpt-realtime")


def test_default_model_is_not_a_chat_model() -> None:
    assert _constant("DEFAULT_VIRTUAL_MODEL") != "gpt-4o"


def _string_literals() -> set[str]:
    """Every string constant in the module.

    Comments are absent from the AST, so prose explaining why the beta header
    is gone cannot make these checks pass or fail.
    """
    return {
        node.value
        for node in ast.walk(_module())
        if isinstance(node, ast.Constant) and isinstance(node.value, str)
    }


def test_beta_header_is_not_sent() -> None:
    literals = _string_literals()

    assert "OpenAI-Beta" not in literals
    assert "realtime=v1" not in literals


def test_azure_vendor_still_uses_api_key_header() -> None:
    """The Azure path is untouched by the OpenAI GA migration."""
    assert "api-key" in _string_literals()
