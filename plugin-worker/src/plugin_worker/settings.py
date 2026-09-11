from pydantic_settings import BaseSettings, SettingsConfigDict


class Settings(BaseSettings):
    model_config = SettingsConfigDict(extra="ignore")

    max_model_bytes: int = 128
    max_index_path_bytes: int = 512
    max_error_bytes: int = 1024
    max_audio_bytes: int = 4 * 1024 * 1024
    max_output_bytes: int = 8 * 1024 * 1024
    service_name: str = "obs/rvc"
    wait_ms: int = 10


settings: Settings = Settings()
