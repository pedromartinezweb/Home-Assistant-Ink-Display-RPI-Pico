from __future__ import annotations

import hmac

from aiohttp import web
from homeassistant.components.http import HomeAssistantView

from .const import API_PREFIX, DOMAIN
from .protocol import signature


class InkPollView(HomeAssistantView):
    url = f"{API_PREFIX}/{{device_id}}"
    name = "api:ha_ink_display:poll"
    requires_auth = False

    async def get(self, request: web.Request, device_id: str) -> web.Response:
        hass = request.app["hass"]
        runtime = hass.data[DOMAIN].get(device_id)
        if runtime is None:
            return web.Response(status=404)
        try:
            revision = int(request.query.get("revision", "0"))
        except ValueError:
            return web.Response(status=400)
        authorization = request.headers.get("X-Ink-Authorization", "")
        expected = signature(runtime.secret, f"POLL\n{revision}\n".encode())
        if not hmac.compare_digest(authorization, expected):
            return web.Response(status=401)
        if revision >= runtime.revision:
            return web.Response(status=204)
        if len(runtime.layout.get("items", [])) < 2:
            return web.Response(status=204)
        body, digest = runtime.payload()
        return web.Response(
            body=body,
            content_type="text/plain",
            headers={"X-Ink-Signature": digest, "Cache-Control": "no-store"},
        )
