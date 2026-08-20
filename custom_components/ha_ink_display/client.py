from __future__ import annotations

import asyncio
import json
from dataclasses import dataclass

from aiohttp import ClientError, ClientSession

from .protocol import pair_payload


class InkClientError(Exception):
    pass


class InkCodeError(InkClientError):
    pass


@dataclass(frozen=True, slots=True)
class InkInfo:
    device_id: str
    name: str
    version: int
    paired: bool


class InkClient:
    def __init__(self, session: ClientSession, host: str, port: int) -> None:
        self._session = session
        self._base = f"http://{host}:{port}"

    async def info(self) -> InkInfo:
        try:
            async with asyncio.timeout(10):
                async with self._session.get(f"{self._base}/v1/info") as response:
                    if response.status != 200:
                        raise InkClientError("cannot_connect")
                    data = await response.json()
        except (
            TimeoutError,
            ClientError,
            json.JSONDecodeError,
            KeyError,
            TypeError,
        ) as error:
            raise InkClientError("cannot_connect") from error
        return InkInfo(
            device_id=str(data["id"]),
            name=str(data["name"]),
            version=int(data["version"]),
            paired=bool(data["paired"]),
        )

    async def pair(
        self,
        code: int,
        callback_host: str,
        callback_port: int,
        callback_path: str,
        secret: bytes,
    ) -> None:
        body = pair_payload(code, callback_host, callback_port, callback_path, secret)
        try:
            async with asyncio.timeout(15):
                async with self._session.post(
                    f"{self._base}/v1/pair",
                    data=body,
                    headers={"Content-Type": "text/plain"},
                ) as response:
                    if response.status == 403:
                        raise InkCodeError("invalid_code")
                    if response.status != 204:
                        raise InkClientError("cannot_pair")
        except InkClientError:
            raise
        except (TimeoutError, ClientError) as error:
            raise InkClientError("cannot_connect") from error
