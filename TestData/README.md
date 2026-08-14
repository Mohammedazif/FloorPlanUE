# FloorPlanUE Test Corpus

70 DXF files, 5.8 MB, in four groups. Everything here is reproducible from the
scripts in `Tools/` — no file in this tree was hand-edited.

## Groups

| Directory | Files | Purpose |
|---|---|---|
| `Fixtures/` | 13 | Authored floor plans with independently derived correct answers. The golden set. |
| `Versions/` | 12 | DXF version coverage, R12 through R2018, ASCII and binary. |
| `Malformed/` | 36 | 17 real-world broken files plus 19 generated adversarial ones. |
| `RealWorld/` | 14 | Files written by different producers — dxflib, BricsCAD, Leica, AutoCAD. |

`RealWorld/expected.json` holds the measured answer for `wipeout_door.dxf`, the only genuine
architectural fragment in the corpus. Its numbers were read out of the file, not assumed,
and three of them are worth knowing before trusting any real drawing:

- **The declared unit is a lie.** `$INSUNITS` is 1 (inches), but a wall 3583 units long and
  100 units thick would be 91 m by 2.54 m under that reading. The geometry is millimetres,
  so a caller must be able to override the header.
- **A third wall convention.** Neither paired faces nor centrelines: each wall is its own
  closed rectangle, 3583.0139 × 100.0000, area 358301.385067 mm². The two runs sit 1893 mm
  apart and enclose nothing, so **zero rooms is the correct answer** for this fragment.
- **The file contains an exploded door** — 136 loose LINEs in model space. With no layer
  filter those outlines chain into closed rings and become phantom rooms that inflate the
  footprint. Filtering to `A.Wall_Internal` removes every one.

## Fixtures

Each fixture is a floor plan whose room areas and wall footprint were derived by
hand before generation, then confirmed independently by `verify_fixtures.py`.
Expected values live in `Fixtures/expected.json`.

Walls are drawn the way real architectural DXF draws them: two separate closed
faces on layer `A-WALL`, an interior face and an exterior face. Room area is the
net area of a face after subtracting the faces immediately contained within it.
Units are millimetres.

| Fixture | What it tests |
|---|---|
| `single_room.dxf` | Baseline. 5000×4000 interior, uniform 200 wall, 20.0 m². |
| `two_rooms_shared_wall.dxf` | A 150 shared partition and the T-junctions it creates. |
| `l_shaped_room.dxf` | Non-convex loop with a reflex corner on the outer face. |
| `room_in_room.dxf` | Containment. The 45.75 m² room area exists in no single face — it is 48.0 m² less the 2.25 m² shaft footprint. |
| `door_and_window.dxf` | Named blocks `DOOR_900` and `WIN_1200`. The door block encodes its swing as an arc. |
| `labeled_rooms.dxf` | Room naming from TEXT entities, plus one label outside every loop that must stay unassigned. |
| `varying_thickness.dxf` | Breaks the uniform-thickness assumption: 300 sides, 150 top and bottom. |
| `arc_wall.dxf` | A semicircular wall as an LWPOLYLINE bulge of 1.0. Area is exactly 20 m² plus π·2000²/2. |
| `single_room_r12.dxf` | The baseline as R12 POLYLINE — no LWPOLYLINE entity exists in R12. |
| `single_room_binary.dxf` | The baseline as binary DXF. Must produce an identical model. |
| `line_pair_room.dxf` | The baseline drawn as 8 loose LINE entities — the common real-world idiom. No closed polyline exists, so the loop assembler must chain them. |
| `line_pair_room_with_noise.dxf` | The same, plus a dimension line, a leader tick and a furniture rectangle on `A-ANNO`. Without layer filtering the furniture nests one level deeper and is silently charged against the room. |

| `single_line_two_rooms.dxf` | Walls as centrelines, not paired faces. Containment nesting cannot separate these rooms — only a planar arrangement can — and the divider forms two T-junctions that must split the outer edges. |

The last three carry no closed faces, so `verify_fixtures.py` skips them and the C++ loop
assembler and arrangement tests own their verification.

R12 carries no `$INSUNITS`. Files at that version have no unit metadata at all,
so scale for R12 input cannot come from the file.

## The adversarial finding

`verify_malformed.py` runs a mature reference parser over the 19 generated
adversarial files. Eight are rejected on structure. **Eleven are silently
accepted:**

- NaN X on the first wall vertex
- infinite Y on the first wall vertex
- coordinate of 1e30
- NaN mid-loop rather than at the head
- LWPOLYLINE declaring INT32_MAX vertices, supplying 4
- LWPOLYLINE declaring −1 vertices, supplying 4
- LWPOLYLINE declaring 2 vertices, supplying 4
- wall layer name of two million characters
- block that inserts itself
- two blocks that insert each other
- INSERT naming a block that has no definition

Every one of those eleven carries its corruption **on a wall LWPOLYLINE in the
ENTITIES section**, so it reaches the geometry stage. `verify_malformed.py`
asserts that landing rather than assuming it — an earlier generator targeted
group codes by first file-wide occurrence and silently mutated `$INSBASE`,
`$CLAYER`, and a CLASSES proxy flag instead, producing six files that looked
adversarial and tested nothing.

Structural corruption is the easy class — every parser catches a missing
`ENDSEC`. The dangerous class is the structurally valid file carrying
semantically impossible values, because it passes validation and reaches the
geometry stage intact. That is where bounds checks, cycle detection and
non-finite rejection have to live, and it is why those nine files matter more
than the eight that fail loudly.

For the accepted nine the contract is not "reject". It is: do not crash, do not
read out of bounds, do not allocate without a bound, and do not silently emit
wrong geometry.

## Regenerating

```
pip install ezdxf
python Tools/download_corpus.py
python Tools/generate_fixtures.py
python Tools/verify_fixtures.py
python Tools/generate_malformed.py
python Tools/audit_corpus.py
```

`verify_fixtures.py` must report 0 failures. `download_corpus.py` pulls from the
ezdxf and IxMilia.Dxf repositories.

## Provenance

Downloaded files come from the test suites of `mozman/ezdxf` and `ixmilia/dxf`.
Authored fixtures and adversarial files are generated locally by the scripts in
`Tools/`.
