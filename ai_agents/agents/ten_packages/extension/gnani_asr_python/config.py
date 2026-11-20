from dataclasses import dataclass, field


@dataclass(frozen=True)
class PrismaConfig:
    api_key: str = field(repr=False)
    model: str = "prisma-v2.5"
    language: str = "en-IN"
    sample_rate: int = 16000

    def __post_init__(self):
        if not isinstance(self.api_key, str) or not self.api_key.strip():
            raise ValueError("GNANI_API_KEY is required")
        if self.model not in ("prisma-v2.5", "prisma-v3.0"):
            raise ValueError("unsupported Prisma model")
        locales = "bn-IN en-IN gu-IN hi-IN kn-IN ml-IN mr-IN pa-IN ta-IN te-IN".split()
        if self.model == "prisma-v3.0":
            locales.append("indic-auto")
        if self.language not in locales:
            raise ValueError("unsupported Prisma language for selected model")
        if self.sample_rate != 16000:
            raise ValueError("Prisma requires 16000 Hz PCM")

    def headers(self):
        return {
            "x-api-key-id": self.api_key,
            "language_code": self.language,
            "model": self.model,
            "x-sample-rate": "16000",
            "format": "verbatim",
            "x-max-speech-s": "20",
        }
