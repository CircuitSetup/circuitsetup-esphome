from pathlib import Path


SECPLUS_PACKAGE = Path("packages/secplus-gdo.yaml")
SECPLUS_CONFIG = Path("circuitsetup-secplus-garage-door-opener.yaml")
SECPLUS_INIT = Path("components/secplus_gdo/__init__.py")
SECPLUS_COMPONENT = Path("components/secplus_gdo/secplus_gdo.cpp")
GDO_DOOR_COMPONENT = Path("components/secplus_gdo/cover/gdo_door.cpp")


def test_secplus_config_owns_pinned_gdolib_dependency():
    component_source = SECPLUS_INIT.read_text(encoding="utf-8")
    config_source = SECPLUS_CONFIG.read_text(encoding="utf-8")

    assert (
        '"gdolib=https://github.com/CircuitSetup/gdolib#'
        '9141a2f1d1032e342aca0239b15d024226efbf31"'
    ) in config_source
    assert "cg.add_library" not in component_source


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


def test_gdolib_callback_defers_entity_updates_to_main_loop():
    source = SECPLUS_COMPONENT.read_text(encoding="utf-8")

    assert "static void process_gdo_event(" in source
    assert "void GDOComponent::defer_gdo_event(" in source
    assert "this->defer([this, status, event]()" in source
    assert "process_gdo_event(&status, event, this);" in source
    assert "gdo->defer_gdo_event(*status, event);" in source


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
