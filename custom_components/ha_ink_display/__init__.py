from __future__ import annotations

from homeassistant.config_entries import ConfigEntry
from homeassistant.core import HomeAssistant
from homeassistant.helpers import config_validation as cv
from homeassistant.helpers import device_registry as dr
from homeassistant.helpers import network

from .const import CONF_DEVICE_ID, DOMAIN
from .frontend import PANEL_PATH, async_setup_frontend
from .http import InkPollView
from .runtime import InkRuntime

CONFIG_SCHEMA = cv.config_entry_only_config_schema(DOMAIN)


async def async_setup(hass: HomeAssistant, config: dict) -> bool:
    hass.data.setdefault(DOMAIN, {})
    hass.http.register_view(InkPollView())
    await async_setup_frontend(hass)
    return True


async def async_setup_entry(hass: HomeAssistant, entry: ConfigEntry) -> bool:
    runtime = InkRuntime(hass, entry)
    device_id = entry.data[CONF_DEVICE_ID]
    instance_url = network.get_url(
        hass,
        allow_internal=True,
        allow_external=True,
        allow_cloud=False,
        prefer_external=True,
    )
    hass.data[DOMAIN][device_id] = runtime
    dr.async_get(hass).async_get_or_create(
        config_entry_id=entry.entry_id,
        identifiers={(DOMAIN, device_id)},
        manufacturer="Raspberry Pi",
        model="Pico W / Pico 2 W Ink Display",
        name=f"Ink Display {device_id[-6:]}",
        configuration_url=f"{instance_url.rstrip('/')}/{PANEL_PATH}",
    )
    await runtime.start()
    entry.async_on_unload(entry.add_update_listener(_reload_entry))
    return True


async def async_unload_entry(hass: HomeAssistant, entry: ConfigEntry) -> bool:
    runtime = hass.data[DOMAIN].pop(entry.data[CONF_DEVICE_ID], None)
    if runtime is not None:
        await runtime.stop()
    return True


async def _reload_entry(hass: HomeAssistant, entry: ConfigEntry) -> None:
    await hass.config_entries.async_reload(entry.entry_id)
