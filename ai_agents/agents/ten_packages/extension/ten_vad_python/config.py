from pydantic import BaseModel


class TENVADConfig(BaseModel):
    prefix_padding_ms: int = 120
    silence_duration_ms: int = 1000
    vad_threshold: float = 0.5
    hop_size_ms: int = 16
    dump: bool = False
    dump_path: str = ""
