"""Run the real cover implementation with host substitutes for ESPHome and gdolib."""

from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]


class CoverBehaviorTest(unittest.TestCase):
    def test_cover_commands(self):
        compiler = shutil.which("g++")
        if compiler is None:
            self.skipTest("g++ is required for the host C++ tests")

        source = (ROOT / "tests/cover_stubs.h").read_text(encoding="utf-8")
        for name in ("automation.h", "gdo_door.h", "gdo_door.cpp"):
            source += "\n".join(
                line
                for line in (ROOT / "components/secplus_gdo/cover" / name)
                .read_text(encoding="utf-8")
                .splitlines()
                if not line.startswith(("#include", "#pragma once"))
            ) + "\n"
        source += (ROOT / "tests/cover_cases.cpp").read_text(encoding="utf-8")

        with tempfile.TemporaryDirectory() as temporary:
            cpp = Path(temporary) / "cover.cpp"
            executable = Path(temporary) / "cover"
            cpp.write_text(source, encoding="utf-8")
            for flags in ([], ["-DUSE_MQTT"]):
                with self.subTest(flags=flags):
                    subprocess.run(
                        [compiler, "-std=c++17", *flags, str(cpp), "-o", str(executable)],
                        check=True,
                    )
                    subprocess.run([str(executable)], check=True)
