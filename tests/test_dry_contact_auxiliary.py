"""Validate dry-contact package wiring and exercise its auxiliary GPIO path.

Run in the pinned ESPHome image: python tests/test_dry_contact_auxiliary.py.
GPIO levels and elapsed time are simulated; this does not test physical wiring.
"""

from contextlib import redirect_stdout
import io
import logging
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import unittest

from esphome.__main__ import generate_cpp_contents
from esphome.config import read_config
from esphome.core import CORE
import esphome


ROOT = Path(__file__).resolve().parents[1]


def local_config(directory, overrides=None, extra_input=None):
    text = (ROOT / "circuitsetup-gdo-dry-contact.yaml").read_text(encoding="utf-8")
    packages = re.search(r"^packages:\n.*?(?=^####)", text, re.M | re.S)
    files = re.findall(r"^      - (packages/.*\.yaml)", packages.group(), re.M)
    replacement = "packages:\n" + "".join(
        f"  local_{index}: !include {(ROOT / file).as_posix()}\n"
        for index, file in enumerate(files)
    )
    if extra_input is not None:
        replacement += (
            "  extra_aux: !include\n"
            f"    file: {(ROOT / 'packages/gdo/binary-sensor.yaml').as_posix()}\n"
            "    vars:\n"
            f"      pin: GPIO{extra_input}\n"
            "      name: Spare Aux Input\n"
        )
    text = text[:packages.start()] + replacement + "\n" + text[packages.end():]
    for key, value in (overrides or {}).items():
        text, count = re.subn(rf"^  {key}: .*", f"  {key}: {value}", text, flags=re.M)
        assert count == 1, key
    path = Path(directory) / "dry-contact.yaml"
    path.write_text(text, encoding="utf-8")
    CORE.reset()
    CORE.config_path = path
    with redirect_stdout(io.StringIO()):
        config = read_config({})
    CORE.config = config
    return config


def entity(config, domain, entity_id):
    return next(item for item in config[domain] if str(item.get("id")) == entity_id)


class DryContactConfigTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)

    def test_default_auxiliary_gpio_configuration(self):
        config = local_config(self.directory.name)
        self.assertIsNotNone(config)
        sensor = entity(config, "binary_sensor", "garage_door_input")
        self.assertEqual(sensor["pin"]["number"], 5)
        self.assertTrue(sensor["pin"]["mode"]["input"])
        self.assertTrue(sensor["pin"]["mode"]["pullup"])
        self.assertFalse(sensor["pin"]["inverted"])
        self.assertEqual(sensor["filters"][0]["delayed_on_off"].total_milliseconds, 200)
        output = entity(config, "output", "garage_door_opener_output")
        self.assertEqual(output["pin"]["number"], 7)
        self.assertTrue(output["pin"]["mode"]["output"])
        self.assertFalse(output["pin"]["inverted"])
        self.assertFalse(output.get("inverted", False))
        button = entity(config, "button", "garage_door_opener_button")
        self.assertEqual(button["duration"].total_milliseconds, 300)
        self.assertEqual(str(button["output"]), "garage_door_opener_output")

    def test_auxiliary_input_two_override(self):
        config = local_config(self.directory.name, {"wired_sensor_pin": "GPIO6"})
        self.assertIsNotNone(config)
        self.assertEqual(entity(config, "binary_sensor", "garage_door_input")["pin"]["number"], 6)
        self.assertEqual(entity(config, "output", "garage_door_opener_output")["pin"]["number"], 7)

    def test_duplicate_auxiliary_pin_is_rejected(self):
        for overrides in ({"wired_sensor_pin": "GPIO7"}, {"door_control_pin": "GPIO5"}):
            with self.subTest(overrides=overrides):
                self.assertIsNone(local_config(self.directory.name, overrides))

    def test_spare_auxiliary_input_can_coexist_with_door_input(self):
        config = local_config(self.directory.name, extra_input=6)
        self.assertIsNotNone(config)
        spare = next(item for item in config["binary_sensor"] if item["name"] == "Spare Aux Input")
        self.assertEqual(spare["pin"]["number"], 6)
        self.assertTrue(spare["pin"]["mode"]["pullup"])
        self.assertEqual(entity(config, "binary_sensor", "garage_door_input")["pin"]["number"], 5)
        for pin in (5, 7):
            with self.subTest(duplicate_pin=pin):
                self.assertIsNone(local_config(self.directory.name, extra_input=pin))

    def test_conflicts_with_populated_hardware_are_rejected(self):
        for setting in ("wired_sensor_pin", "door_control_pin"):
            for pin in (3, 4, 15, 16, 18):
                with self.subTest(setting=setting, pin=pin):
                    self.assertIsNone(local_config(self.directory.name, {setting: f"GPIO{pin}"}))

    def test_generated_cpp_keeps_auxiliary_wiring_and_timing(self):
        for pin in (5, 6):
            with self.subTest(input_pin=pin):
                config = local_config(self.directory.name, {"wired_sensor_pin": f"GPIO{pin}"})
                self.assertIsNotNone(config)
                generate_cpp_contents(config)
                source = CORE.cpp_main_section
                calls = dict(re.findall(r"^\s*(\w+)->set_pin\(([\w:]+)\);", source, re.M))
                self.assertEqual(calls[calls["garage_door_input"]], f"::GPIO_NUM_{pin}")
                self.assertEqual(calls[calls["garage_door_opener_output"]], "::GPIO_NUM_7")
                sensor_pin = re.escape(calls["garage_door_input"])
                relay_pin = re.escape(calls["garage_door_opener_output"])
                self.assertRegex(source, sensor_pin + r"->set_flags\(\(gpio::Flags::FLAG_INPUT \| gpio::Flags::FLAG_PULLUP\)\);")
                self.assertRegex(source, relay_pin + r"->set_flags\(gpio::Flags::FLAG_OUTPUT\);")
                self.assertNotRegex(source, relay_pin + r"->set_inverted\(true\);")
                self.assertRegex(source, r"garage_door_opener_button->set_duration\(300\);")
                self.assertRegex(source, r"garage_door_opener_button->set_output\(garage_door_opener_output\);")
                self.assertRegex(source, r"(?s)->set_on_delay\(\[\]\(\) -> uint32_t \{\s*return 200;")
                self.assertRegex(source, r"(?s)->set_off_delay\(\[\]\(\) -> uint32_t \{\s*return 200;")

    @unittest.skipUnless(shutil.which("g++"), "g++ is required for the auxiliary GPIO process test")
    def test_sensor_filter_cover_and_momentary_output_process(self):
        from dry_contact_runtime import build_harness

        for pin in (5, 6):
            with self.subTest(input_pin=pin):
                config = local_config(self.directory.name, {"wired_sensor_pin": f"GPIO{pin}"}, 6 if pin == 5 else 5)
                self.assertIsNotNone(config)
                source = build_harness(config, Path(esphome.__file__).parent)
                cpp = Path(self.directory.name) / "auxiliary.cpp"
                executable = Path(self.directory.name) / "auxiliary"
                cpp.write_text(source, encoding="utf-8")
                subprocess.run(["g++", "-std=c++17", str(cpp), "-o", str(executable)], check=True)
                subprocess.run([str(executable)], check=True)


if __name__ == "__main__":
    logging.getLogger().setLevel(logging.ERROR)
    unittest.main()
