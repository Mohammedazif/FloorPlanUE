import json
import sys
from pathlib import Path

import ezdxf


def inventory(root):
    rows = []
    for directory in sorted(p for p in root.iterdir() if p.is_dir()):
        if directory.name == "Tools":
            continue
        files = sorted(
            {p.resolve() for p in directory.rglob("*") if p.suffix.lower() == ".dxf"}
        )
        total = sum(f.stat().st_size for f in files)
        rows.append((directory.name, len(files), total))
    return rows


def probe(path):
    try:
        ezdxf.readfile(str(path))
    except Exception as error:
        return type(error).__name__, str(error)[:90]
    return None, None


def main():
    root = Path(__file__).resolve().parent.parent

    print("INVENTORY")
    grand_files = 0
    grand_bytes = 0
    for name, count, total in inventory(root):
        print(f"  {name:<12} {count:>4} files  {total:>12,} bytes")
        grand_files += count
        grand_bytes += total
    print(f"  {'TOTAL':<12} {grand_files:>4} files  {grand_bytes:>12,} bytes")

    generated = root / "Malformed" / "Generated"
    manifest_path = generated / "expected.json"
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))

    print("\nADVERSARIAL CORPUS — reference parser behaviour")
    accepted = []
    for name in manifest["files"]:
        path = generated / name
        if not path.exists():
            print(f"  MISSING   {name}")
            continue
        kind, message = probe(path)
        if kind is None:
            print(f"  ACCEPTED  {name}")
            accepted.append(name)
        else:
            print(f"  rejected  {name:<32} {kind}: {message}")

    if accepted:
        print(
            f"\n{len(accepted)} file(s) accepted by the reference parser. "
            "These are tolerated-malformed, not invalid:"
        )
        for name in accepted:
            print(f"  {name} — {manifest['files'][name]['reason']}")
        print(
            "\nFor these, the contract is not 'reject' but 'do not crash and do not "
            "silently produce wrong geometry'."
        )

    return 0


if __name__ == "__main__":
    sys.exit(main())
