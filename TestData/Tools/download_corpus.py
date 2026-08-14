import sys
import urllib.error
import urllib.request
from pathlib import Path

EZDXF_RAW = "https://raw.githubusercontent.com/mozman/ezdxf/master/"
IXMILIA_RAW = "https://raw.githubusercontent.com/ixmilia/dxf/main/"

VERSIONS = [
    "examples_dxf/Minimal_DXF_AC1006.dxf",
    "examples_dxf/Minimal_DXF_AC1009.dxf",
    "examples_dxf/Minimal_DXF_AC1021.dxf",
    "integration_tests/data/AC1003_LINE_Example.dxf",
    "integration_tests/data/ASCII_R12.dxf",
    "integration_tests/data/small_r13.dxf",
    "integration_tests/data/small_r14.dxf",
    "integration_tests/data/bin_dxf_r12.dxf",
    "integration_tests/data/bin_dxf_r13.dxf",
    "integration_tests/data/bin_dxf_r14.dxf",
    "integration_tests/data/bin_dxf_r2000.dxf",
]

MALFORMED = [
    "integration_tests/data/R12_with_trash_beyond_EOF.dxf",
    "integration_tests/data/duplicate_handles.dxf",
    "integration_tests/data/empty_handles.dxf",
    "integration_tests/data/layout_broken_links.dxf",
    "integration_tests/data/layout_broken_links_2.dxf",
    "integration_tests/data/layout_missing_block_definition.dxf",
    "integration_tests/data/layout_missing_block_record.dxf",
    "integration_tests/data/r12_blocks_with_no_names.dxf",
    "integration_tests/data/r2000_blocks_with_no_names.dxf",
    "integration_tests/data/r2000_blocks_with_no_name_and_no_block_record.dxf",
    "integration_tests/data/recover01.dxf",
    "integration_tests/data/recover02.dxf",
    "integration_tests/data/no_layouts.dxf",
    "integration_tests/data/dxf_unicode.dxf",
    "examples_dxf/caret_encoding.dxf",
    "examples_dxf/circle_radius_le_0.dxf",
    "examples_dxf/uncommon.dxf",
]

REAL_WORLD = [
    "integration_tests/data/MODEL.dxf",
    "integration_tests/data/POLI-ALL210_12.DXF",
    "integration_tests/data/cc_dxflib.dxf",
    "integration_tests/data/Leica_Disto_S910.dxf",
    "integration_tests/data/custom_blocks.dxf",
    "examples_dxf/multi_insert_with_attribs.dxf",
    "examples_dxf/dimension_in_block.dxf",
    "examples_dxf/dimension_in_nested_blocks.dxf",
    "examples_dxf/insert_bricscad_level_1.dxf",
    "examples_dxf/hatches_1.dxf",
    "examples_dxf/wipeout_door.dxf",
    "examples/edgeminer/1_polylines.dxf",
    "examples/edgeminer/6_closed_loop_with_arcs.dxf",
    "tests/test_01_dxf_entities/houses_of_parliament_georeferenced.dxf",
]

EXTRA = [(IXMILIA_RAW, "src/IxMilia.Dxf.Test/diamond-bin.dxf", "Versions")]


def fetch(url, destination):
    request = urllib.request.Request(url, headers={"User-Agent": "floorplanue-corpus"})
    with urllib.request.urlopen(request, timeout=60) as response:
        destination.write_bytes(response.read())
    return destination.stat().st_size


def download_group(base, paths, target_dir):
    ok = 0
    for path in paths:
        destination = target_dir / Path(path).name
        try:
            size = fetch(base + path, destination)
        except urllib.error.HTTPError as error:
            print(f"  FAIL {Path(path).name}: HTTP {error.code}")
            continue
        except urllib.error.URLError as error:
            print(f"  FAIL {Path(path).name}: {error.reason}")
            continue
        print(f"  {destination.name}  {size:,} bytes")
        ok += 1
    return ok, len(paths)


def main():
    root = Path(__file__).resolve().parent.parent
    groups = [
        ("Versions", EZDXF_RAW, VERSIONS),
        ("Malformed", EZDXF_RAW, MALFORMED),
        ("RealWorld", EZDXF_RAW, REAL_WORLD),
    ]
    total_ok = 0
    total = 0
    for name, base, paths in groups:
        print(f"\n{name}")
        target = root / name
        target.mkdir(parents=True, exist_ok=True)
        ok, count = download_group(base, paths, target)
        total_ok += ok
        total += count

    print("\nExtra")
    for base, path, group in EXTRA:
        target = root / group
        destination = target / Path(path).name
        try:
            size = fetch(base + path, destination)
            print(f"  {destination.name}  {size:,} bytes")
            total_ok += 1
        except (urllib.error.HTTPError, urllib.error.URLError) as error:
            print(f"  FAIL {Path(path).name}: {error}")
        total += 1

    print(f"\n{total_ok}/{total} downloaded")
    return 0 if total_ok == total else 1


if __name__ == "__main__":
    sys.exit(main())
