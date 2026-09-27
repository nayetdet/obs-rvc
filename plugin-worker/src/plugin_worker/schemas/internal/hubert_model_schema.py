from threading import Lock
from typing import Any

from pydantic import BaseModel, Field


class HubertModelSchema(BaseModel):
    model: Any
    lock: Any = Field(default_factory=Lock)
