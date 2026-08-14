# FloorPlanUE — Build Specification

## WHAT TO BUILD

An Unreal Engine 5 plugin that reads a 2D architectural floor plan in DXF and
compiles it into a parametric 3D building inside Unreal, with no manual drawing
step and no AI. Every room and wall becomes a distinct object carrying a stable
identity and a measured floor area.

Input: DXF (ASCII and binary DXF). Nothing else in v1.
Output: per-room and per-wall actors with generated geometry, stable IDs,
computed areas, and openings cut as real boolean subtractions.

The distinguishing property of this plugin is IDENTITY. A generated building
whose elements have no stable, reproducible identifier is a picture, not data.
Treat identity as a first-class requirement, not a feature.

## ARCHITECTURE — NON-NEGOTIABLE

Two layers with a hard boundary between them.

    Source/
      FloorPlanCore/          pure C++, ZERO Unreal dependency
        Dxf/                  lexer, group-code parser, entity model
        Geometry/             segments, polylines, planar arrangement, half-edge
        Walls/                parallel-pair detection, centerlines, junctions
        Rooms/                loop finding, area, label association
        Openings/             block reference to opening mapping
        Model/                BuildingModel, identity derivation
        FloorPlanLimits.h     every limit, budget and tolerance in the codebase
      FloorPlanUnreal/        Unreal module — the only layer that knows about UE
        Import/               asset import path
        Build/                Geometry Script mesh generation
        Actors/               room and wall actors, selection, inspection
      Tests/                  unit tests for FloorPlanCore only

FloorPlanCore must not include a single Unreal header. No FString, no TArray,
no UObject, no FVector, no UE_LOG. Standard library only. It must compile and
its tests must run without Unreal present. This is what makes the geometry
core testable and the whole thing debuggable.

FloorPlanUnreal converts core types to Unreal types at the boundary and does
nothing else of substance. No geometry algorithms live in this layer.

## TEST CORPUS — ALREADY BUILT, USE IT

`TestData/` holds 70 DXF files in four groups. Do not create test data; consume
this. `TestData/README.md` documents it in full.

    Fixtures/    10  authored plans with independently derived correct answers
    Versions/    12  R12 through R2018, ASCII and binary
    Malformed/   34  17 real-world broken files, 17 generated adversarial
    RealWorld/   14  written by dxflib, BricsCAD, Leica, AutoCAD

`Fixtures/expected.json` carries two independent golden invariants per fixture:
room areas and total wall footprint area. Check both. The wall footprint catches
thickness errors that correct-looking room areas hide.

Fixtures draw walls the way real architectural DXF does: two separate closed
faces on layer `A-WALL`, an interior face and an exterior face. Units are
millimetres. The `RealWorld/` files will violate this tidiness — that is their
purpose.

Regenerate with the scripts in `TestData/Tools/`. `verify_fixtures.py` must
report 0 failures.

## SECURITY — DXF IS UNTRUSTED INPUT

A DXF arriving from outside is an attacker-controlled byte stream. The parser
is the attack surface.

The corpus audit established where that surface actually is. Of 17 adversarial
files, a mature reference parser rejects 8 on structure and **silently accepts
9**: NaN coordinate, infinite coordinate, coordinate of 1e30, LWPOLYLINE
declaring INT32_MAX vertices, LWPOLYLINE declaring −1 vertices, layer name of
two million characters, block that inserts itself, two blocks that insert each
other, and INSERT naming a block with no definition.

Structural corruption is the easy class — every parser catches a missing
`ENDSEC`. **The dangerous class is the structurally valid file carrying
semantically impossible values**, because it passes structural validation and
arrives at the geometry stage intact. Those nine files matter more than the
eight that fail loudly, and they are where the defenses below have to live.

Required defenses, each enforced by a named constant in FloorPlanLimits.h:
  - Total file size bound; refuse before allocating.
  - Maximum entity count. Never allocate based on a count declared in the file;
    bound by bytes actually read. A declared count of INT32_MAX or −1 must not
    reach an allocation.
  - Maximum vertices per polyline.
  - Maximum block-reference recursion depth, and cycle detection. A BLOCK that
    INSERTs itself, or a cycle across several blocks, must terminate — detect
    the cycle, do not rely on the depth limit alone.
  - Maximum group-code line length, and maximum string value length.
  - Coordinate magnitude bound; reject NaN and infinity on every coordinate
    read, at the point of read, before the value enters any geometry type.
  - Dangling block references resolve to a diagnostic, never a null dereference.
  - Maximum wall, room and opening counts in the produced model.
  - Bounded arrangement complexity — intersection counts grow quadratically;
    cap and fail cleanly rather than hanging the editor.

Rules:
  - Truncated, malformed, or partial files must produce a clean typed error,
    never a crash, never an out-of-bounds read, never unbounded allocation.
  - No exception may cross the module boundary. Core returns an explicit
    result type carrying either a model or a diagnostic.
  - Every failure identifies what was wrong and where — byte offset or group
    code index — so a real user with a real broken file can act on it.
  - Parsing must be deterministic. Same bytes in, same model out, same IDs,
    every run, on every platform.

For the nine accepted files the contract is not "reject". It is: do not crash,
do not read out of bounds, do not allocate without a bound, and do not silently
emit wrong geometry. Each of the seventeen has a test.

## IDENTITY — GET THIS RIGHT OR THE PROJECT HAS NO POINT

Each wall, room and opening carries an identifier derived deterministically
from its geometry and type. Requirements:
  - Quantize coordinates to a fixed grid before hashing. Raw floats make IDs
    unstable across trivial recomputation; this is the single most likely way
    to get identity silently wrong.
  - The hash function and quantization step are named constants.
  - Reopening the same DXF produces byte-identical IDs.
  - Collisions must be detected and reported, not silently tolerated.
  - Document the derivation once, in the plugin documentation, not in comments
    scattered through the code.

## PIPELINE — IMPLEMENT IN THIS ORDER

Each milestone ends with passing tests and an audit gate. Do not start the next
milestone until the current one is clean.

**M1 — DXF lexer and group-code parser.** Entity model for LINE, LWPOLYLINE,
POLYLINE, ARC, CIRCLE, INSERT, BLOCK, TEXT, MTEXT, layers. Both ASCII and
binary DXF, R12 through R2018.

LWPOLYLINE carries a bulge value per vertex and it is load-bearing. A parser
that reads vertices and drops bulges produces a valid closed loop with a wrong
area and reports no error — `Fixtures/arc_wall.dxf` yields 20.0 m² instead of
26.283185 m² under that bug. Bulges are not an optional refinement; treat a
dropped bulge as a correctness defect. R12 has no LWPOLYLINE at all, so POLYLINE
with its own vertex entities is a separate path, not a fallback.

All limits enforced. Malformed suite passing.

**M2 — Geometry primitives.** Segment, polyline, tolerance-aware comparison,
collinear merge, arc and bulge evaluation, planar arrangement with half-edge
traversal.

**M3 — Wall extraction.** Detect parallel face pairs within a configurable gap
tolerance, project helper lines, split at intersections, produce a wall
centerline graph with thickness per wall. Resolve junctions and T-intersections
into a clean topological graph. Wall thickness varies within one building —
`Fixtures/varying_thickness.dxf` exists to break any uniform-thickness
assumption.

**M4 — Room extraction.** Closed-loop finding over the arrangement, signed
area, containment, association of TEXT/MTEXT entities to the loop that contains
them for room naming.

Room area is defined as the net area of a face after subtracting the areas of
the faces immediately contained within it. Only immediate containment counts —
a shaft's interior belongs to the shaft, not to the room the shaft sits in.
`Fixtures/room_in_room.dxf` has an expected room area of 45.75 m² that appears
in no single face; it is 48.0 less the 2.25 m² shaft footprint, and it passes
only if this subtraction is correct. A TEXT entity that falls inside no loop
must remain unassigned rather than attach to the nearest room.

**M5 — Openings.** Map INSERT block references to doors and windows by block
name, with a user-editable name mapping. Derive host wall, position along wall,
width, and where the block geometry allows it, swing direction and hinge side.
Sill and head heights come from named defaults per opening type.

**M6 — Identity and the building model.** Stable IDs, areas, the complete
immutable model that the Unreal layer consumes.

**M7 — Unreal geometry generation.** Extrude walls via Geometry Script,
boolean-subtract openings, generate floor and ceiling surfaces per room.
Watertight output; verify it.

**M8 — Actors and inspection.** One actor per room and per wall. Selecting a
room surfaces its name, area and ID. Multi-storey placement by level.

**M9 — Full audit pass** across the entire codebase.

## CODE QUALITY — ENFORCED, NOT ASPIRATIONAL

Structure
  - One public type per file. File name matches the type.
  - Files under ~400 lines. Past that, split by responsibility.
  - No magic numbers anywhere. Every limit, budget, tolerance and threshold is
    a named constant in FloorPlanLimits.h, named so its purpose needs no
    explanation.
  - No dead code. No commented-out code. No speculative abstractions built for
    a future that has not arrived.

Comments — the default is zero
  - Code explains itself through names and structure. A file with no comments
    is the expected outcome, not a deficiency.
  - Never write a comment that restates what the code does, narrates what
    changed, introduces the next line, or divides a file into sections.
    Banner and divider comments are forbidden.
  - No paragraph comments. No multi-line block comments. Anything needing a
    long explanation belongs in the plugin documentation, not in source.
  - A comment is permitted only for a constraint the code cannot express — a
    DXF format quirk, a hostile-input defense, a numerical-robustness caveat —
    and must be a single short line.
  - Doc comments: a one-line summary on public types, and on public functions
    whose behavior is not obvious from the signature. Nothing else. No
    parameter or return boilerplate. No doc comments on self-evident accessors.
  - No TODO comments.

Naming
  - FloorPlanCore uses plain C++ conventions: PascalCase types, camelCase
    locals and parameters, trailing-underscore or m_ private members — pick one
    and hold it everywhere.
  - FloorPlanUnreal follows Unreal conventions exactly: F/U/A/E prefixes,
    PascalCase members, Unreal container and string types.
  - Do not leak either convention across the boundary.

Correctness
  - Tolerance comparisons everywhere floats meet geometry. No raw == on
    coordinates.
  - Every tolerance is named and lives in FloorPlanLimits.h.
  - Prefer exact predicates or robust orientation tests where degeneracies
    matter — junctions and loop finding are where naive floating point fails.

## TESTS

  - FloorPlanCore has unit tests and they run without Unreal.
  - Every milestone's algorithms have tests before the milestone is called done.
  - The malformed suite is part of the test suite, not a side script. All 17
    generated adversarial files and all 17 real-world broken files have tests.
  - Golden tests assert both invariants from `Fixtures/expected.json`: room
    areas and total wall footprint.
  - Encoding-independence test: `single_room.dxf` and `single_room_binary.dxf`
    are the same model in ASCII and binary. They must produce identical models
    and identical IDs. This is a stronger determinism check than parsing one
    file twice, and it catches encoding-dependent ID derivation.
  - Version-independence test: `single_room.dxf` and `single_room_r12.dxf` are
    the same geometry as LWPOLYLINE and as POLYLINE. Identical room areas.
  - Tests must pass before any commit.

## AUDIT — MANDATORY, AT EVERY MILESTONE GATE AND IN FULL AT M9

Review the code you just wrote against this checklist and fix what fails.
Report findings honestly; do not declare a gate passed that did not pass.

   1. Does FloorPlanCore reference Unreal anywhere? Any hit is a defect.
   2. Every numeric literal outside FloorPlanLimits.h — justified or a defect?
   3. Every comment in the diff — does it pass the comment policy above? Flag
      each violation as a defect of the same severity as a code smell.
   4. Any dead code, unused function, commented-out block, or abstraction with
      exactly one caller and no second use in sight?
   5. Any file over 400 lines, or any file with more than one public type?
   6. Every DXF field read — is it bounds-checked, NaN-checked, and bounded by
      a named limit?
   7. Can any input reach an unbounded allocation, unbounded loop, or
      unbounded recursion? Trace the worst case explicitly. Include the nine
      silently-accepted adversarial files by name in that trace.
   8. Does any exception escape FloorPlanCore into the Unreal layer?
   9. Is every error path tested, and does every error carry a usable location?
  10. Is ID derivation stable under recomputation, across ASCII and binary
      encodings, and across DXF versions? Prove it with the tests, not by
      inspection.
  11. Any raw float equality on coordinates?
  12. Is any LWPOLYLINE bulge silently discarded anywhere in the read path?
  13. Do all tests pass right now? State the actual result, not the intent.

## DEFINITION OF DONE

  - A real DXF floor plan imports and produces a walkable building in Unreal
    with correct wall thicknesses and openings cut through.
  - Clicking a room reports its name, its floor area, and its stable ID.
  - Reimporting the same file yields identical IDs.
  - All 10 fixtures pass both golden invariants.
  - Every file in the malformed suite behaves per its contract — reject
    cleanly, or survive without crashing or emitting wrong geometry.
  - All tests pass.
  - The M9 audit reports no unresolved defects.

## TECHNICAL NOTES

  - Target the current UE5 release. Confirm the Geometry Script module
    dependencies against that version's build rules rather than assuming —
    the module names have moved between releases.
  - Geometry Script is the mesh generation path; boolean subtraction is how
    openings are cut. Verify boolean robustness on coplanar and near-coplanar
    cases early, because that is where mesh booleans fail.
  - Editor-time import is v1. Do not build the runtime path yet, but do not
    make design decisions that foreclose it: keep all geometry generation
    free of editor-only APIs so the same code can run at runtime later.
  - Scale is explicit in v1. Read `$INSUNITS` where present, otherwise take a
    unit from the user. Do not infer scale. R12 does not export `$INSUNITS` at
    all, so for every R12 file the user-supplied branch is the only branch —
    it is the rule for that version, not an edge case.

## OUT OF SCOPE FOR v1 — DO NOT BUILD

Raster or PDF input. AI or machine learning of any kind. Scale inference.
Curved geometry beyond the arcs and polyline bulges the DXF states explicitly.
Materials beyond a neutral default. Roofs. Furniture. Export formats. Runtime
loading. Multi-sheet cross-storey registration.

If any of these seems necessary to finish a milestone, stop and say so rather
than expanding scope.

## WORKING METHOD

  - Work milestone by milestone. Report the audit result at each gate before
    proceeding.
  - Small focused commits with imperative messages, one per milestone or per
    coherent unit of work.
  - When a design decision has real trade-offs, state the choice and the
    reason in one sentence and continue. Do not stop to ask about routine
    judgment calls.
  - If something is genuinely blocked or a limit above is wrong, say so
    plainly and propose the correction.
