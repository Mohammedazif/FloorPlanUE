import json
import sys
from pathlib import Path

import ezdxf

TARGETS = {
    "nan_coordinate.dxf": ("LWPOLYLINE", 10, "nan"),
    "infinite_coordinate.dxf": ("LWPOLYLINE", 20, "1e400"),
    "absurd_coordinate.dxf": ("LWPOLYLINE", 10, "1.0e30"),
    "nan_coordinate_second_vertex.dxf": ("LWPOLYLINE", 20, "nan"),
    "huge_vertex_count.dxf": ("LWPOLYLINE", 90, "2147483647"),
    "negative_vertex_count.dxf": ("LWPOLYLINE", 90, "-1"),
    "understated_vertex_count.dxf": ("LWPOLYLINE", 90, "2"),
    "enormous_string_value.dxf": ("LWPOLYLINE", 8, "A" * 64),
}


def entities_tags(path):
    lines = path.read_text(encoding="utf-8", errors="replace").splitlines()
    start = -1
    for index in range(len(lines) - 1):
        if lines[index].strip() == "2" and lines[index + 1].strip() == "ENTITIES":
            start = index + 2
            break
    if start < 0:
        return []
    tags = []
    current = None
    index = start
    while index + 1 < len(lines):
        code = lines[index].strip()
        value = lines[index + 1].strip()
        if code == "0":
            if value == "ENDSEC":
                break
            current = value
        elif current:
            tags.append((current, code, value))
        index += 2
    return tags


def probe(path):
    try:
        ezdxf.readfile(str(path))
    except Exception as error:
        return f"rejected  {type(error).__name__}"
    return "ACCEPTED"


def main():
    root = Path(__file__).resolve().parent.parent / "Malformed" / "Generated"
    manifest = json.loads((root / "expected.json").read_text(encoding="utf-8"))

    failures = 0
    print("MUTATION LANDING CHECK")
    for name, (entity, code, expected) in TARGETS.items():
        path = root / name
        if not path.exists():
            print(f"  MISSING  {name}")
            failures += 1
            continue
        hits = [
            value
            for (etype, ecode, value) in entities_tags(path)
            if etype == entity and ecode == str(code)
        ]
        landed = any(value.startswith(expected) for value in hits)
        status = "OK  " if landed else "FAIL"
        if not landed:
            failures += 1
        shown = hits[0][:40] if hits else "(no such tag)"
        print(
            f"  {status} {name:<36} {entity} group {code} = {shown!r}  "
            f"[{probe(path)}]"
        )

    print("\nREFERENCE PARSER OUTCOME, all files")
    for name in manifest["files"]:
        path = root / name
        if path.exists():
            print(f"  {probe(path):<22} {name}")

    print(f"\n{failures} landing failure(s)")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
