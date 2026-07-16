from pathlib import Path


SECPLUS_PACKAGE = Path("packages/secplus-gdo.yaml")
SECPLUS_COMPONENT = Path("components/secplus_gdo/secplus_gdo.cpp")
GDO_DOOR_COMPONENT = Path("components/secplus_gdo/cover/gdo_door.cpp")


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
