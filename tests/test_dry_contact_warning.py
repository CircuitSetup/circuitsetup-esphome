from pathlib import Path
from types import SimpleNamespace

import pytest
from esphome.yaml_util import load_yaml


COVER = Path(__file__).resolve().parents[1] / "packages/garage-door-cover.yaml"


def run_actions(actions, door, pulses):
    """Step the package's relay actions, yielding when its warning delay starts."""
    for action in actions:
        if "if" in action:
            branch = action["if"]
            condition = branch["condition"]
            if "lambda" in condition:
                expression = str(condition["lambda"]).removeprefix("return ").removesuffix(";")
                matches = eval(
                    expression.replace("id(garage_door).position", "position"),
                    {"__builtins__": {}},
                    {"position": door.position, "COVER_OPEN": 1.0, "COVER_CLOSED": 0.0},
                )
            elif "binary_sensor.is_on" in condition:
                assert condition["binary_sensor.is_on"] == "garage_door_input"
                matches = door.sensor_open
            else:
                raise AssertionError(f"Unhandled condition: {condition}")
            yield from run_actions(branch.get("then" if matches else "else", []), door, pulses)
        elif "delay" in action:
            yield "warning"
        elif "button.press" in action:
            if action["button.press"] == "garage_door_opener_button":
                pulses.append("relay")
        elif "cover.template.publish" in action:
            door.operation = action["cover.template.publish"]["current_operation"]
        elif "script.execute" not in action and "lambda" not in action:
            raise AssertionError(f"Unhandled action: {action}")


@pytest.mark.parametrize("command", ["close_action", "toggle_action"])
@pytest.mark.parametrize("closed_during_warning", [False, True])
def test_warned_close_does_not_pulse_a_door_that_closed_during_warning(command, closed_during_warning):
    door = SimpleNamespace(position=1.0, sensor_open=True, operation="IDLE")
    pulses = []
    config = load_yaml(COVER)
    actions = run_actions(config["cover"][0][command], door, pulses)
    assert next(actions) == "warning"
    assert pulses == []

    if closed_during_warning:
        door.position = 0.0
        door.sensor_open = False
    list(actions)

    assert pulses == ([] if closed_during_warning else ["relay"])
    assert door.operation == ("IDLE" if closed_during_warning else "CLOSING")


def test_toggle_opens_a_closed_door_without_a_warning_delay():
    door = SimpleNamespace(position=0.0, sensor_open=False, operation="IDLE")
    pulses = []
    config = load_yaml(COVER)
    assert list(run_actions(config["cover"][0]["toggle_action"], door, pulses)) == []
    assert pulses == ["relay"]
    assert door.operation == "OPENING"
