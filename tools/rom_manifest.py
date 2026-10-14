#!/usr/bin/env python3
"""Describe Splat's configured ROM inputs without extracting or modifying the ROM."""
from pathlib import Path
import argparse
import json
import re

import yaml

ROOT = Path(__file__).resolve().parent.parent


def manifest(config: dict) -> dict:
    records, byproducts = [], []
    segments = config["segments"]
    for index, segment in enumerate(segments):
        if not isinstance(segment, dict):
            continue
        kind, name = segment["type"], segment["name"]
        if kind == "header":
            records.append({"kind": "asm", "path": f"asm/{name}.s"})
        elif kind == "bin":
            records.append({"kind": "bin", "path": f"assets/{name}.bin"})
        elif kind == "code":
            following = segments[index + 1]
            end = following["start"] if isinstance(following, dict) else following[0]
            for sub in segment["subsegments"]:
                if isinstance(sub, list):
                    start, kind = sub[:2]
                    name = sub[2] if len(sub) > 2 else f"{start:X}"
                else:
                    start = sub.get("start", end)
                    kind = sub["type"]
                    name = sub.get("name", f"{start:X}")
                if kind in ("c", "hasm"):
                    suffix = "c" if kind == "c" else "s"
                    records.append({"kind": kind, "path": f"src/{name}.{suffix}"})
                    if kind == "c":
                        records.append({"kind": "reference", "path": f"asm/{name}.s"})
                elif kind == "asm":
                    records.append({"kind": "asm", "path": f"asm/{name}.s"})
                elif kind in ("data", "rodata", "bss"):
                    records.append({"kind": "asm", "path": f"asm/data/{name}.{kind}.s"})
                elif kind in ("textbin", "databin"):
                    records.append({"kind": "asm", "path": f"asm/data/{name}.s"})
                    byproducts.append(f"assets/{name}.{kind}.bin")
                elif kind.startswith("."):
                    # A named section can reference a data-only source with no
                    # corresponding text subsegment (e.g. libultra/os/thread).
                    if (ROOT / f"src/{name}.c").is_file():
                        records.append({"kind": "c", "path": f"src/{name}.c"})
                    elif (ROOT / f"src/{name}.s").is_file():
                        records.append({"kind": "hasm", "path": f"src/{name}.s"})
                    else:
                        raise FileNotFoundError(f"Configured section {kind} has no source: {name}")
                else:
                    raise ValueError(f"unmodeled Splat subsegment type: {kind}")
        else:
            raise ValueError(f"unmodeled Splat segment type: {kind}")
    records = list({record["path"]: record for record in records}.values())
    for record in records:
        if record["kind"] == "c":
            text = (ROOT / record["path"]).read_text()
            for folder, name in re.findall(r'INCLUDE_(?:ASM|RODATA)\s*\(\s*"([^"]+)"\s*,\s*(\w+)\s*\)', text):
                byproducts.append(f"{folder}/{name}.s")
    return {"sources": records, "byproducts": byproducts}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    args.output.write_text(json.dumps(manifest(yaml.safe_load((ROOT / "mariogolf64.yaml").read_text())), indent=2) + "\n")
