from pydantic import BaseModel


class MainControlConfig(BaseModel):
    greeting: str = "Hello, I am your AI assistant."
    agent_user_id: str = (
        ""  # RTM user ID of the agent, used to filter out self-messages
    )
