from __future__ import annotations

import asyncio
from dataclasses import dataclass

from homeassistant.components import zeroconf
from homeassistant.core import HomeAssistant
from zeroconf import IPVersion, ServiceStateChange
from zeroconf.asyncio import AsyncServiceBrowser, AsyncServiceInfo

SERVICE_TYPE = "_ha-ink._tcp.local."


@dataclass(frozen=True, slots=True)
class InkDiscovery:
    device_id: str
    host: str
    port: int

    @property
    def label(self) -> str:
        return f"Ink Display {self.device_id[-6:]}"


async def async_discover_displays(
    hass: HomeAssistant, timeout: float = 5.0
) -> list[InkDiscovery]:
    instance = await zeroconf.async_get_async_instance(hass)
    service = instance.zeroconf
    found: dict[str, InkDiscovery] = {}
    tasks: set[asyncio.Task[None]] = set()
    ready = asyncio.Event()

    async def resolve(name: str) -> None:
        info = AsyncServiceInfo(SERVICE_TYPE, name)
        if not await info.async_request(service, 3000):
            return
        device_id = info.decoded_properties.get("id")
        addresses = info.parsed_addresses(IPVersion.V4Only)
        if not device_id or not addresses or not info.port:
            return
        found[device_id] = InkDiscovery(device_id, addresses[0], info.port)
        ready.set()

    def changed(_service, _type, name, state) -> None:
        if state not in (ServiceStateChange.Added, ServiceStateChange.Updated):
            return
        task = hass.async_create_task(resolve(name))
        tasks.add(task)
        task.add_done_callback(tasks.discard)

    browser = AsyncServiceBrowser(service, [SERVICE_TYPE], handlers=[changed])
    try:
        try:
            await asyncio.wait_for(ready.wait(), timeout)
            await asyncio.sleep(0.3)
        except TimeoutError:
            pass
    finally:
        await browser.async_cancel()
    if tasks:
        await asyncio.gather(*tasks, return_exceptions=True)
    return sorted(found.values(), key=lambda item: item.device_id)
