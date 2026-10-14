from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parent
sys.path.insert(0, str(ROOT / "tools"))
from rom_build import build_dir, python_path, tool_path


def apply(config, args):
    binary = build_dir()
    config["mapfile"] = str(binary / "mariogolf64.map")
    config["myimg"] = str(binary / "mariogolf64.z64")
    config["baseimg"] = str(ROOT / "baserom.z64")
    config["arch"] = "mips"
    config["make_command"] = [
        str(python_path()),
        str(ROOT / "tools" / "rom_build.py"),
        "build",
    ]
    config["build_dir"] = str(binary)
    config["objdump_executable"] = tool_path("objdump")
