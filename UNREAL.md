# FloorPlanUE — Unreal integration

Target: **UE 5.3+**, developed against 5.6.

## Verification status — read this first

| Layer | Status |
|---|---|
| `Source/FloorPlanCore` | **Verified twice.** 198 tests passing under g++ 16.1 with `-Werror`, and compiled + linked unchanged as a module under MSVC in UE 5.6. |
| `Source/FloorPlanUnreal` | **Builds and runs** in UE 5.6. It has no automated tests — nothing here is compiled by `build.py`, so every change to it is verified by building the plugin. |

### Layout: one module, not two

The core lives at `Source/FloorPlanUnreal/FloorPlanCore/` and compiles **into** the
`FloorPlanUnreal` module. It is not a UE module of its own.

It was two modules to begin with, and that failed to link with six unresolved externals. A UE
module builds as a DLL, and nothing in the core is annotated with a `MODULENAME_API`
dllexport. Annotating it would have meant putting Unreal macros through a body of code whose
entire value is that it has none.

Collapsing to one module also removed a hazard that was there from the start: the core hands
`std::string`, `std::vector` and `std::unique_ptr` across its API, and passing those across a
DLL boundary is fragile even when the exports are correct. There is now no boundary to cross.

The separation the spec asks for is still enforced — the core is its own directory, still has
zero Unreal references, and still builds and tests standalone under g++ via `build.py`.

### Build settings that matter

**`FloorPlanUnreal` must not pin `CppStandard`.** UE 5.3+ compiles at C++20 — its shared PCH
is named `...Cpp20.InclOrderUnreal5_3` — and `UObjectGlobals.h` uses an abbreviated function
template (`BuildMask(auto... Types)`). Forcing C++17 makes the engine's own headers fail to
parse with `error C3533: a parameter cannot have a type that contains 'auto'`. Let the module
inherit the engine default.

**`_CRT_SECURE_NO_WARNINGS` is defined privately** because the core opens files with
`std::fopen` and MSVC deprecates it. The alternative would be platform branching inside code
that is deliberately platform-free, so the build system is the right place for it.

Everything the core does — parsing, geometry, rooms, walls, openings, identity — is proven by
the standalone suite. The adapter layer is not, so that is where to look first when something
renders wrongly.

## Install

1. Copy the whole `FloorPlanUE` folder into your project's `Plugins/` directory.
2. Delete or ignore `build.py`, `Tests/` and `TestData/` — they are the standalone core
   harness and are not part of the plugin. `TestData/` is worth keeping for sample files.
3. Regenerate project files and build.

Unreal's adaptive unity build already excludes the adapter files; if you hit duplicate-symbol
errors from the core's anonymous-namespace helpers, set `bUseUnity = false` in
`FloorPlanUnreal.Build.cs`.

## Running it

### Console — fastest way to see it work

Rebuild, open the level, and type into the editor console (the command line at the bottom of
the Output Log, or `~` in PIE):

```
FloorPlan.Import D:/Plans/single_room.dxf
```

Full form — every argument after the path is optional:

```
FloorPlan.Import <path.dxf> [WallLayer|*] [MmPerUnit] [single|double]
```

Pass `*` for the layer to accept every layer. Pass `0` for `MmPerUnit` to keep what the file
declares.

Three flags may appear **anywhere** in the line, in any order, because they are flags rather
than positions: `bake` writes static mesh assets instead of dynamic meshes, `roof` caps the
topmost storey with a slab, and `json=<path>` dumps the whole model to a file.
`FloorPlan.Import plan.dxf bake` is enough on its own.

Use `roof` whenever you light the building with a sun. Without it the building is open to the
sky: the sun floods the top storey and turns every wall top into a brilliantly lit ledge, which
rims the inside faces with light along their top edges and feeds daylight into the storey
joints. A capped building keeps the sun outside, which is what interior lighting expects.

Actors appear in the level immediately; the log prints storey, room and wall counts, total
area, the adjacency summary, and any label that belonged to no room.

### A whole building

```
FloorPlan.ImportBuilding Ground=D:/Plans/g.dxf@0 First=D:/Plans/1.dxf@2900 bake
```

Each argument is `<name>=<path>@<elevationMm>`; the elevation may be left off for ground level.
With the default 2700 mm walls and the 200 mm slab, an upper storey at 2900 sits flush on the
wall tops below — a larger elevation leaves a visible gap band between the storeys.
Every storey is compiled separately, stacked at its elevation, and parented under one building
actor. The same flags apply, and `json=` writes all storeys into one document.

### Expected results for the shipped fixtures

Point it at `TestData/Fixtures/`. These numbers are the same ones the 198 core tests assert,
so they are a real end-to-end check that the Unreal layer did not distort anything.

| Command | Expect |
|---|---|
| `FloorPlan.Import .../single_room.dxf` | 1 room, **20.00 m²**, 4 walls |
| `FloorPlan.Import .../two_rooms_shared_wall.dxf` | 2 rooms, **21.00 m²** total, 7 walls |
| `FloorPlan.Import .../labeled_rooms.dxf` | 2 rooms named *Bedroom 1* and *Bathroom*, 1 unassigned label |
| `FloorPlan.Import .../room_in_room.dxf` | 2 rooms, **47.19 m²** total (45.75 + 1.44) |
| `FloorPlan.Import .../arc_wall.dxf` | 1 room, **26.28 m²**, **4 walls one of which is a semicircle** |
| `FloorPlan.Import .../door_and_window.dxf` | 1 room, 20.00 m², **2 openings** cut |
| `FloorPlan.Import .../single_line_two_rooms.dxf A-WALL 0 single` | 2 rooms, **40.00 m²** total |
| `FloorPlan.Import .../line_pair_room.dxf` | 1 room, 20.00 m² — walls drawn as loose lines |

And the real-world file, which needs both a layer filter and a unit override:

```
FloorPlan.Import .../RealWorld/wipeout_door.dxf A.Wall_Internal 1.0
```

Expect **0 rooms and 2 walls**. That is correct — it is a fragment of two parallel wall runs
enclosing nothing. Drop the layer argument and you will get phantom rooms from the exploded
door, which is exactly why the filter exists.

### Blueprint

`Import Dxf Simple` is a single node — no object construction needed. Inputs: file path, wall
layer (leave empty for all layers), convention, millimetres per unit (0 keeps what the file
declares), wall height, and whether to bake static meshes.

For full control use `Import Dxf` with a `FloorPlanImportOptions` object, or `Import Building`
with an array of `FloorPlanStorey` (path, name, elevation) for a multi-storey building.

### Inspecting the result

Each import produces one `AFloorPlanBuildingActor`, and beneath it one `AFloorPlanStoreyActor`
per plan, and beneath those the rooms and walls. Moving, rotating or deleting the building
moves or removes everything; importing a second DXF gives a second, independent root.

A single-file import still builds all three levels, with one storey named after the file, so
the hierarchy and the exported data are the same shape whether you import one plan or ten.

Each element sits at its own pivot. A wall's origin is the midpoint of its centreline with
local **+X running along its length**, so its rotation is meaningful and its gizmo lands on the
wall. A room's origin is the centre of its boundary.

Select any actor. Rooms expose `ElementId`, `RoomName`, `StoreyName`,
`FloorAreaSquareMetres`, `AdjacentRoomIds`, `ConnectedRoomIds` and `bTouchesOutside`; walls
expose `ElementId`, `ThicknessMm`, `LengthMm`, `OpeningCount` and `bIsExterior`; storeys and
the building expose their totals. All four have a `Describe()` node. Reimport the same file and
the ids are identical.

## The adjacency graph

Two rooms are **adjacent** when wall material separates them, and **connected** when an opening
sits in that wall. The distinction is the useful part: adjacency tells you what shares a
partition, connectivity tells you what you can walk between.

It is derived by probing, not assumed from the drawing. Each wall is sampled at five points
along its length, and at each the space half a thickness plus 10 mm off either face is tested
against every room boundary, taking the smallest room that contains it. A wall with a room on
one side and nothing on the other is an exterior wall, which is where `bIsExterior` and
`bTouchesOutside` come from. Curved walls are sampled around their arc, not along their chord.

Every wall separating the same pair of spaces collapses into one link that lists them all, so
a room split across three wall segments still yields one relationship, not three.

## Stairs and vertical connections

A room is **vertical circulation** when its label starts with `STAIR`/`STAIRCASE`/`STR` or
`LIFT`/`ELEV`, or when two or more lines on a stair layer fall inside it. Both routes are
configurable; the layer list defaults to `A-FLOR-STRS`, `A-STRS`, `STAIR`, `STAIRS`.

Nothing tries to recognise a staircase from the shape of its linework. That would be guesswork.
What the drawing *says* — through a label or a layer — is what it goes on.

**The run direction comes from the treads.** Tread lines are averaged as an axis rather than a
direction, since they point either way, and the run is taken perpendicular to that. Only when
no treads are found does it fall back to the room's longer side, which is often wrong for a
square stairwell — so draw the treads.

**Step count comes from the treads too, when they are credible.** The rise between storeys
divided by the tread count has to land in the 100–250 mm a person can climb; if it does not,
the lines were hatching or a landing outline, not one line per step, and the flight is divided
by target riser height instead. Either way the steps add up to exactly the storey height —
there is no partial step at the top.

The top storey has nothing above it, so its flights climb to `WallHeightMm`.

**Stacked stairs link the storeys.** Two circulation regions of the same kind on consecutive
plans are connected when their footprints overlap, measured by sampling the lower one on an
8×8 grid and requiring 40% of it to fall inside the upper. Lifts link the same way and never
get steps.

The result reports `VerticalLinks`, and each `AFloorPlanStairActor` carries its step count,
riser, tread, total rise, which storey it arrives at, and whether the steps came from drawn
treads or from the fallback rule.

## What else a plan carries

Beyond rooms and walls, these are read and reported. Everything is keyed by the same stable
ids, exported to JSON, and where it has geometry, spawned as an actor.

**Dimensions** are the one that earns its place. A DIMENSION carries what the draughtsman
*asserted* a distance is, which is independent evidence against the geometry you measured. Both
are kept: `MeasurementMm` for the claim, `GeometryMm` for the extension lines it is drawn
across, and `AgreesWithGeometry` for whether they match.

A drawing stretched without its dimensions being updated is a real and nasty failure — the
geometry is wrong and nothing about it looks wrong. `annotated_room.dxf` contains exactly that:
a dimension stating 3800 across a 4000 run. The import warns rather than believing either.

Note that **many writers omit the cached measurement** (group 42) entirely. When it is absent
the extension lines are measured instead and `MeasurementWasStated` is false, so you always
know whether the number came from the drawing or from us.

**Columns** are closed profiles or blocks on a column layer, extruded full height as
`AFloorPlanColumnActor`. Profiles are kept apart from the room loops so a column can never be
mistaken for a room boundary.

**Blocks** — furniture, fittings, equipment — become `AFloorPlanFixtureActor`: an empty actor
at the right place, rotation and scale, carrying the block name and source layer. The plugin
cannot know what a `FURN_SOFA` looks like, so it gives you the transform and the name and you
attach your own mesh. Blocks already understood as doors or windows are not listed twice.

**Grid lines** are lines on a grid layer, each labelled from the nearest text on the same layer.

**Elevation** is reported rather than used: the Z values and polyline elevations found, and
whether they are all one level. A plan that turns out not to be planar is worth knowing about
before you trust anything else.

**Hatches** are read in full: every boundary path, polyline or edge, and every edge type.

- **Line** and **circular arc** edges are exact, the arc keeping its curve as a bulge.
- **Elliptic arc** edges are chorded, because an ellipse is not an arc and has no bulge.
- **Spline** edges are evaluated as the NURBS they are, by de Boor, weights included. A spline
  whose knot vector does not match its control points and degree falls back to its fit points,
  and then to its control polygon, rather than being dropped.

Every path becomes its own loop and joins the same containment tree as everything else, so an
inner path is a hole by the ordinary parity rule and needs no special case.

The convention that matters: **the outermost loop is the outside face of wall material**, as it
is everywhere else in this plugin. A hatch used as wall poche therefore reads correctly — its
outer path is the outside of the wall, its inner path is the room, and the two pair into walls
with a measured thickness exactly as a pair of polylines would. A hatch used as a room *fill*
reads inside out under the same rule, which is why `WallLayers` matters here as much as
anywhere.

`skippedHatchPaths` still reports any path that could not be built.

## Data export

`json=<path>` on either command, or `DataExportPath` in the options, writes the whole model:

```json
{
  "storeys": [
    {
      "storey": "Ground", "elevationMm": 0,
      "totalFloorAreaMm2": 20000000, "wallFootprintMm2": 3760000,
      "rooms": [ {"id": "...", "name": "Bedroom 1", "areaMm2": 12000000, "boundaryMm": [...]} ],
      "walls": [ {"id": "...", "startMm": [...], "bulge": 0, "lengthMm": 5400, ...} ],
      "openings": [ {"id": "...", "kind": "door", "hostWallId": "...", "widthMm": 900, ...} ],
      "adjacency": [ {"rooms": ["...", null], "wallIds": [...], "traversable": false, ...} ],
      "circulation": [ {"kind": "stair", "roomId": "...", "drawnTreads": 17, ...} ]
    }
  ],
  "verticalConnections": [
    {"kind": "stair", "fromStorey": "Ground", "toStorey": "First", "fromRoomId": "...", ...}
  ]
}
```

Everything is keyed by the same element ids the actors carry, so the JSON and the level refer
to the same things. `null` in an adjacency pair means the outside. Numbers are written in the
shortest form that reads back bit-identical, so a room area of `26283185.307179585` survives
the trip out and back with nothing lost.

## Materials

`WallMaterial` and `FloorMaterial` are applied to whichever component ends up carrying the
geometry, baked or not. UVs are one unit per metre, so a tiling material holds its scale across
every element without per-wall adjustment.

## Use

```cpp
UFloorPlanImportOptions* Options = NewObject<UFloorPlanImportOptions>();
Options->Convention = EFloorPlanWallConvention::DoubleLine;
Options->WallLayers.Add(TEXT("A-WALL"));
Options->WallHeightMm = 2700.0;

const FFloorPlanImportResult Result =
    UFloorPlanImporter::ImportDxf(this, TEXT("D:/plans/school.dxf"), Options);
```

Every spawned actor carries `ElementId`. Rooms also carry `RoomName` and
`FloorAreaSquareMetres`; walls carry `ThicknessMm`, `LengthMm` and `OpeningCount`. Both
expose `Describe()` for a one-line summary. Reimporting the same file yields the same ids.

## Three settings that cannot be inferred from the file

Real drawings proved each of these, so they are exposed rather than guessed:

- **`WallLayers`** — not optional. `wipeout_door.dxf` contains an exploded door as 136 loose
  lines; with no filter those outlines close into phantom rooms and overstate the wall
  footprint by 30%.
- **`bOverrideDeclaredUnits`** — `wipeout_door.dxf` declares `$INSUNITS = 1` (inches) for
  geometry that is plainly millimetres. Honouring the header gives a 2.5 m thick internal
  wall. R12 files declare nothing at all.
- **`Convention`** — double-line and single-line drawings are genuinely ambiguous; nothing in
  a DXF tells you which you have.

## Curved walls

A wall whose two faces are concentric arcs is carried through as a centreline plus a DXF
bulge, and built as a tessellated tube rather than a box. The centreline runs at the mean of
the two radii and the wall spans the angular range both faces cover.

The pairing is separate from the straight one because chords lie about arcs. In
`arc_wall.dxf` the semicircular wall's inner and outer faces have chords that are *collinear*
— both sit on x = 5000 — so measuring them as straight faces gives a gap of zero and the wall
is silently discarded. Arc faces therefore pair only with arc faces, matched on centre and
radius; a curved face and a flat face never bound the same wall.

Junction closing leaves curved walls alone. Their ends already land on the tangent point where
the straight neighbour's centreline stops, so there is nothing to extend, and extending along
a chord would be the wrong direction anyway.

Tessellation uses the same sagitta tolerance as the core's area computation, so a curved
wall's inner face and the room floor it encloses agree to well under a millimetre.

A curved wall's `LengthMm` is measured along its centreline, so it is longer than the distance
between the two ends the actor sits between.

## Geometry approach, and why

Walls are built as prisms directly from `FDynamicMesh3`, decomposing each wall's elevation
into rectangular slabs around its openings. **No mesh booleans are used.**

That is deliberate. `ApplyMeshBoolean` returns no success flag — no bool, no outcome pin, no
documented debug message — so a failed cut is silent. It is also documented as
non-deterministic across engine versions, which would break the stable-identity guarantee
that is the whole point of this plugin. Slab decomposition is exact, branch-free and
reproducible.

The trade-off: adjacent slabs share coplanar interior faces. Total volume stays correct
because the slabs do not overlap, but the wall is not a single manifold. If you need one,
weld with `UGeometryScriptLibrary_MeshRepairFunctions::WeldMeshEdges` after generation.

## What to check first on your machine

In rough order of likelihood:

1. **`UDynamicMesh::EditMesh` signature.** Used in `FloorPlanMeshBuilder::CopyToDynamicMesh`
   with four arguments. Confirmed against working 5.x code but not compiled here.
2. **`ADynamicMeshActor::GetDynamicMeshComponent()`.** The 5.8 docs still describe the
   UE4-era `USimpleDynamicMeshComponent`; the accessor is the real one, but verify.
3. **Module names in `FloorPlanUnreal.Build.cs`.** `GeometryCore`, `GeometryFramework` and
   `GeometryScriptingCore` are correct for 5.3–5.8. `DynamicMesh` and `GeometryAlgorithms`
   are only needed if you extend beyond what is here — drop them if they fail to resolve.
4. **`FDynamicMesh3::AppendVertex` / `AppendTriangle`.** Stable across 5.x, but the
   `FIndex3i` overload is what this code assumes.
5. **`THIRD_PARTY_INCLUDES_START`** around the core includes. If your project's warning
   settings are lenient you can drop it; if they are strict you may need it in more places.
6. **`FloorPlanMeshUVs.cpp`** touches the attribute set directly —
   `Attributes()->PrimaryUV()`, `ClearElements`, `AppendElement`, `SetTriangle`. All stable
   across 5.x, but it is new here.
7. **`FloorPlanStaticMeshBaker.cpp`** is the newest and least proven file. It calls
   `UGeometryScriptLibrary_CreateNewAssetFunctions::CreateNewStaticMeshAssetFromMesh` from the
   editor-only `GeometryScriptingEditor` module. If the field names on
   `FGeometryScriptCreateNewStaticMeshAssetOptions` have moved, that is the whole surface area
   — everything else in the file is `IAssetTools` and `NewObject`.

## Triangle winding

The face tables in `FloorPlanMeshBuilder` are written in **right-handed order**, where
`(B-A) x (C-A)` is the outward normal — the convention the geometry is easiest to reason
about. Unreal's rasterizer treats the reverse as front-facing, so `AppendOutwardTriangle`
performs the swap in exactly one place.

If walls ever render inside-out — looking from outside you see the far wall's inner surface,
looking from inside you see the outer surface — that is backface culling with inverted
winding, and that helper is the only line to change. Note the symptom is *not* transparency:
the near face is culled, so you see straight through to whatever is behind it.

## Baking to static meshes

`UDynamicMeshComponent` renders and casts sun shadows, but it generates no mesh distance
field, so it contributes nothing to Lumen and cannot use Nanite. In an interior that shows up
as corners which never darken — the geometry is sealed, but no ambient occlusion term exists
to darken it — and as shadow-map bias artifacts at every contact point.

Turning on `bBakeToStaticMesh` writes each room and wall out as a real `UStaticMesh` asset and
puts a `UStaticMeshComponent` on the actor instead. Distance fields, Lumen and Nanite all apply
from that point on.

```
FloorPlan.Import D:/Plans/single_room.dxf bake
```

Assets land in `StaticMeshFolder` under a subfolder named after the DXF —
`/Game/FloorPlan/single_room/Wall_a685e55d` by default. Names come from the element id, so the
same wall in the same drawing always produces the same asset name.

Things worth knowing:

- **The assets are created unsaved.** They appear dirty in the Content Browser; save them or
  they are gone when the editor closes. This matches how Modeling Mode's bake behaves.
- **Re-importing does not overwrite.** A second import of the same file creates
  `Wall_a685e55d1`, `Wall_a685e55d2` and so on rather than replacing assets you may have
  edited. Delete the folder first if you want a clean rebuild.
- **Editor only.** `FFloorPlanStaticMeshBaker::Bake` returns null in a packaged build and the
  importer falls back to the dynamic mesh, so nothing breaks — you just get no assets. The
  same fallback runs if asset creation fails for any other reason, and `BakedMeshes` on the
  result reports how many actually got written.
- **Nanite is off by default.** A wall is twelve triangles; Nanite is overhead at that size.
  It becomes worth turning on once curved walls and dense plans push the counts up.
- **Lightmap UVs are not generated.** UV0 is a real unwrap, so if you want baked lighting turn
  on *Generate Lightmap UVs* in the static mesh editor and it has something sane to work from.

The actor keeps its `UDynamicMeshComponent` as its root — it is what `ADynamicMeshActor`
provides — but it is emptied and hidden, and the static mesh component is attached beneath it.

## Walls sit *into* the slab, not on it

The floor slab occupies Z from 0 down to −200 mm. Walls run from the same −200 mm up to the
wall height, so a wall overlaps the full thickness of the slab beside it instead of meeting it
along a line. The slab is deliberately at real construction thickness: Lumen and Virtual Shadow
Maps bleed light straight through plates much thinner than their trace bias, so a token-thin
slab reads as translucent from below.

Butting them at Z = 0 leaves the building with no continuous underside: below the finished
floor and outside the room boundary there is nothing, so from a low angle you see an open slot
under every wall, and shadow bias leaks a bright band along the inside of the wall base. The
overlap closes both. The visible wall height above the floor is unchanged.

This applies only when floors are generated. With `bGenerateFloors` off the walls start at
Z = 0, because there is no slab for them to meet.

## UVs

Every generated mesh carries a planar unwrap on UV0 at **one UV unit per metre**, so a tiling
material holds the same real-world scale on a 1 m wall and a 12 m one, and on the floor as on
the walls. No per-element tweaking, no stretching between elements.

Each triangle is projected along whichever axis its normal points down most strongly, so a
wall's faces, ends, top and bottom each get their own flat projection rather than one
projection smeared over the whole box.

Curved walls are unwrapped in the frame they were swept along, not in world space. Each vertex
is converted to *distance along the centreline*, *offset across it*, and height, and the
projection runs there. A box projection in world space would flip axis every 90° around the
sweep and put a seam in the middle of the wall; unrolling first means a curved wall's texture
runs along it exactly as a straight wall's does, and a brick course carries around the bend.

## Shading

Generated meshes get an attribute set and **per-triangle** normals, not per-vertex. A wall is
a box, and smoothing its normals across an edge rounds every corner into mush. If you want
smoothing later, call `RecomputeNormals` with a crease angle rather than switching to
per-vertex.

Meshes built without normals at all render black and change appearance with the view angle.
If you see that, the attribute set is missing — check `CopyToDynamicMesh`.

## Known limitations

- **No Nanite, no Lumen without baking.** `UDynamicMeshComponent` supports neither, in any 5.x
  release, and there is no runtime path to Nanite at all — the builder ships in the engine's
  `Developer` tree. Turn on `bBakeToStaticMesh` in the editor to get both.
- **Floors do not cut contained voids.** A room with a shaft inside it gets a floor slab that
  spans the shaft. The area reported on the actor is correct; only the slab geometry is not.
- **Openings are cut as rectangles.** Swing direction and hinge side are parsed by the core
  but not yet represented in geometry.
- **Openings are not cut into curved walls.** An opening is positioned by projecting onto the
  wall's chord, which is not where the wall is. A curved wall is therefore built solid and its
  actor reports `OpeningCount` 0 rather than placing the hole wrongly. No test file has one.
- **Furniture is placed, not modelled.** A fixture actor is an empty transform with a name.
- **Roofs are flat slabs with eaves.** The `roof` flag caps the topmost storey with one slab
  per building outline — 400 mm thick (`RoofSlabThicknessMm`), overhanging the facade by
  500 mm (`RoofOverhangMm`), bearing on the wall heads with the wall tops raised into its
  underside. The overhang shades the upper walls and pushes sunlit surfaces away from the
  ceiling junctions, which is what keeps sealed interiors dark at the seams. A single-line
  plan has no envelope outline, so its rooms are capped one by one. There are no pitched
  roofs or parapets. Non-top storeys get the same burial: their wall tops rise into the slab
  of the storey above rather than meeting it edge to edge. Stairs are built, but nothing
  else joins storeys: no ramps, no escalators, and a lift shaft is linked as data without a
  car in it.
- **Only straight flights.** A dog-leg or a spiral stair is built as one straight run across
  its footprint. The link between storeys is still correct; the geometry is not.
- **Every storey is a separate DXF.** A single file holding several plans on different layers
  is not detected as multi-storey.
- Editor-only calls are guarded with `WITH_EDITOR`; nothing here depends on
  `AGeneratedDynamicMeshActor`, which is editor-only and silently disappears in PIE and
  packaged builds.

## Lighting sealed interiors

A sealed, windowless room lit by a physically bright sun (a SunSky's directional light is
75,000 lux) shows a **thin bright line hugging the floor–wall junction of every wall whose
exterior faces the sun**, with a bloom halo around it. This is not a gap in the imported
geometry: the shell is watertight and light-tight — a point light placed inside such a room
leaks nothing out — and a plain engine cube stood against a wall grows the same line. It is a
renderer contact artifact. Every shadow method (ray-traced shadows, virtual shadow maps,
cascaded shadow maps) is imprecise in the last few millimetres where a receiver touches its
occluder, and auto exposure, balancing a dim interior against a 75,000-lux sun, amplifies that
sliver of missed occlusion by orders of magnitude until it reads as a glowing seam.

The cure is the screen-space contact trace, which exists for exactly this range and works
under every shadow method: on the directional light, **Contact Shadow Length = 0.5** (leave
*Contact Shadow Length in World Space Units* off). Half a screen of trace is what fully
extinguishes the seam in interiors; if the same light also covers content where so long a
march shows streaking or costs too much, 0.05–0.1 removes most of the line for a fraction of
the price — the value lives in `FloorPlanLimits.h` as `SunContactShadowScreenFraction`.
**The importer applies this itself**: after spawning, it switches the trace on for
every directional light in the level whose contact length is zero, and reports having done so.
A light with any contact length already set is left alone, and
`bEnsureSunContactShadows = false` on the import options disables the behaviour entirely.
Clamping auto exposure (a PostProcessVolume with a Min EV100 suited to interiors) reduces the
amplification and is the standard companion setting; the importer does not touch exposure.

The importer also attacks the seam at its geometric root when baking: every wall carries an
**invisible shadow blocker** — a copy inflated by 100 mm on every side (`ShadowBlockerMarginMm`)
that is hidden from the camera but still casts shadows, so the shadow test sees a wall thick
enough to swallow the seam zone while the visible geometry stays exactly what the drawing
says. Openings are cut oversize in the blocker so it never eats legitimate light through a
door or window. Disable with `bGenerateShadowBlockers = false`; the blockers exist only on
the baked path.

Do not chase this artifact through shadow cvars. It survives, unchanged, all of:
`r.Shadow.Virtual.*`, `r.RayTracing.Shadows` and its denoiser and bias settings,
`r.DistanceFieldShadowing`, `r.DynamicGlobalIlluminationMethod 0`, and reflection toggles —
because it is not specific to any one shadow path. `showflag.DirectLighting 0` removing the
line while an interior point light stays contained is the signature that identifies it.
