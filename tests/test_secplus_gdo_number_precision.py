from pathlib import Path


NUMBER_HEADER = Path("components/secplus_gdo/number/gdo_number.h")


def test_gdo_number_restores_exact_values_and_migrates_legacy_float_preferences():
    source = NUMBER_HEADER.read_text(encoding="utf-8")

    assert "make_entity_preference<double>()" in source
    assert "make_entity_preference<float>()" in source
    assert "std::function<esp_err_t(double)>" in source
    assert "apply_value_(value)" in source


def test_gdo_number_control_functions_keep_double_precision_until_uint32_cast():
    source = Path("components/secplus_gdo/secplus_gdo.cpp").read_text(encoding="utf-8")

    assert "[](double value) { return gdo_set_client_id(static_cast<uint32_t>(value)); }" in source
    assert "[](double value) { return gdo_set_rolling_code(static_cast<uint32_t>(value)); }" in source
