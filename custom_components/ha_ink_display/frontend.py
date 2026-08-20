from __future__ import annotations

from pathlib import Path

import voluptuous as vol
from homeassistant.components import panel_custom, websocket_api
from homeassistant.components.http import StaticPathConfig
from homeassistant.core import HomeAssistant

from .const import CONF_LAYOUT, DOMAIN
from .layout import normalize_layout

PANEL_PATH = "ink-display"
STATIC_PATH = "/ha_ink_display_frontend"


async def async_setup_frontend(hass: HomeAssistant) -> None:
    frontend_dir = Path(__file__).parent / "frontend"
    await hass.http.async_register_static_paths(
        [StaticPathConfig(STATIC_PATH, str(frontend_dir), False)]
    )
    websocket_api.async_register_command(hass, websocket_config)
    websocket_api.async_register_command(hass, websocket_save)
    await panel_custom.async_register_panel(
        hass,
        webcomponent_name="ha-ink-display-panel",
        frontend_url_path=PANEL_PATH,
        module_url=f"{STATIC_PATH}/ha-ink-display-panel.js",
        sidebar_title="Ink Display",
        sidebar_icon="mdi:monitor-dashboard",
        require_admin=True,
    )


@websocket_api.websocket_command({vol.Required("type"): f"{DOMAIN}/config"})
@websocket_api.async_response
async def websocket_config(hass, connection, msg) -> None:
    connection.require_admin()
    entries = []
    for entry in hass.config_entries.async_entries(DOMAIN):
        source = entry.options or entry.data.get(CONF_LAYOUT, {})
        entries.append(
            {
                "entry_id": entry.entry_id,
                "title": entry.title,
                "layout": normalize_layout(source, allow_empty=True),
            }
        )
    connection.send_result(msg["id"], {"entries": entries})


@websocket_api.websocket_command(
    {
        vol.Required("type"): f"{DOMAIN}/save",
        vol.Required("entry_id"): str,
        vol.Required("layout"): dict,
    }
)
@websocket_api.async_response
async def websocket_save(hass, connection, msg) -> None:
    connection.require_admin()
    entry = hass.config_entries.async_get_entry(msg["entry_id"])
    if entry is None or entry.domain != DOMAIN:
        connection.send_error(msg["id"], "not_found", "Display not found")
        return
    try:
        layout = normalize_layout(msg["layout"])
    except (TypeError, ValueError) as error:
        connection.send_error(msg["id"], "invalid_layout", str(error))
        return
    hass.config_entries.async_update_entry(entry, options=layout)
    connection.send_result(msg["id"], {"saved": True})
