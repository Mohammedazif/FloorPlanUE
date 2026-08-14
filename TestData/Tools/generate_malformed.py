import json
import sys
from pathlib import Path

INT32_MAX = 2_147_483_647


def entities_section_start(lines):
    for index in range(len(lines) - 1):
        if lines[index].strip() == "2" and lines[index + 1].strip() == "ENTITIES":
            return index + 2
    return -1


def find_entity_tag_line(lines, entity_type, group_code, skip=0):
    start = entities_section_start(lines)
    if start < 0:
        return -1
    current = None
    seen = 0
    index = start
    while index + 1 < len(lines):
        code = lines[index].strip()
        value = lines[index + 1].strip()
        if code == "0":
            if value == "ENDSEC":
                return -1
            current = value
        elif current == entity_type and code == str(group_code):
            if seen == skip:
                return index + 1
            seen += 1
        index += 2
    return -1


def mutate_entity_tag(text, entity_type, group_code, replacement, skip=0):
    lines = text.splitlines(keepends=True)
    index = find_entity_tag_line(lines, entity_type, group_code, skip)
    if index < 0:
        raise LookupError(
            f"{entity_type} group {group_code} occurrence {skip} not found in ENTITIES"
        )
    lines[index] = replacement + "\n"
    return "".join(lines)


def mutate_entity_code(text, entity_type, group_code, replacement):
    lines = text.splitlines(keepends=True)
    index = find_entity_tag_line(lines, entity_type, group_code)
    if index < 1:
        raise LookupError(f"{entity_type} group {group_code} not found in ENTITIES")
    lines[index - 1] = replacement + "\n"
    return "".join(lines)


def empty(text):
    return ""


def truncate_at_20_bytes(text):
    return text[:20]


def truncate_half(text):
    return text[: len(text) // 2]


def truncate_mid_entity(text):
    lines = text.splitlines(keepends=True)
    index = find_entity_tag_line(lines, "LWPOLYLINE", 10)
    if index < 0:
        raise LookupError("no LWPOLYLINE vertex found in ENTITIES")
    return "".join(lines[: index + 3])


def strip_eof(text):
    return text.replace("  0\nEOF\n", "").replace("0\nEOF", "")


def not_dxf(text):
    return "This is a plain text file and not a DXF document at all.\n" * 50


def nan_coordinate(text):
    return mutate_entity_tag(text, "LWPOLYLINE", 10, "nan")


def infinite_coordinate(text):
    return mutate_entity_tag(text, "LWPOLYLINE", 20, "1e400")


def absurd_coordinate(text):
    return mutate_entity_tag(text, "LWPOLYLINE", 10, "1.0e30")


def nan_coordinate_second_vertex(text):
    return mutate_entity_tag(text, "LWPOLYLINE", 20, "nan", skip=2)


def huge_vertex_count(text):
    return mutate_entity_tag(text, "LWPOLYLINE", 90, str(INT32_MAX))


def negative_vertex_count(text):
    return mutate_entity_tag(text, "LWPOLYLINE", 90, "-1")


def understated_vertex_count(text):
    return mutate_entity_tag(text, "LWPOLYLINE", 90, "2")


def non_numeric_group_code(text):
    return mutate_entity_code(text, "LWPOLYLINE", 10, "NOTACODE")


def enormous_string_value(text):
    return mutate_entity_tag(text, "LWPOLYLINE", 8, "A" * 2_000_000)


def self_referencing_block(text):
    block = (
        "  0\nBLOCK\n  2\nCYCLE_A\n 70\n0\n 10\n0.0\n 20\n0.0\n 30\n0.0\n"
        "  3\nCYCLE_A\n"
        "  0\nINSERT\n  8\n0\n  2\nCYCLE_A\n 10\n0.0\n 20\n0.0\n 30\n0.0\n"
        "  0\nENDBLK\n  8\n0\n"
    )
    return insert_into_blocks_section(text, block)


def mutual_block_cycle(text):
    blocks = (
        "  0\nBLOCK\n  2\nCYCLE_A\n 70\n0\n 10\n0.0\n 20\n0.0\n 30\n0.0\n"
        "  3\nCYCLE_A\n"
        "  0\nINSERT\n  8\n0\n  2\nCYCLE_B\n 10\n0.0\n 20\n0.0\n 30\n0.0\n"
        "  0\nENDBLK\n  8\n0\n"
        "  0\nBLOCK\n  2\nCYCLE_B\n 70\n0\n 10\n0.0\n 20\n0.0\n 30\n0.0\n"
        "  3\nCYCLE_B\n"
        "  0\nINSERT\n  8\n0\n  2\nCYCLE_A\n 10\n0.0\n 20\n0.0\n 30\n0.0\n"
        "  0\nENDBLK\n  8\n0\n"
    )
    return insert_into_blocks_section(text, blocks)


def insert_into_blocks_section(text, payload):
    anchor = text.find("BLOCKS")
    if anchor < 0:
        return text
    end_of_line = text.find("\n", anchor) + 1
    return text[:end_of_line] + payload + text[end_of_line:]


def dangling_block_reference(text):
    lines = text.splitlines(keepends=True)
    entities = text.find("ENTITIES")
    if entities < 0:
        return text
    end_of_line = text.find("\n", entities) + 1
    reference = (
        "  0\nINSERT\n  8\nA-DOOR\n  2\nBLOCK_THAT_DOES_NOT_EXIST\n"
        " 10\n1000.0\n 20\n0.0\n 30\n0.0\n"
    )
    return text[:end_of_line] + reference + text[end_of_line:]


CORRUPTIONS = [
    ("empty.dxf", empty, "Zero-byte file."),
    ("truncate_at_20_bytes.dxf", truncate_at_20_bytes, "Truncated inside the header."),
    ("truncate_half.dxf", truncate_half, "Truncated at 50% of the byte length."),
    ("truncate_mid_entity.dxf", truncate_mid_entity, "Truncated inside the first wall LWPOLYLINE's vertex list, in the ENTITIES section."),
    ("no_eof.dxf", strip_eof, "Valid content with the EOF marker removed."),
    ("not_dxf.dxf", not_dxf, "Plain text with no DXF structure."),
    ("nan_coordinate.dxf", nan_coordinate, "Vertex 0 X of the first wall LWPOLYLINE is nan."),
    ("infinite_coordinate.dxf", infinite_coordinate, "Vertex 0 Y of the first wall LWPOLYLINE overflows to infinity."),
    ("absurd_coordinate.dxf", absurd_coordinate, "Vertex 0 X of the first wall LWPOLYLINE is 1e30, beyond any sane extent."),
    ("nan_coordinate_second_vertex.dxf", nan_coordinate_second_vertex, "Vertex 2 Y of the first wall LWPOLYLINE is nan, mid-loop rather than at the head."),
    ("huge_vertex_count.dxf", huge_vertex_count, "First wall LWPOLYLINE declares INT32_MAX vertices and supplies 4."),
    ("negative_vertex_count.dxf", negative_vertex_count, "First wall LWPOLYLINE declares -1 vertices and supplies 4."),
    ("understated_vertex_count.dxf", understated_vertex_count, "First wall LWPOLYLINE declares 2 vertices and supplies 4."),
    ("non_numeric_group_code.dxf", non_numeric_group_code, "The group code preceding a wall vertex X is not an integer."),
    ("enormous_string_value.dxf", enormous_string_value, "The first wall LWPOLYLINE's layer name is two million characters."),
    ("self_referencing_block.dxf", self_referencing_block, "BLOCK CYCLE_A inserts itself."),
    ("mutual_block_cycle.dxf", mutual_block_cycle, "CYCLE_A inserts CYCLE_B which inserts CYCLE_A."),
    ("dangling_block_reference.dxf", dangling_block_reference, "INSERT names a block with no definition."),
]


def main():
    root = Path(__file__).resolve().parent.parent
    source = root / "Fixtures" / "single_room.dxf"
    if not source.exists():
        print(f"Source fixture missing: {source}")
        return 1

    target = root / "Malformed" / "Generated"
    target.mkdir(parents=True, exist_ok=True)
    text = source.read_text(encoding="utf-8")

    manifest = {}
    for name, corrupt, reason in CORRUPTIONS:
        path = target / name
        path.write_text(corrupt(text), encoding="utf-8", newline="")
        size = path.stat().st_size
        manifest[name] = {
            "reason": reason,
            "bytes": size,
            "must_fail_cleanly": True,
        }
        print(f"  {name}  {size:,} bytes")

    binary_source = root / "Fixtures" / "single_room_binary.dxf"
    if binary_source.exists():
        raw = binary_source.read_bytes()
        path = target / "binary_truncated.dxf"
        path.write_bytes(raw[: len(raw) // 3])
        manifest["binary_truncated.dxf"] = {
            "reason": "Binary DXF truncated to one third of its length.",
            "bytes": path.stat().st_size,
            "must_fail_cleanly": True,
        }
        print(f"  binary_truncated.dxf  {path.stat().st_size:,} bytes")

    expected = target / "expected.json"
    expected.write_text(
        json.dumps(
            {
                "source": "Fixtures/single_room.dxf",
                "contract": "Every file must produce a typed diagnostic. No crash, no out-of-bounds read, no unbounded allocation.",
                "files": manifest,
            },
            indent=2,
        ),
        encoding="utf-8",
    )
    print(f"  expected.json  {expected.stat().st_size:,} bytes")
    return 0


if __name__ == "__main__":
    sys.exit(main())
