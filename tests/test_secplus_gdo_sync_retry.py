from pathlib import Path


SECPLUS_CPP = Path("components/secplus_gdo/secplus_gdo.cpp")
SECPLUS_HEADER = Path("components/secplus_gdo/secplus_gdo.h")


def test_unsynced_callback_with_opener_status_does_not_advance_rolling_code():
    source = SECPLUS_CPP.read_text(encoding="utf-8")

    assert "const bool has_opener_status = status->door != GDO_DOOR_STATE_UNKNOWN;" in source
    assert "if (has_opener_status || gdo->is_sync_state())" in source
    assert "Skipping rolling-code advance" in source


def test_component_exposes_last_sync_state_to_callback():
    source = SECPLUS_HEADER.read_text(encoding="utf-8")

    assert "bool is_sync_state() const { return this->status_.synced; }" in source
