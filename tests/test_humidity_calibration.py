"""Compile and execute the actual humidity filter from the shipped package."""

from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

from esphome.yaml_util import load_yaml


ROOT = Path(__file__).resolve().parents[1]


@unittest.skipUnless(shutil.which("g++"), "g++ is required for the C++ host checks")
class HumidityCalibrationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory()
        cls.addClassCleanup(cls.temp.cleanup)
        config = load_yaml(ROOT / "packages/temp-sensor.yaml")
        body = str(config["sensor"][0]["humidity"]["filters"][0]["lambda"])
        body = body.replace("${dew_point_offset}", "1.3")
        source = """
#include <cassert>
#include <cmath>
#include <string>
#include <algorithm>
using std::isnan;
using std::min;
using std::max;
#define id(x) x
struct { float state = 68.0f; } garage_temp_sensor;
float calibrate(float x) {
""" + body + """
}
int main(int argc, char **argv) {
    assert(argc == 2);
    const std::string scenario = argv[1];
    if (scenario == "zero") assert(calibrate(0.0f) == 0.0f);
    if (scenario == "saturated") assert(calibrate(100.0f) == 100.0f);
    if (scenario == "normal") assert(std::abs(calibrate(50.0f) - 52.48275f) < 0.001f);
    if (scenario == "missing_humidity") assert(isnan(calibrate(NAN)));
    if (scenario == "missing_temperature") {
        garage_temp_sensor.state = NAN;
        assert(isnan(calibrate(50.0f)));
        assert(isnan(calibrate(0.0f)));
    }
}
"""
        cpp = Path(cls.temp.name) / "humidity.cpp"
        cpp.write_text(source, encoding="utf-8")
        cls.executable = Path(cls.temp.name) / "humidity"
        subprocess.run(["g++", "-std=c++17", str(cpp), "-o", str(cls.executable)], check=True)

    def test_humidity_boundaries_and_unavailable_states(self):
        for scenario in ("zero", "saturated", "normal", "missing_humidity", "missing_temperature"):
            with self.subTest(scenario=scenario):
                subprocess.run([str(self.executable), scenario], check=True)


if __name__ == "__main__":
    unittest.main()
