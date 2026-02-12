from pydantic import BaseModel


class MainControlConfig(BaseModel):
    greeting: str = "Hello, I am your AI assistant."
    interrupt_on_interim: bool = True  # If False, only interrupt on final STT result
