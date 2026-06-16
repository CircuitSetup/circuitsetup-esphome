from pathlib import Path


SECPLUS_CPP = Path("components/secplus_gdo/secplus_gdo.cpp")
SECPLUS_HEADER = Path("components/secplus_gdo/secplus_gdo.h")


def test_component_releases_uart_tx_pin_to_safe_state_after_driver_teardown():
    source = SECPLUS_CPP.read_text(encoding="utf-8")

    assert "void GDOComponent::release_uart_tx_pin_to_safe_state_()" in source
    assert "gpio_set_direction((gpio_num_t) GDO_UART_TX_PIN, GPIO_MODE_INPUT);" in source
    assert "gpio_pulldown_en((gpio_num_t) GDO_UART_TX_PIN);" in source
    assert "void GDOComponent::on_shutdown()" in source
    assert "this->release_uart_tx_pin_to_safe_state_();" in source
    assert "void GDOComponent::restart_driver_for_diagnostic_sync_()" in source


def test_component_declares_safe_state_helper_for_uart_tx_pin():
    source = SECPLUS_HEADER.read_text(encoding="utf-8")

    assert "void release_uart_tx_pin_to_safe_state_();" in source
