from pathlib import Path
import subprocess
import sys


SECPLUS_PACKAGE = Path("packages/secplus-gdo.yaml")
CORE_PACKAGE = Path("packages/core-esp32-s3.yaml")
SECPLUS_INIT = Path("components/secplus_gdo/__init__.py")
SECPLUS_COMPONENT = Path("components/secplus_gdo/secplus_gdo.cpp")
GDO_DOOR_COMPONENT = Path("components/secplus_gdo/cover/gdo_door.cpp")


def test_secplus_component_registers_pinned_gdolib_without_yaml_dependency(tmp_path):
    config_path = tmp_path / "dependency.yaml"
    config_path.write_text(
        f"""esphome:
  name: dependency-check
esp32:
  board: esp32-s3-devkitc-1
  framework:
    type: esp-idf
external_components:
  - source:
      type: local
      path: {SECPLUS_INIT.resolve().parents[1].as_posix()}
    components: [secplus_gdo]
secplus_gdo:
  input_gdo_pin: GPIO2
  output_gdo_pin: GPIO1
""",
        encoding="utf-8",
    )
    result = subprocess.run(
        [sys.executable, "-c", """
import sys
from pathlib import Path
from esphome.config import read_config
from esphome.__main__ import generate_cpp_contents
from esphome.core import CORE

CORE.config_path = Path(sys.argv[1])
config = read_config({})
assert config is not None
CORE.config = config
generate_cpp_contents(config)
assert "gdolib" in CORE.platformio_libraries, "secplus_gdo did not register gdolib"
assert CORE.platformio_libraries["gdolib"].as_lib_dep == (
    "gdolib=https://github.com/CircuitSetup/gdolib#v1.4.0"
)
""", str(config_path)],
        capture_output=True,
        text=True,
        check=False,
    )
    assert result.returncode == 0, result.stdout + result.stderr


def test_yaml_configs_do_not_pin_gdolib():
    for config_path in [*Path(".").glob("*.yaml"), *Path("packages").glob("*.yaml")]:
        assert "gdolib=" not in config_path.read_text(encoding="utf-8"), str(config_path)


def test_core_package_uses_native_esp32_flash_mode():
    source = CORE_PACKAGE.read_text(encoding="utf-8")

    assert "  flash_mode: dio" in source
    assert "board_build.flash_mode" not in source


def test_pre_close_warning_can_be_stopped_directly():
    source = SECPLUS_PACKAGE.read_text(encoding="utf-8")

    assert "pc_warn" not in source
    assert "      - button.press: pre_close_warning" in source
    assert "      - rtttl.stop" in source


def test_component_rejects_multiple_driver_instances():
    source = SECPLUS_INIT.read_text(encoding="utf-8")

    assert "MULTI_CONF = True" not in source


def test_legacy_panic_wrapper_flags_are_version_gated():
    source = SECPLUS_INIT.read_text(encoding="utf-8")

    assert "cv.Version.parse(ESPHOME_VERSION) < cv.Version(2026, 3, 0)" in source


def test_component_requires_esp32_with_esp_idf():
    source = SECPLUS_INIT.read_text(encoding="utf-8")

    assert 'DEPENDENCIES = ["esp32", "preferences"]' in source
    assert "cv.only_on_esp32" in source
    assert 'cv.only_with_framework("esp-idf")' in source


def test_resync_client_id_uses_uint32_hex_format_macro():
    source = SECPLUS_PACKAGE.read_text(encoding="utf-8")

    assert '0x%08" PRIX32 "' in source
    assert "0x%08X" not in source


def test_pre_close_warning_uses_uint32_format_macro():
    source = GDO_DOOR_COMPONENT.read_text(encoding="utf-8")

    assert '"WARNING for %" PRIu32 "ms"' in source
    assert '"WARNING for %dms"' not in source


def test_secplus_status_logs_use_sized_unsigned_format_macros():
    source = SECPLUS_COMPONENT.read_text(encoding="utf-8")

    assert '"Openings: %" PRIu16' in source
    assert '"Time to close: %" PRIu16' in source
    assert '"Paired devices: %" PRIu8 " remotes, %" PRIu8 " keypads, %" PRIu8 " wall controls, %" PRIu8' in source
    assert '"Open duration: %" PRIu16' in source
    assert '"Close duration: %" PRIu16' in source
