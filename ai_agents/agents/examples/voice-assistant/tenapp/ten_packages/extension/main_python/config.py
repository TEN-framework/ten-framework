from pydantic import BaseModel


class MainControlConfig(BaseModel):
    greeting: str = "Hello, I am your AI assistant."
    # Optional extension name that also receives every `tts_text_input`,
    # e.g. an ASR node that uses the agent's replies as conversational
    # context (AssemblyAI `agent_context`). Empty disables the fan-out.
    agent_context_dest: str = ""
