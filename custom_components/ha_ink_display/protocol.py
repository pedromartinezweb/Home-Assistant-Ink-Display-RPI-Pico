from __future__ import annotations

import hashlib
import hmac
import math
import re
from dataclasses import dataclass
from datetime import datetime

from .const import MAX_ITEMS, MAX_ROW_ITEMS, NO_RED

SUPPORTED_TEXT = re.compile(r"^[A-Z0-9 .:/%\-]+$")


@dataclass(frozen=True, slots=True)
class DisplayItem:
    entity: str
    label: str
    unit: str
    row: int
    decimals: int
    red_threshold: int


@dataclass(frozen=True, slots=True)
class DisplayValue:
    value_milli: int
    valid: bool


def normalize_text(value: str, maximum: int, empty: bool = False) -> str:
    text = value.strip().upper()
    if (not text and not empty) or len(text) > maximum:
        raise ValueError("invalid_length")
    if text and SUPPORTED_TEXT.fullmatch(text) is None:
        raise ValueError("invalid_characters")
    return text


def validate_items(items: list[DisplayItem]) -> None:
    if len(items) < 2 or len(items) > MAX_ITEMS:
        raise ValueError("invalid_item_count")
    rows = {1: 0, 2: 0}
    for item in items:
        normalize_text(item.label, 12)
        normalize_text(item.unit, 5, True)
        if item.row not in rows or item.decimals not in (0, 1, 2):
            raise ValueError("invalid_item")
        rows[item.row] += 1
    if any(count < 1 or count > MAX_ROW_ITEMS for count in rows.values()):
        raise ValueError("invalid_row_count")


def state_value(state: str | None) -> DisplayValue:
    if state is None:
        return DisplayValue(0, False)
    try:
        number = float(state)
    except (TypeError, ValueError):
        return DisplayValue(0, False)
    if not math.isfinite(number) or number < -2147483 or number > 2147483:
        return DisplayValue(0, False)
    return DisplayValue(round(number * 1000), True)


def frame_payload(
    revision: int,
    interval: int,
    title: str,
    updated: str,
    timestamp: datetime,
    items: list[DisplayItem],
    values: list[DisplayValue],
) -> bytes:
    validate_items(items)
    if revision <= 0 or len(items) != len(values) or interval < 60 or interval > 86400:
        raise ValueError("invalid_frame")
    lines = [
        "INK1",
        str(revision),
        str(interval),
        str(timestamp.hour),
        str(timestamp.minute),
        normalize_text(title, 24),
        normalize_text(updated, 8),
        str(len(items)),
    ]
    for item, value in zip(items, values, strict=True):
        lines.append(
            "|".join(
                (
                    str(item.row),
                    str(item.decimals),
                    str(item.red_threshold),
                    str(value.value_milli),
                    "1" if value.valid else "0",
                    normalize_text(item.label, 12),
                    normalize_text(item.unit, 5, True),
                )
            )
        )
    return ("\n".join(lines) + "\n").encode()


def signature(secret: bytes, payload: bytes) -> str:
    return hmac.new(secret, payload, hashlib.sha256).hexdigest()


def pair_payload(
    code: int,
    host: str,
    port: int,
    path: str,
    secret: bytes,
) -> bytes:
    if (
        code < 100000
        or code > 999999
        or not host
        or port < 1
        or port > 65535
        or not path.startswith("/")
    ):
        raise ValueError("invalid_pairing")
    return f"PAIR1\n{code}\n{host}\n{port}\n{path}\n{secret.hex()}\n".encode()


def item_from_dict(data: dict, row: int) -> DisplayItem:
    threshold = data.get("red_threshold")
    return DisplayItem(
        entity=data["entity"],
        label=normalize_text(data["label"], 12),
        unit=normalize_text(data.get("unit", ""), 5, True),
        row=row,
        decimals=int(data["decimals"]),
        red_threshold=NO_RED if threshold in (None, "") else int(threshold),
    )
