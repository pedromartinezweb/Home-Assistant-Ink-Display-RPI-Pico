from __future__ import annotations

import secrets
from urllib.parse import urlsplit

import voluptuous as vol
from homeassistant import config_entries
from homeassistant.const import CONF_HOST, CONF_PORT
from homeassistant.helpers import network, selector
from homeassistant.helpers.aiohttp_client import async_get_clientsession

from .client import InkClient, InkClientError, InkCodeError
from .const import (
    API_PREFIX,
    CONF_DECIMALS,
    CONF_DEVICE_ID,
    CONF_ENTITY,
    CONF_INTERVAL,
    CONF_ITEMS,
    CONF_LABEL,
    CONF_LAYOUT,
    CONF_RED_THRESHOLD,
    CONF_ROW_ONE_COUNT,
    CONF_ROW_TWO_COUNT,
    CONF_SECRET,
    CONF_TITLE,
    CONF_UNIT,
    CONF_UPDATED,
    DEFAULT_INTERVAL,
    DEFAULT_TITLE,
    DEFAULT_UPDATED,
    DOMAIN,
    MAX_INTERVAL,
    MIN_INTERVAL,
    NO_RED,
    PAIR_PORT,
)
from .protocol import normalize_text


def dashboard_schema(defaults: dict | None = None) -> vol.Schema:
    source = defaults or {}
    return vol.Schema(
        {
            vol.Required(
                CONF_TITLE, default=source.get(CONF_TITLE, DEFAULT_TITLE)
            ): selector.TextSelector(),
            vol.Required(
                CONF_UPDATED, default=source.get(CONF_UPDATED, DEFAULT_UPDATED)
            ): selector.TextSelector(),
            vol.Required(
                CONF_INTERVAL,
                default=source.get(CONF_INTERVAL, DEFAULT_INTERVAL),
            ): selector.NumberSelector(
                selector.NumberSelectorConfig(
                    min=MIN_INTERVAL,
                    max=MAX_INTERVAL,
                    step=60,
                    mode=selector.NumberSelectorMode.BOX,
                    unit_of_measurement="s",
                )
            ),
            vol.Required(
                CONF_ROW_ONE_COUNT,
                default=source.get(CONF_ROW_ONE_COUNT, 2),
            ): selector.NumberSelector(
                selector.NumberSelectorConfig(
                    min=1, max=4, step=1, mode=selector.NumberSelectorMode.BOX
                )
            ),
            vol.Required(
                CONF_ROW_TWO_COUNT,
                default=source.get(CONF_ROW_TWO_COUNT, 2),
            ): selector.NumberSelector(
                selector.NumberSelectorConfig(
                    min=1, max=4, step=1, mode=selector.NumberSelectorMode.BOX
                )
            ),
        }
    )


def item_schema(defaults: dict | None = None) -> vol.Schema:
    source = defaults or {}
    fields: dict = {}
    entity = source.get(CONF_ENTITY)
    entity_key = (
        vol.Required(CONF_ENTITY, default=entity)
        if entity
        else vol.Required(CONF_ENTITY)
    )
    fields[entity_key] = selector.EntitySelector()
    fields[vol.Required(CONF_LABEL, default=source.get(CONF_LABEL, "VALUE"))] = (
        selector.TextSelector()
    )
    fields[vol.Optional(CONF_UNIT, default=source.get(CONF_UNIT, ""))] = (
        selector.TextSelector()
    )
    fields[vol.Required(CONF_DECIMALS, default=source.get(CONF_DECIMALS, 0))] = (
        selector.SelectSelector(
            selector.SelectSelectorConfig(
                options=["0", "1", "2"], mode=selector.SelectSelectorMode.DROPDOWN
            )
        )
    )
    threshold = source.get(CONF_RED_THRESHOLD, "")
    if threshold == NO_RED:
        threshold = ""
    fields[vol.Optional(CONF_RED_THRESHOLD, default=str(threshold))] = (
        selector.TextSelector()
    )
    return vol.Schema(fields)


def clean_dashboard(data: dict) -> dict:
    return {
        CONF_TITLE: normalize_text(str(data[CONF_TITLE]), 24),
        CONF_UPDATED: normalize_text(str(data[CONF_UPDATED]), 8),
        CONF_INTERVAL: int(data[CONF_INTERVAL]),
        CONF_ROW_ONE_COUNT: int(data[CONF_ROW_ONE_COUNT]),
        CONF_ROW_TWO_COUNT: int(data[CONF_ROW_TWO_COUNT]),
        CONF_ITEMS: [],
    }


def clean_item(data: dict, row: int) -> dict:
    threshold = str(data.get(CONF_RED_THRESHOLD, "")).strip()
    return {
        CONF_ENTITY: data[CONF_ENTITY],
        CONF_LABEL: normalize_text(str(data[CONF_LABEL]), 12),
        CONF_UNIT: normalize_text(str(data.get(CONF_UNIT, "")), 5, True),
        CONF_DECIMALS: int(data[CONF_DECIMALS]),
        CONF_RED_THRESHOLD: NO_RED if not threshold else int(threshold),
        "row": row,
    }


class LayoutMixin:
    _layout: dict
    _positions: list[int]
    _position: int

    def _start_items(self, layout: dict) -> None:
        self._layout = layout
        self._positions = [1] * layout[CONF_ROW_ONE_COUNT] + [2] * layout[
            CONF_ROW_TWO_COUNT
        ]
        self._position = 0

    def _item_default(self) -> dict | None:
        return None

    async def _item_step(self, user_input: dict | None = None):
        errors = {}
        row = self._positions[self._position]
        if user_input is not None:
            try:
                self._layout[CONF_ITEMS].append(clean_item(user_input, row))
            except (TypeError, ValueError):
                errors["base"] = "invalid_item"
            else:
                self._position += 1
                if self._position == len(self._positions):
                    return await self._layout_complete()
                row = self._positions[self._position]
        position_in_row = self._positions[: self._position + 1].count(row)
        return self.async_show_form(
            step_id="item",
            data_schema=item_schema(self._item_default()),
            errors=errors,
            description_placeholders={
                "row": str(row),
                "position": str(position_in_row),
                "total": str(self._positions.count(row)),
            },
        )


class InkConfigFlow(LayoutMixin, config_entries.ConfigFlow, domain=DOMAIN):
    VERSION = 1

    def __init__(self) -> None:
        self._host = ""
        self._port = PAIR_PORT
        self._device_id = ""
        self._name = "Ink Display"
        self._code = 0

    async def async_step_zeroconf(self, discovery_info):
        self._host = discovery_info.host
        self._port = discovery_info.port
        self._device_id = str(discovery_info.properties.get("id", ""))
        if not self._device_id:
            return self.async_abort(reason="invalid_discovery")
        await self.async_set_unique_id(self._device_id)
        self._abort_if_unique_id_configured()
        return await self.async_step_pair()

    async def async_step_user(self, user_input: dict | None = None):
        errors = {}
        if user_input is not None:
            self._host = user_input[CONF_HOST]
            self._port = int(user_input[CONF_PORT])
            client = InkClient(
                async_get_clientsession(self.hass), self._host, self._port
            )
            try:
                info = await client.info()
            except InkClientError:
                errors["base"] = "cannot_connect"
            else:
                if info.paired:
                    errors["base"] = "already_paired"
                else:
                    self._device_id = info.device_id
                    self._name = info.name
                    await self.async_set_unique_id(self._device_id)
                    self._abort_if_unique_id_configured()
                    return await self.async_step_pair()
        return self.async_show_form(
            step_id="user",
            data_schema=vol.Schema(
                {
                    vol.Required(CONF_HOST): selector.TextSelector(),
                    vol.Required(CONF_PORT, default=PAIR_PORT): selector.NumberSelector(
                        selector.NumberSelectorConfig(
                            min=1, max=65535, mode=selector.NumberSelectorMode.BOX
                        )
                    ),
                }
            ),
            errors=errors,
        )

    async def async_step_pair(self, user_input: dict | None = None):
        errors = {}
        if user_input is not None:
            try:
                self._code = int(str(user_input["code"]).strip())
                if self._code < 100000 or self._code > 999999:
                    raise ValueError
            except (TypeError, ValueError):
                errors["base"] = "invalid_code"
            else:
                return await self.async_step_dashboard()
        return self.async_show_form(
            step_id="pair",
            data_schema=vol.Schema({vol.Required("code"): selector.TextSelector()}),
            errors=errors,
            description_placeholders={"device": self._device_id[-6:]},
        )

    async def async_step_dashboard(self, user_input: dict | None = None):
        errors = {}
        if user_input is not None:
            try:
                layout = clean_dashboard(user_input)
            except (TypeError, ValueError):
                errors["base"] = "invalid_dashboard"
            else:
                self._start_items(layout)
                return await self.async_step_item()
        return self.async_show_form(
            step_id="dashboard",
            data_schema=dashboard_schema(),
            errors=errors,
        )

    async def async_step_item(self, user_input: dict | None = None):
        return await self._item_step(user_input)

    async def _layout_complete(self):
        try:
            instance = network.get_url(
                self.hass,
                allow_internal=True,
                allow_external=False,
                allow_cloud=False,
                prefer_external=False,
            )
        except network.NoURLAvailableError:
            return self.async_abort(reason="no_internal_url")
        parsed = urlsplit(instance)
        if parsed.scheme != "http" or parsed.hostname is None:
            return self.async_abort(reason="local_http_required")
        secret = secrets.token_bytes(32)
        path = f"{API_PREFIX}/{self._device_id}"
        client = InkClient(async_get_clientsession(self.hass), self._host, self._port)
        try:
            await client.pair(
                self._code,
                parsed.hostname,
                parsed.port or 80,
                path,
                secret,
            )
        except InkCodeError:
            return self.async_abort(reason="invalid_code")
        except InkClientError:
            return self.async_abort(reason="cannot_pair")
        return self.async_create_entry(
            title=f"Home Assistant Ink Display {self._device_id[-6:]}",
            data={
                CONF_DEVICE_ID: self._device_id,
                CONF_HOST: self._host,
                CONF_PORT: self._port,
                CONF_SECRET: secret.hex(),
                CONF_LAYOUT: self._layout,
            },
        )

    @staticmethod
    def async_get_options_flow(config_entry):
        return InkOptionsFlow(config_entry)


class InkOptionsFlow(LayoutMixin, config_entries.OptionsFlow):
    def __init__(self, entry) -> None:
        self._entry = entry
        self._source = dict(entry.options or entry.data[CONF_LAYOUT])

    async def async_step_init(self, user_input: dict | None = None):
        errors = {}
        if user_input is not None:
            try:
                layout = clean_dashboard(user_input)
            except (TypeError, ValueError):
                errors["base"] = "invalid_dashboard"
            else:
                self._start_items(layout)
                return await self.async_step_item()
        return self.async_show_form(
            step_id="init",
            data_schema=dashboard_schema(self._source),
            errors=errors,
        )

    def _item_default(self) -> dict | None:
        row = self._positions[self._position]
        offset = self._positions[: self._position + 1].count(row) - 1
        matches = [item for item in self._source[CONF_ITEMS] if int(item["row"]) == row]
        return matches[offset] if offset < len(matches) else None

    async def async_step_item(self, user_input: dict | None = None):
        return await self._item_step(user_input)

    async def _layout_complete(self):
        return self.async_create_entry(title="", data=self._layout)
