import json
import math
import sys
from pathlib import Path

import ezdxf
from ezdxf.enums import TextEntityAlignment

LAYER_WALL = "A-WALL"
LAYER_DOOR = "A-DOOR"
LAYER_GLAZ = "A-GLAZ"
LAYER_ANNO = "A-ANNO"
LAYER_STRS = "A-FLOR-STRS"

TEXT_HEIGHT = 200.0
DOOR_WIDTH = 900.0
WINDOW_WIDTH = 1200.0


def new_document(version="R2010"):
    doc = ezdxf.new(version, setup=True)
    doc.units = ezdxf.units.MM
    for name in (LAYER_WALL, LAYER_DOOR, LAYER_GLAZ, LAYER_ANNO, LAYER_STRS):
        if name not in doc.layers:
            doc.layers.add(name)
    return doc


def add_face(msp, points, bulges=None):
    if bulges is None:
        msp.add_lwpolyline(points, close=True, dxfattribs={"layer": LAYER_WALL})
        return
    vertices = [(x, y, bulge) for (x, y), bulge in zip(points, bulges)]
    msp.add_lwpolyline(
        vertices, format="xyb", close=True, dxfattribs={"layer": LAYER_WALL}
    )


def define_door_block(doc, name=f"DOOR_{int(DOOR_WIDTH)}"):
    if name in doc.blocks:
        return name
    blk = doc.blocks.new(name=name)
    blk.add_line((0.0, 0.0), (0.0, DOOR_WIDTH))
    blk.add_arc(center=(0.0, 0.0), radius=DOOR_WIDTH, start_angle=0.0, end_angle=90.0)
    blk.add_line((0.0, 0.0), (DOOR_WIDTH, 0.0))
    return name


def define_window_block(doc, name=f"WIN_{int(WINDOW_WIDTH)}"):
    if name in doc.blocks:
        return name
    blk = doc.blocks.new(name=name)
    blk.add_line((0.0, 0.0), (WINDOW_WIDTH, 0.0))
    blk.add_line((0.0, 60.0), (WINDOW_WIDTH, 60.0))
    return name


def label(msp, text, position):
    entity = msp.add_text(
        text, height=TEXT_HEIGHT, dxfattribs={"layer": LAYER_ANNO}
    )
    entity.set_placement(position, align=TextEntityAlignment.MIDDLE_CENTER)


def single_room(path):
    doc = new_document()
    msp = doc.modelspace()
    add_face(msp, [(0, 0), (5000, 0), (5000, 4000), (0, 4000)])
    add_face(msp, [(-200, -200), (5200, -200), (5200, 4200), (-200, 4200)])
    doc.saveas(path)


def two_rooms_shared_wall(path, with_labels=False):
    doc = new_document()
    msp = doc.modelspace()
    add_face(msp, [(0, 0), (4000, 0), (4000, 3000), (0, 3000)])
    add_face(msp, [(4150, 0), (7150, 0), (7150, 3000), (4150, 3000)])
    add_face(msp, [(-200, -200), (7350, -200), (7350, 3200), (-200, 3200)])
    if with_labels:
        label(msp, "Bedroom 1", (2000, 1500))
        label(msp, "Bathroom", (5650, 1500))
        label(msp, "NORTH ELEVATION", (3500, -1500))
    doc.saveas(path)


def l_shaped_room(path):
    doc = new_document()
    msp = doc.modelspace()
    add_face(
        msp,
        [(0, 0), (6000, 0), (6000, 3000), (4000, 3000), (4000, 5000), (0, 5000)],
    )
    add_face(
        msp,
        [
            (-200, -200),
            (6200, -200),
            (6200, 3200),
            (4200, 3200),
            (4200, 5200),
            (-200, 5200),
        ],
    )
    doc.saveas(path)


def room_in_room(path):
    doc = new_document()
    msp = doc.modelspace()
    add_face(msp, [(0, 0), (8000, 0), (8000, 6000), (0, 6000)])
    add_face(msp, [(-200, -200), (8200, -200), (8200, 6200), (-200, 6200)])
    add_face(msp, [(3000, 2000), (4500, 2000), (4500, 3500), (3000, 3500)])
    add_face(msp, [(3150, 2150), (4350, 2150), (4350, 3350), (3150, 3350)])
    doc.saveas(path)


def door_and_window(path):
    doc = new_document()
    msp = doc.modelspace()
    add_face(msp, [(0, 0), (5000, 0), (5000, 4000), (0, 4000)])
    add_face(msp, [(-200, -200), (5200, -200), (5200, 4200), (-200, 4200)])
    door = define_door_block(doc)
    window = define_window_block(doc)
    msp.add_blockref(
        door, (1500, -100), dxfattribs={"layer": LAYER_DOOR, "rotation": 0.0}
    )
    msp.add_blockref(
        window, (5100, 1400), dxfattribs={"layer": LAYER_GLAZ, "rotation": 90.0}
    )
    doc.saveas(path)


def varying_thickness(path):
    doc = new_document()
    msp = doc.modelspace()
    add_face(msp, [(0, 0), (5000, 0), (5000, 4000), (0, 4000)])
    add_face(msp, [(-300, -150), (5300, -150), (5300, 4150), (-300, 4150)])
    doc.saveas(path)


def arc_wall(path):
    doc = new_document()
    msp = doc.modelspace()
    add_face(
        msp,
        [(0, 0), (5000, 0), (5000, 4000), (0, 4000)],
        bulges=[0.0, 1.0, 0.0, 0.0],
    )
    add_face(
        msp,
        [(-200, -200), (5000, -200), (5000, 4200), (-200, 4200)],
        bulges=[0.0, 1.0, 0.0, 0.0],
    )
    doc.saveas(path)


def single_room_r12(path):
    doc = new_document("R12")
    msp = doc.modelspace()
    msp.add_polyline2d(
        [(0, 0), (5000, 0), (5000, 4000), (0, 4000)],
        close=True,
        dxfattribs={"layer": LAYER_WALL},
    )
    msp.add_polyline2d(
        [(-200, -200), (5200, -200), (5200, 4200), (-200, 4200)],
        close=True,
        dxfattribs={"layer": LAYER_WALL},
    )
    doc.saveas(path)


def add_rectangle_lines(msp, x0, y0, x1, y1, layer=LAYER_WALL):
    corners = [(x0, y0), (x1, y0), (x1, y1), (x0, y1)]
    for index in range(4):
        start = corners[index]
        end = corners[(index + 1) % 4]
        msp.add_line(start, end, dxfattribs={"layer": layer})


def line_pair_room(path):
    doc = new_document()
    msp = doc.modelspace()
    add_rectangle_lines(msp, 0, 0, 5000, 4000)
    add_rectangle_lines(msp, -200, -200, 5200, 4200)
    doc.saveas(path)


def line_pair_room_with_noise(path):
    doc = new_document()
    msp = doc.modelspace()
    add_rectangle_lines(msp, 0, 0, 5000, 4000)
    add_rectangle_lines(msp, -200, -200, 5200, 4200)
    msp.add_line((-2000, -2000), (8000, -2000), dxfattribs={"layer": LAYER_ANNO})
    msp.add_line((-2000, -2200), (-2000, -1800), dxfattribs={"layer": LAYER_ANNO})
    add_rectangle_lines(msp, 1000, 1000, 1800, 1600, layer=LAYER_ANNO)
    label(msp, "Studio", (2500, 2000))
    doc.saveas(path)


def single_line_two_rooms(path):
    doc = new_document()
    msp = doc.modelspace()
    add_rectangle_lines(msp, 0, 0, 8000, 5000)
    msp.add_line((5000, 0), (5000, 5000), dxfattribs={"layer": LAYER_WALL})
    label(msp, "Living", (2500, 2500))
    label(msp, "Kitchen", (6500, 2500))
    doc.saveas(path)


def single_room_binary(path):
    doc = new_document()
    msp = doc.modelspace()
    add_face(msp, [(0, 0), (5000, 0), (5000, 4000), (0, 4000)])
    add_face(msp, [(-200, -200), (5200, -200), (5200, 4200), (-200, 4200)])
    doc.saveas(path, fmt="bin")


STOREY_TREADS = 17


def storey_plan(path, main_label, with_treads=True):
    doc = new_document()
    msp = doc.modelspace()
    add_face(msp, [(-200, -200), (8200, -200), (8200, 4200), (-200, 4200)])
    add_face(msp, [(0, 0), (5000, 0), (5000, 4000), (0, 4000)])
    add_face(msp, [(5200, 0), (8000, 0), (8000, 4000), (5200, 4000)])

    if with_treads:
        step = 4000.0 / STOREY_TREADS
        for index in range(STOREY_TREADS):
            y = step * (index + 0.5)
            msp.add_line((5200, y), (8000, y), dxfattribs={"layer": LAYER_STRS})

    label(msp, main_label, (2500, 2000))
    label(msp, "STAIR", (6600, 2000))
    doc.saveas(path)


def two_storey_ground(path):
    storey_plan(path, "Living")


def two_storey_first(path):
    storey_plan(path, "Bedroom")


LAYER_COLS = "A-COLS"
LAYER_GRID = "A-GRID"
LAYER_FURN = "A-FURN"


def annotated_room(path):
    doc = new_document()
    for name in (LAYER_COLS, LAYER_GRID, LAYER_FURN):
        if name not in doc.layers:
            doc.layers.add(name)
    msp = doc.modelspace()
    add_face(msp, [(0, 0), (5000, 0), (5000, 4000), (0, 4000)])
    add_face(msp, [(-200, -200), (5200, -200), (5200, 4200), (-200, 4200)])

    honest = msp.add_linear_dim(
        base=(0, -800), p1=(0, 0), p2=(5000, 0), dxfattribs={"layer": LAYER_ANNO}
    )
    honest.render()
    honest.dimension.dxf.actual_measurement = 5000.0

    stale = msp.add_linear_dim(
        base=(-800, 0),
        p1=(0, 0),
        p2=(0, 4000),
        angle=90,
        dxfattribs={"layer": LAYER_ANNO},
    )
    stale.render()
    stale.dimension.dxf.actual_measurement = 3800.0

    msp.add_lwpolyline(
        [(1000, 1000), (1400, 1000), (1400, 1400), (1000, 1400)],
        close=True,
        dxfattribs={"layer": LAYER_COLS},
    )

    msp.add_line((-1000, 2000), (6000, 2000), dxfattribs={"layer": LAYER_GRID})
    grid_label = msp.add_text("A", height=TEXT_HEIGHT, dxfattribs={"layer": LAYER_GRID})
    grid_label.set_placement((-1000, 2000), align=TextEntityAlignment.MIDDLE_CENTER)

    sofa = doc.blocks.new(name="FURN_SOFA")
    sofa.add_lwpolyline([(0, 0), (2000, 0), (2000, 800), (0, 800)], close=True)
    msp.add_blockref("FURN_SOFA", (3000, 3000), dxfattribs={"layer": LAYER_FURN,
                                                            "rotation": 45.0})

    doc.saveas(path)


LAYER_HATCH = "A-FLOR-FILL"

HATCH_OUTER = 6000.0
HATCH_WALL = 200.0
HATCH_ROOM = HATCH_OUTER - 2 * HATCH_WALL
HATCH_COLUMN = 900.0
HATCH_ROOM_AREA = HATCH_ROOM * HATCH_ROOM - HATCH_COLUMN * HATCH_COLUMN
HATCH_FOOTPRINT = HATCH_OUTER * HATCH_OUTER - HATCH_ROOM * HATCH_ROOM + (
    HATCH_COLUMN * HATCH_COLUMN
)


def square(x0, y0, side):
    return [(x0, y0), (x0 + side, y0), (x0 + side, y0 + side), (x0, y0 + side)]


def hatched_room(path):
    doc = new_document()
    if LAYER_HATCH not in doc.layers:
        doc.layers.add(LAYER_HATCH)
    msp = doc.modelspace()

    hatch = msp.add_hatch(dxfattribs={"layer": LAYER_HATCH})
    outer = hatch.paths.add_edge_path(flags=1)
    for index, start in enumerate(square(0, 0, HATCH_OUTER)):
        end = square(0, 0, HATCH_OUTER)[(index + 1) % 4]
        outer.add_line(start, end)

    hatch.paths.add_polyline_path(
        square(HATCH_WALL, HATCH_WALL, HATCH_ROOM), is_closed=True, flags=0
    )
    column = HATCH_OUTER / 2.0 - HATCH_COLUMN / 2.0
    hatch.paths.add_polyline_path(
        square(column, column, HATCH_COLUMN), is_closed=True, flags=0
    )
    doc.saveas(path)


def hatch_curved_edges(path):
    doc = new_document()
    if LAYER_HATCH not in doc.layers:
        doc.layers.add(LAYER_HATCH)
    msp = doc.modelspace()

    hatch = msp.add_hatch(dxfattribs={"layer": LAYER_HATCH})
    edges = hatch.paths.add_edge_path(flags=1)
    edges.add_line((0, 0), (4000, 0))
    edges.add_arc(center=(4000, 2000), radius=2000, start_angle=270, end_angle=90)
    edges.add_line((4000, 4000), (0, 4000))
    edges.add_ellipse(
        center=(0, 2000), major_axis=(0, 2000), ratio=0.5, start_angle=0, end_angle=180
    )
    doc.saveas(path)


SEMICIRCLE_AREA = math.pi * 2000.0 * 2000.0 / 2.0
ARC_WALL_INTERIOR = 5000.0 * 4000.0 + SEMICIRCLE_AREA
ARC_WALL_EXTERIOR = 5200.0 * 4400.0 + math.pi * 2200.0 * 2200.0 / 2.0

WALL_FOOTPRINT = {
    "single_room.dxf": 3_760_000.0,
    "two_rooms_shared_wall.dxf": 4_670_000.0,
    "l_shaped_room.dxf": 4_560_000.0,
    "room_in_room.dxf": 6_570_000.0,
    "door_and_window.dxf": 3_760_000.0,
    "labeled_rooms.dxf": 4_670_000.0,
    "varying_thickness.dxf": 4_080_000.0,
    "arc_wall.dxf": ARC_WALL_EXTERIOR - ARC_WALL_INTERIOR,
    "single_room_r12.dxf": 3_760_000.0,
    "single_room_binary.dxf": 3_760_000.0,
    "line_pair_room.dxf": 3_760_000.0,
    "line_pair_room_with_noise.dxf": 3_760_000.0,
    "single_line_two_rooms.dxf": 0.0,
    "two_storey_ground.dxf": 5_760_000.0,
    "two_storey_first.dxf": 5_760_000.0,
    "annotated_room.dxf": 3_760_000.0,
    "hatched_room.dxf": HATCH_FOOTPRINT,
    "hatch_curved_edges.dxf": 0.0,
}

EXPECTED = {
    "single_room.dxf": {
        "rooms": [{"area_mm2": 20_000_000.0, "name": None}],
        "walls": 4,
        "wall_thickness_mm": [200, 200, 200, 200],
        "openings": [],
        "note": "Interior 5000x4000, uniform 200 wall. Baseline case.",
    },
    "two_rooms_shared_wall.dxf": {
        "rooms": [
            {"area_mm2": 12_000_000.0, "name": None},
            {"area_mm2": 9_000_000.0, "name": None},
        ],
        "walls": 5,
        "wall_thickness_mm": [200, 200, 200, 200, 150],
        "openings": [],
        "note": "Shared 150 partition. Wall count before junction splitting; splitting policy may raise it.",
    },
    "l_shaped_room.dxf": {
        "rooms": [{"area_mm2": 26_000_000.0, "name": None}],
        "walls": 6,
        "wall_thickness_mm": [200] * 6,
        "openings": [],
        "note": "6000x5000 less a 2000x2000 notch. Reflex corner on the outer face.",
    },
    "room_in_room.dxf": {
        "rooms": [
            {"area_mm2": 45_750_000.0, "name": None},
            {"area_mm2": 1_440_000.0, "name": None},
        ],
        "walls": 8,
        "wall_thickness_mm": [200, 200, 200, 200, 150, 150, 150, 150],
        "openings": [],
        "note": "Main room 48 m2 less the 1.5x1.5 shaft footprint. Shaft interior 1200x1200.",
    },
    "door_and_window.dxf": {
        "rooms": [{"area_mm2": 20_000_000.0, "name": None}],
        "walls": 4,
        "wall_thickness_mm": [200, 200, 200, 200],
        "openings": [
            {"type": "door", "block": "DOOR_900", "width_mm": 900.0, "host": "bottom"},
            {"type": "window", "block": "WIN_1200", "width_mm": 1200.0, "host": "right"},
        ],
        "note": "Block names carry the type and nominal width. Door block encodes swing as an arc.",
    },
    "labeled_rooms.dxf": {
        "rooms": [
            {"area_mm2": 12_000_000.0, "name": "Bedroom 1"},
            {"area_mm2": 9_000_000.0, "name": "Bathroom"},
        ],
        "walls": 5,
        "wall_thickness_mm": [200, 200, 200, 200, 150],
        "openings": [],
        "unassigned_text": ["NORTH ELEVATION"],
        "note": "Third TEXT sits outside every loop and must not be assigned to a room.",
    },
    "varying_thickness.dxf": {
        "rooms": [{"area_mm2": 20_000_000.0, "name": None}],
        "walls": 4,
        "wall_thickness_mm": [150, 300, 150, 300],
        "openings": [],
        "note": "Breaks the uniform-thickness assumption. Left/right 300, top/bottom 150.",
    },
    "arc_wall.dxf": {
        "rooms": [{"area_mm2": 20_000_000.0 + SEMICIRCLE_AREA, "name": None}],
        "walls": 4,
        "wall_thickness_mm": [200, 200, 200, 200],
        "openings": [],
        "note": "Right wall is a semicircular bulge, r=2000 interior / 2200 exterior, encoded as an LWPOLYLINE bulge of 1.0.",
    },
    "single_room_r12.dxf": {
        "rooms": [{"area_mm2": 20_000_000.0, "name": None}],
        "walls": 4,
        "wall_thickness_mm": [200, 200, 200, 200],
        "openings": [],
        "note": "single_room.dxf as R12 POLYLINE, no LWPOLYLINE entity available.",
    },
    "single_room_binary.dxf": {
        "rooms": [{"area_mm2": 20_000_000.0, "name": None}],
        "walls": 4,
        "wall_thickness_mm": [200, 200, 200, 200],
        "openings": [],
        "note": "single_room.dxf written as binary DXF. Must produce an identical model.",
    },
    "line_pair_room.dxf": {
        "rooms": [{"area_mm2": 20_000_000.0, "name": None}],
        "walls": 4,
        "wall_thickness_mm": [200, 200, 200, 200],
        "openings": [],
        "note": "single_room geometry drawn as 8 loose LINE entities, the common real-world idiom. No closed polyline exists.",
    },
    "single_line_two_rooms.dxf": {
        "rooms": [
            {"area_mm2": 25_000_000.0, "name": "Living"},
            {"area_mm2": 15_000_000.0, "name": "Kitchen"},
        ],
        "walls": 5,
        "wall_thickness_mm": [],
        "openings": [],
        "convention": "single-line",
        "note": "Walls are centrelines, not paired faces. Containment nesting cannot separate these rooms; only a planar arrangement can. The divider forms two T-junctions that must split the outer edges.",
    },
    "two_storey_ground.dxf": {
        "rooms": [
            {"area_mm2": 20_000_000.0, "name": "Living"},
            {"area_mm2": 11_200_000.0, "name": "STAIR"},
        ],
        "walls": 7,
        "wall_thickness_mm": [200] * 7,
        "openings": [],
        "wall_layer_filter": LAYER_WALL,
        "note": f"Ground plan of a two storey building. The stair room holds {STOREY_TREADS} tread lines on {LAYER_STRS}; those lines are geometry and must be excluded by the wall layer filter or they close into phantom rooms.",
    },
    "two_storey_first.dxf": {
        "rooms": [
            {"area_mm2": 20_000_000.0, "name": "Bedroom"},
            {"area_mm2": 11_200_000.0, "name": "STAIR"},
        ],
        "walls": 7,
        "wall_thickness_mm": [200] * 7,
        "openings": [],
        "wall_layer_filter": LAYER_WALL,
        "note": "First floor of two_storey_ground.dxf. Same outline and same stair footprint, so the two stairs must match up vertically.",
    },
    "annotated_room.dxf": {
        "rooms": [{"area_mm2": 20_000_000.0, "name": None}],
        "walls": 4,
        "wall_thickness_mm": [200] * 4,
        "openings": [],
        "wall_layer_filter": LAYER_WALL,
        "dimensions": [
            {"measurement_mm": 5000.0, "geometry_mm": 5000.0, "agrees": True},
            {"measurement_mm": 3800.0, "geometry_mm": 4000.0, "agrees": False},
        ],
        "note": "single_room plus the things a real drawing carries: a 400 square column on A-COLS, a grid line labelled A on A-GRID, and a furniture block on A-FURN. None may change the room area. The vertical DIMENSION deliberately states 3800 across a 4000 run, the signature of a drawing stretched without its dimensions being updated, and must be reported as disagreeing rather than believed.",
    },
    "hatched_room.dxf": {
        "rooms": [{"area_mm2": HATCH_ROOM_AREA, "name": None}],
        "walls": 4,
        "wall_thickness_mm": [200] * 4,
        "openings": [],
        "note": "Wall poche drawn as one HATCH: an outer boundary of four LINE edges, an inner closed polyline path making the room, and a third path for a column standing in it. Reading only the first path would give no room at all. The two path types must both be built, and the containment nesting then charges the outermost and the column as wall material. The column is 900 so its own opposite faces are further apart than a wall may be, and it does not pair into one.",
    },
    "hatch_curved_edges.dxf": {
        "rooms": [],
        "walls": 0,
        "wall_thickness_mm": [],
        "openings": [],
        "note": "One HATCH edge path using every curved edge type: two LINE edges, a semicircular ARC edge bulging right, and a half ELLIPSE edge on the left. Exercises the arc bulge and the ellipse tessellation.",
    },
    "line_pair_room_with_noise.dxf": {
        "rooms": [{"area_mm2": 20_000_000.0, "name": "Studio"}],
        "walls": 4,
        "wall_thickness_mm": [200, 200, 200, 200],
        "openings": [],
        "note": "line_pair_room plus a dimension line, a leader tick and a furniture rectangle on A-ANNO. Only A-WALL may contribute geometry.",
    },
}

BUILDERS = [
    ("single_room.dxf", single_room),
    ("two_rooms_shared_wall.dxf", two_rooms_shared_wall),
    ("l_shaped_room.dxf", l_shaped_room),
    ("room_in_room.dxf", room_in_room),
    ("door_and_window.dxf", door_and_window),
    ("labeled_rooms.dxf", lambda p: two_rooms_shared_wall(p, with_labels=True)),
    ("varying_thickness.dxf", varying_thickness),
    ("arc_wall.dxf", arc_wall),
    ("single_room_r12.dxf", single_room_r12),
    ("single_room_binary.dxf", single_room_binary),
    ("line_pair_room.dxf", line_pair_room),
    ("line_pair_room_with_noise.dxf", line_pair_room_with_noise),
    ("single_line_two_rooms.dxf", single_line_two_rooms),
    ("two_storey_ground.dxf", two_storey_ground),
    ("two_storey_first.dxf", two_storey_first),
    ("annotated_room.dxf", annotated_room),
    ("hatched_room.dxf", hatched_room),
    ("hatch_curved_edges.dxf", hatch_curved_edges),
]


def main():
    target = Path(__file__).resolve().parent.parent / "Fixtures"
    target.mkdir(parents=True, exist_ok=True)

    for name, builder in BUILDERS:
        path = target / name
        builder(str(path))
        print(f"  {name}  {path.stat().st_size:,} bytes")

    for name, spec in EXPECTED.items():
        for room in spec["rooms"]:
            room["area_m2"] = round(room["area_mm2"] / 1_000_000.0, 9)
        spec["wall_footprint_mm2"] = WALL_FOOTPRINT[name]

    manifest = {
        "units": "mm",
        "wall_representation": "double line: separate closed faces on layer A-WALL",
        "fixtures": EXPECTED,
    }
    expected_path = target / "expected.json"
    expected_path.write_text(json.dumps(manifest, indent=2), encoding="utf-8")
    print(f"  expected.json  {expected_path.stat().st_size:,} bytes")

    missing = set(n for n, _ in BUILDERS) - set(EXPECTED)
    if missing:
        print(f"\nFixtures with no expected values: {sorted(missing)}")
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
