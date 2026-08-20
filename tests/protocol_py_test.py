import importlib.util
import sys
import unittest
from datetime import UTC, datetime
from pathlib import Path
from types import ModuleType

ROOT = Path(__file__).resolve().parents[1]
PACKAGE = "custom_components.ha_ink_display"
package = ModuleType(PACKAGE)
package.__path__ = [str(ROOT / "custom_components" / "ha_ink_display")]
sys.modules[PACKAGE] = package


def load(name: str):
    path = ROOT / "custom_components" / "ha_ink_display" / f"{name}.py"
    spec = importlib.util.spec_from_file_location(f"{PACKAGE}.{name}", path)
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


const = load("const")
protocol = load("protocol")
layout = load("layout")


class ProtocolTest(unittest.TestCase):
    def test_frame(self):
        items = [
            protocol.DisplayItem("sensor.co2", "CO2", "PPM", 1, 0, const.ALERT_ABOVE, 1000000),
            protocol.DisplayItem("sensor.temp", "", "°C", 2, 1, const.ALERT_BELOW, 5000),
        ]
        values = [
            protocol.DisplayValue(1201000, True),
            protocol.DisplayValue(23400, True),
        ]
        body = protocol.frame_payload(
            42,
            300,
            "House",
            datetime(2026, 8, 20, 18, 7, tzinfo=UTC),
            items,
            values,
        )
        self.assertEqual(
            body,
            b"INK2\n42\n300\n18\n7\nHOUSE\nACT\n2\n"
            b"1|0|1|1000000|1201000|1|CO2|PPM\n"
            b"2|1|2|5000|23400|1||~C\n",
        )
        self.assertEqual(len(protocol.signature(bytes(32), body)), 64)

    def test_pair(self):
        body = protocol.pair_payload(
            123456,
            "192.168.1.20",
            8123,
            "/api/ha_ink_display/poll/device",
            bytes(range(32)),
        )
        self.assertTrue(body.startswith(b"PAIR1\n123456\n192.168.1.20\n8123\n"))
        self.assertTrue(body.endswith(b"1d1e1f\n"))

    def test_validation(self):
        with self.assertRaises(ValueError):
            protocol.normalize_text("temperature", 8)
        invalid = [protocol.DisplayItem("sensor.one", "ONE", "C", 1, 0, const.ALERT_OFF, 0)]
        with self.assertRaises(ValueError):
            protocol.validate_items(invalid)
        self.assertFalse(protocol.state_value("unknown").valid)
        self.assertFalse(protocol.state_value("nan").valid)

    def test_layout_migration(self):
        source = {
            "title": "House",
            "interval": 300,
            "items": [
                {
                    "entity": "sensor.temperature",
                    "label": "",
                    "unit": "°C",
                    "decimals": 1,
                    "red_threshold": const.NO_RED,
                    "row": 1,
                },
                {
                    "entity": "sensor.humidity",
                    "label": "HUM",
                    "unit": "%",
                    "decimals": 0,
                    "red_threshold": 70,
                    "row": 2,
                },
            ],
        }
        result = layout.normalize_layout(source)
        self.assertEqual(result["items"][0]["alert_mode"], const.ALERT_OFF)
        self.assertEqual(result["items"][1]["alert_mode"], const.ALERT_ABOVE)
        self.assertEqual(result["items"][1]["alert_threshold"], "70")
        self.assertEqual(protocol.wire_text("ºC", 5), "~C")


if __name__ == "__main__":
    unittest.main()
