from __future__ import annotations

from decimal import Decimal, InvalidOperation

from .const import (
    ALERT_ABOVE,
    ALERT_MODES,
    ALERT_OFF,
    CONF_ALERT_MODE,
    CONF_ALERT_THRESHOLD,
    CONF_DECIMALS,
    CONF_ENTITY,
    CONF_INTERVAL,
    CONF_ITEMS,
    CONF_LABEL,
    CONF_RED_THRESHOLD,
    CONF_ROW_ONE_COUNT,
    CONF_ROW_TWO_COUNT,
    CONF_TITLE,
    CONF_UNIT,
    DEFAULT_INTERVAL,
    DEFAULT_TITLE,
    MAX_INTERVAL,
    MAX_ITEMS,
    MAX_ROW_ITEMS,
    MIN_INTERVAL,
    NO_RED,
)
from .protocol import normalize_text


def empty_layout() -> dict:
    return {
        CONF_TITLE: DEFAULT_TITLE,
        CONF_INTERVAL: DEFAULT_INTERVAL,
        CONF_ROW_ONE_COUNT: 0,
        CONF_ROW_TWO_COUNT: 0,
        CONF_ITEMS: [],
    }


def normalize_layout(source: dict | None, allow_empty: bool = False) -> dict:
    data = source or empty_layout()
    items = [normalize_item(item) for item in data.get(CONF_ITEMS, [])]
    if not allow_empty and len(items) < 2:
        raise ValueError("invalid_item_count")
    if len(items) > MAX_ITEMS:
        raise ValueError("invalid_item_count")
    counts = {1: 0, 2: 0}
    for item in items:
        row = item["row"]
        if row not in counts:
            raise ValueError("invalid_row")
        counts[row] += 1
    if any(count > MAX_ROW_ITEMS for count in counts.values()):
        raise ValueError("invalid_row_count")
    if items and any(count == 0 for count in counts.values()):
        raise ValueError("empty_row")
    interval = int(data.get(CONF_INTERVAL, DEFAULT_INTERVAL))
    if interval < MIN_INTERVAL or interval > MAX_INTERVAL:
        raise ValueError("invalid_interval")
    return {
        CONF_TITLE: normalize_text(str(data.get(CONF_TITLE, DEFAULT_TITLE)), 24),
        CONF_INTERVAL: interval,
        CONF_ROW_ONE_COUNT: counts[1],
        CONF_ROW_TWO_COUNT: counts[2],
        CONF_ITEMS: items,
    }


def normalize_item(source: dict) -> dict:
    entity = str(source.get(CONF_ENTITY, "")).strip()
    if not entity:
        raise ValueError("invalid_entity")
    mode = str(source.get(CONF_ALERT_MODE, "")).strip()
    threshold = source.get(CONF_ALERT_THRESHOLD, "")
    legacy = source.get(CONF_RED_THRESHOLD, NO_RED)
    if not mode:
        mode = ALERT_OFF if legacy in (None, "", NO_RED) else ALERT_ABOVE
        threshold = "" if mode == ALERT_OFF else legacy
    if mode not in ALERT_MODES:
        raise ValueError("invalid_alert_mode")
    normalized_threshold = ""
    if mode != ALERT_OFF:
        try:
            number = Decimal(str(threshold).strip())
        except InvalidOperation as error:
            raise ValueError("invalid_alert_threshold") from error
        if not number.is_finite() or number < -2147483 or number > 2147483:
            raise ValueError("invalid_alert_threshold")
        normalized_threshold = format(number.normalize(), "f")
    decimals = int(source.get(CONF_DECIMALS, 0))
    if decimals not in (0, 1, 2):
        raise ValueError("invalid_decimals")
    return {
        CONF_ENTITY: entity,
        CONF_LABEL: normalize_text(str(source.get(CONF_LABEL, "")), 12, True),
        CONF_UNIT: normalize_text(str(source.get(CONF_UNIT, "")), 5, True),
        CONF_DECIMALS: decimals,
        CONF_ALERT_MODE: mode,
        CONF_ALERT_THRESHOLD: normalized_threshold,
        "row": int(source.get("row", 0)),
    }
