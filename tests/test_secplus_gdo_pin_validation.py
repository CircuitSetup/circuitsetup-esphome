from pathlib import Path
import subprocess
import sys

import pytest


ROOT = Path(__file__).resolve().parents[1]


def validate_pins(tmp_path, input_pin, output_pin, extra=""):
    config = tmp_path / "pins.yaml"
    config.write_text(
        f"""esphome:
  name: pin-validation
esp32:
  board: esp32-s3-devkitc-1
  framework:
    type: esp-idf
external_components:
  - source:
      type: local
      path: {ROOT.as_posix()}/components
    components: [secplus_gdo]
secplus_gdo:
  input_gdo_pin: {input_pin}
  output_gdo_pin: {output_pin}
{extra}
""",
        encoding="utf-8",
    )
    return subprocess.run(
        [sys.executable, "-m", "esphome", "config", str(config)],
        capture_output=True,
        text=True,
        check=False,
    )


def test_uart_accepts_distinct_internal_gpio_numbers(tmp_path):
    result = validate_pins(tmp_path, "GPIO2", "GPIO1")
    assert result.returncode == 0, result.stdout + result.stderr


@pytest.mark.parametrize(
    ("input_pin", "output_pin"),
    [
        ("{number: GPIO2, inverted: true}", "GPIO1"),
        ("GPIO2", "{number: GPIO1, inverted: true}"),
        ("{number: GPIO2, mode: INPUT_PULLUP}", "GPIO1"),
        ("GPIO2", "{number: GPIO1, mode: OUTPUT_OPEN_DRAIN}"),
    ],
)
def test_uart_rejects_pin_options_the_driver_cannot_apply(tmp_path, input_pin, output_pin):
    result = validate_pins(tmp_path, input_pin, output_pin)
    assert result.returncode != 0, result.stdout + result.stderr


def test_uart_rejects_expander_pins(tmp_path):
    result = validate_pins(
        tmp_path,
        "{mcp23xxx: port_expander, number: 2}",
        "GPIO1",
        """i2c:
  sda: GPIO16
  scl: GPIO15
mcp23017:
  - id: port_expander
    address: 0x20
""",
    )
    assert result.returncode != 0, result.stdout + result.stderr


def test_uart_rejects_the_same_gpio_for_rx_and_tx(tmp_path):
    result = validate_pins(tmp_path, "GPIO2", "GPIO2")
    assert result.returncode != 0, result.stdout + result.stderr
