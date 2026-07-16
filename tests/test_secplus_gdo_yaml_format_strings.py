from pathlib import Path


SECPLUS_PACKAGE = Path("packages/secplus-gdo.yaml")


def test_resync_client_id_uses_uint32_hex_format_macro():
    source = SECPLUS_PACKAGE.read_text(encoding="utf-8")

    assert '0x%08" PRIX32 "' in source
    assert "0x%08X" not in source
