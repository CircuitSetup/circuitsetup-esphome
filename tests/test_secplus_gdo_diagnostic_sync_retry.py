from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
CPP = ROOT / "components" / "secplus_gdo" / "secplus_gdo.cpp"
HEADER = ROOT / "components" / "secplus_gdo" / "secplus_gdo.h"


def test_accepted_rolling_code_retries_diagnostic_sync_without_advancing_code():
    cpp = CPP.read_text()
    header = HEADER.read_text()

    assert "void schedule_diagnostic_sync_retry();" in header
    assert "gdo->schedule_diagnostic_sync_retry();" in cpp
    assert "retrying diagnostic sync without advancing rolling code" in cpp
    assert "diagnostic_sync_retry" in cpp

    accepted_branch = cpp.split("if (rolling_code_accepted) {", 1)[1].split("} else {", 1)[0]
    assert "next_rolling_code_search_value" not in accepted_branch
