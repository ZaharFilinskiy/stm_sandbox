"""Run portable settings/CLI tests with a host GCC, without hardware access."""
from pathlib import Path
import os
import subprocess

root = Path(__file__).resolve().parent.parent
output = root / "build" / "host-tests"
output.mkdir(parents=True, exist_ok=True)
suffix = ".exe" if os.name == "nt" else ""
tests = {
    "settings_test": (["-ICore/Inc"], ["tests/settings_test.c", "Core/Src/settings.c"]),
    "cli_test": (["-Itests/stubs", "-ICore/Inc", "-ICore/embedcli"],
                 ["tests/cli_test.c", "Core/Src/app_cli.c", "Core/Src/settings.c",
                  "Core/embedcli/embedded_cli.c"]),
}
for name, (includes, sources) in tests.items():
    binary = output / (name + suffix)
    subprocess.run(["gcc", "-std=c11", "-Wall", "-Wextra", "-Werror",
                    *includes, *sources, "-o", str(binary)], cwd=root, check=True)
    subprocess.run([str(binary)], cwd=root, check=True)
