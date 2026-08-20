from __future__ import annotations

import time
from dataclasses import replace
from typing import Any

from homeassistant.config_entries import ConfigEntry
from homeassistant.core import Event, HomeAssistant, callback
from homeassistant.helpers.event import async_track_state_change_event
from homeassistant.util import dt as dt_util

from .const import CONF_INTERVAL, CONF_ITEMS, CONF_LAYOUT, CONF_TITLE
from .layout import normalize_layout
from .protocol import DisplayItem, frame_payload, item_from_dict, signature, state_value


class InkRuntime:
    def __init__(self, hass: HomeAssistant, entry: ConfigEntry) -> None:
        self.hass = hass
        self.entry = entry
        self.secret = bytes.fromhex(entry.data["secret"])
        self.revision = time.time_ns()
        self._remove_listener: Any = None

    @property
    def layout(self) -> dict:
        source = dict(self.entry.options or self.entry.data[CONF_LAYOUT])
        return normalize_layout(source, allow_empty=True)

    async def start(self) -> None:
        entities = [item["entity"] for item in self.layout[CONF_ITEMS]]
        if not entities:
            return
        self._remove_listener = async_track_state_change_event(
            self.hass,
            entities,
            self._state_changed,
        )

    async def stop(self) -> None:
        if self._remove_listener is not None:
            self._remove_listener()
            self._remove_listener = None

    @callback
    def _state_changed(self, event: Event) -> None:
        self.revision = max(self.revision + 1, time.time_ns())

    def _unit(self, configured: str, entity: str) -> str:
        if configured:
            return configured
        state = self.hass.states.get(entity)
        if state is None:
            return ""
        value = str(state.attributes.get("unit_of_measurement", ""))
        value = value.replace("µ", "U").replace("μ", "U").replace("³", "3")
        return "".join(
            character
            for character in value.upper()
            if character in "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 .:/%-º°"
        )[:5]

    def payload(self) -> tuple[bytes, str]:
        layout = self.layout
        items: list[DisplayItem] = []
        values = []
        for source in layout[CONF_ITEMS]:
            item = item_from_dict(source, int(source["row"]))
            item = replace(item, unit=self._unit(item.unit, item.entity))
            items.append(item)
            state = self.hass.states.get(item.entity)
            values.append(state_value(None if state is None else state.state))
        body = frame_payload(
            self.revision,
            int(layout[CONF_INTERVAL]),
            layout[CONF_TITLE],
            dt_util.now(),
            items,
            values,
        )
        return body, signature(self.secret, body)
