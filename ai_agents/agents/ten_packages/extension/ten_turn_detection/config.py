from pydantic import BaseModel


class TENTurnDetectorConfig(BaseModel):
    base_url: str = "http://localhost:8000/v1"
    api_key: str = "TEN_Turn_Detection"
    model: str = "TEN_Turn_Detection"
    temperature: float = 0.1
    top_p: float = 0.1

    force_threshold_ms: int = 5000  # <=0 means disable

    def force_chat_enabled(self) -> bool:
        return self.force_threshold_ms > 0

    def to_json(self) -> str:
        return self.model_dump_json()
