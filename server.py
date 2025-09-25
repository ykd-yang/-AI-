import os
import json
import logging
from typing import Any, Dict, List

import httpx
from dotenv import load_dotenv
from fastapi import FastAPI, HTTPException
from pydantic import BaseModel, Field, validator

VERSION = "v5-robust-output-parse"
ALLOWED_ROLES = {"user", "assistant"}

load_dotenv()
KEY = os.getenv("OPENAI_API_KEY")
MODEL = os.getenv("OPENAI_MODEL", "gpt-4.1-mini")
SYSTEM_PROMPT = os.getenv(
    "SYSTEM_PROMPT",
    "너는게임속 NPC다. 한국어로간결하고상황에맞게답하라."
)
LOG_LEVEL_NAME = os.getenv("NPC_LOG_LEVEL", os.getenv("LOG_LEVEL", "INFO")).upper()
LOG_FORMAT = os.getenv("NPC_LOG_FORMAT", "[%(asctime)s] [%(levelname)s] %(message)s")

logging.basicConfig(level=getattr(logging, LOG_LEVEL_NAME, logging.INFO), format=LOG_FORMAT)
logger = logging.getLogger("npc_proxy")



def _truncate_for_log(text: str, limit: int = 400) -> str:
    compact = text.replace("\r", "\\r").replace("\n", "\\n")
    if len(compact) <= limit:
        return compact
    return f"{compact[: limit - 3]}..."


if not KEY:
    raise RuntimeError("OPENAI_API_KEY 누락: .env 파일을확인하세요.")

OPENAI_URL = "https://api.openai.com/v1/responses"

app = FastAPI(title="NPC Proxy (OpenAI Responses API)", version=VERSION)


class ChatMsg(BaseModel):
    role: str = Field(..., description='"user" 또는 "assistant"')
    text: str = Field(..., description="Plain text content for the role")

    @validator("role")
    def validate_role(cls, value: str) -> str:
        role = value.strip().lower()
        if role not in ALLOWED_ROLES:
            raise ValueError(f"role must be one of {sorted(ALLOWED_ROLES)}")
        return role

    @validator("text")
    def text_not_blank(cls, value: str) -> str:
        if not value or not value.strip():
            raise ValueError("text must not be blank")
        return value


class SayReq(BaseModel):
    messages: List[ChatMsg]

    @validator("messages")
    def require_messages(cls, value: List[ChatMsg]) -> List[ChatMsg]:
        if not value:
            raise ValueError("messages must contain at least one entry")
        return value


class ReplyOut(BaseModel):
    reply: str
    version: str = VERSION


@app.on_event("startup")
async def on_startup() -> None:
    timeout = httpx.Timeout(30.0, connect=10.0)
    app.state.http_client = httpx.AsyncClient(timeout=timeout)


@app.on_event("shutdown")
async def on_shutdown() -> None:
    client: httpx.AsyncClient = app.state.http_client
    await client.aclose()


async def call_openai(messages: List[ChatMsg]) -> Dict[str, Any]:
    payload_messages: List[Dict[str, Any]] = []

    if SYSTEM_PROMPT:
        payload_messages.append(
            {
                "role": "system",
                "content": [{"type": "text", "text": SYSTEM_PROMPT}]
            }
        )

    for message in messages:
        payload_messages.append(
            {
                "role": message.role,
                "content": [{"type": "text", "text": message.text}]
            }
        )

    payload = {
        "model": MODEL,
        "input": payload_messages,
    }

    headers = {
        "Authorization": f"Bearer {KEY}",
        "Content-Type": "application/json",
    }

    client: httpx.AsyncClient = app.state.http_client
    response = await client.post(OPENAI_URL, headers=headers, json=payload)

    if response.status_code >= 400:
        logger.warning(
            "OpenAI error %s: %s",
            response.status_code,
            _truncate_for_log(response.text),
        )
        raise HTTPException(
            status_code=502,
            detail=f"openai error {response.status_code}: {response.text}",
        )

    try:
        data: Dict[str, Any] = response.json()
    except json.JSONDecodeError as exc:
        logger.exception("invalid json from openai")
        raise HTTPException(status_code=502, detail=f"invalid json from openai: {exc}")

    return data


# ──유틸: OpenAI 응답에서텍스트안전추출──
def extract_output_text(data: Any) -> str | None:
    if isinstance(data, dict):
        ot = data.get("output_text")
        if isinstance(ot, str) and ot.strip():
            return ot.strip()

    texts: List[str] = []

    # 2) 'output'가 dict 또는 list 인 모든 케이스 커버
    output = data.get("output") if isinstance(data, dict) else None
    if isinstance(output, dict):
        output = [output]
    if isinstance(output, list):
        for item in output:
            if isinstance(item, dict) and item.get("type") == "message" and item.get("role") == "assistant":
                for part in item.get("content", []):
                    if isinstance(part, dict) and part.get("type") == "text":
                        text = part.get("text")
                        if isinstance(text, str) and text.strip():
                            texts.append(text.strip())
        if texts:
            return "\n".join(texts)

    # 3) Responses API 단일 message 필드 fallback
    message = data.get("message") if isinstance(data, dict) else None
    if isinstance(message, dict):
        content = message.get("content")
        if isinstance(content, list):
            for part in content:
                if isinstance(part, dict) and part.get("type") == "text":
                    text = part.get("text")
                    if isinstance(text, str) and text.strip():
                        return text.strip()

    # 4) 예전 completion 포맷(choices) 대비 안전장치
    choices = data.get("choices") if isinstance(data, dict) else None
    if isinstance(choices, list):
        for choice in choices:
            if isinstance(choice, dict):
                message = choice.get("message")
                if isinstance(message, dict):
                    text = message.get("content")
                    if isinstance(text, str) and text.strip():
                        return text.strip()
    return None


@app.post("/npc/say", response_model=ReplyOut)
async def npc_say(payload: SayReq) -> ReplyOut:
    roles = ", ".join(message.role for message in payload.messages)
    logger.info("POST /npc/say received %d message(s) [%s]", len(payload.messages), roles or "n/a")
    data = await call_openai(payload.messages)

    reply = extract_output_text(data)
    if not reply:
        detail = "assistant reply not found in openai response"
        logger.error("%s; keys=%s", detail, sorted(map(str, data.keys())) if isinstance(data, dict) else type(data))
        raise HTTPException(status_code=502, detail=detail)

    logger.info("GPT reply (%d chars): %s", len(reply), _truncate_for_log(reply))
    return ReplyOut(reply=reply)


@app.get("/health")
async def health() -> Dict[str, str]:
    return {"status": "ok", "version": VERSION}


