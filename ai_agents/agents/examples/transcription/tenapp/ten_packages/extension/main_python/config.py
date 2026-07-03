from pydantic import BaseModel


class MainControlConfig(BaseModel):
    enable_llm_correction: bool = True
    correction_prompt: str = (
        "You are a transcription proofreader. Improve grammar and fix recognition "
        "mistakes, but keep the speaker's intent unchanged. Return only the corrected "
        "sentence without extra commentary.\n\nTranscript:\n{text}"
    )
