from __future__ import annotations

import hashlib
import hmac
import math
import re
from dataclasses import dataclass
from datetime import datetime
from decimal import Decimal

from .const import (
    ALERT_ABOVE,
    ALERT_BELOW,
    ALERT_OFF,
    CONF_ALERT_MODE,
    CONF_ALERT_THRESHOLD,
    MAX_ITEMS,
    MAX_ROW_ITEMS,
)

SUPPORTED_TEXT = re.compile(r"^[A-Z0-9 .:/%\-º°]+$")
ALERT_CODES = {ALERT_OFF: 0, ALERT_ABOVE: 1, ALERT_BELOW: 2}


@dataclass(frozen=True, slots=True)
class DisplayItem:
    entity: str
    label: str
    unit: str
    row: int
    decimals: int
    alert_mode: str
    alert_threshold_milli: int


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


def wire_text(value: str, maximum: int, empty: bool = False) -> str:
    return normalize_text(value, maximum, empty).replace("º", "~").replace("°", "~")


def validate_items(items: list[DisplayItem]) -> None:
    if len(items) < 2 or len(items) > MAX_ITEMS:
        raise ValueError("invalid_item_count")
    rows = {1: 0, 2: 0}
    for item in items:
        normalize_text(item.label, 12, True)
        normalize_text(item.unit, 5, True)
        if (
            item.row not in rows
            or item.decimals not in (0, 1, 2)
            or item.alert_mode not in ALERT_CODES
        ):
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
    timestamp: datetime,
    items: list[DisplayItem],
    values: list[DisplayValue],
) -> bytes:
    validate_items(items)
    if revision <= 0 or len(items) != len(values) or interval < 60 or interval > 86400:
        raise ValueError("invalid_frame")
    lines = [
        "INK2",
        str(revision),
        str(interval),
        str(timestamp.hour),
        str(timestamp.minute),
        wire_text(title, 24),
        "ACT",
        str(len(items)),
    ]
    for item, value in zip(items, values, strict=True):
        lines.append(
            "|".join(
                (
                    str(item.row),
                    str(item.decimals),
                    str(ALERT_CODES[item.alert_mode]),
                    str(item.alert_threshold_milli),
                    str(value.value_milli),
                    "1" if value.valid else "0",
                    wire_text(item.label, 12, True),
                    wire_text(item.unit, 5, True),
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
    mode = data.get(CONF_ALERT_MODE, ALERT_OFF)
    threshold = data.get(CONF_ALERT_THRESHOLD, "")
    threshold_milli = 0
    if mode != ALERT_OFF:
        threshold_milli = int((Decimal(str(threshold)) * 1000).to_integral_value())
    return DisplayItem(
        entity=data["entity"],
        label=normalize_text(data.get("label", ""), 12, True),
        unit=normalize_text(data.get("unit", ""), 5, True),
        row=row,
        decimals=int(data["decimals"]),
        alert_mode=mode,
        alert_threshold_milli=threshold_milli,
    )
