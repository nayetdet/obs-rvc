from typing import ClassVar

from pydantic import Field
from pydantic_settings import BaseSettings, SettingsConfigDict


class Settings(BaseSettings):
    model_config = SettingsConfigDict(
        env_prefix="RVC_",
        env_file=".env",
        env_file_encoding="utf-8",
        extra="ignore",
        validate_assignment=True,
    )

    max_model_bytes: ClassVar[int] = 128
    max_index_path_bytes: ClassVar[int] = 512
    max_error_bytes: ClassVar[int] = 1024
    max_audio_bytes: ClassVar[int] = 4 * 1024 * 1024
    max_output_bytes: ClassVar[int] = 8 * 1024 * 1024

    service_name: str = "obs/rvc"
    wait_ms: int = Field(default=10, ge=1, le=1000)


settings: Settings = Settings()
