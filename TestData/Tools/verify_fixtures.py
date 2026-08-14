import json
import math
import sys
from pathlib import Path

import ezdxf

AREA_TOLERANCE_MM2 = 1.0
CONTAINMENT_TOLERANCE_MM = 1e-6


def read_faces(path):
    doc = ezdxf.readfile(path)
    faces = []
    for entity in doc.modelspace():
        if entity.dxftype() == "LWPOLYLINE":
            raw = entity.get_points()
            points = [(p[0], p[1]) for p in raw]
            bulges = [p[4] for p in raw]
        elif entity.dxftype() == "POLYLINE":
            points = [(v.dxf.location.x, v.dxf.location.y) for v in entity.vertices]
            bulges = [0.0] * len(points)
        else:
            continue
        faces.append(Face(points, bulges))
    return doc.dxfversion, faces


class Face:
    def __init__(self, points, bulges):
        self.points = points
        self.bulges = bulges
        self.area = shoelace(points) + bulge_area(points, bulges)
        xs = [p[0] for p in points]
        ys = [p[1] for p in points]
        self.min = (min(xs), min(ys))
        self.max = (max(xs), max(ys))

    def contains(self, other):
        if other is self:
            return False
        return (
            other.min[0] >= self.min[0] - CONTAINMENT_TOLERANCE_MM
            and other.min[1] >= self.min[1] - CONTAINMENT_TOLERANCE_MM
            and other.max[0] <= self.max[0] + CONTAINMENT_TOLERANCE_MM
            and other.max[1] <= self.max[1] + CONTAINMENT_TOLERANCE_MM
            and other.area < self.area
        )


def shoelace(points):
    total = 0.0
    for index in range(len(points)):
        x1, y1 = points[index]
        x2, y2 = points[(index + 1) % len(points)]
        total += x1 * y2 - x2 * y1
    return abs(total) / 2.0


def bulge_area(points, bulges):
    extra = 0.0
    for index, bulge in enumerate(bulges):
        if abs(bulge) < 1e-12:
            continue
        x1, y1 = points[index]
        x2, y2 = points[(index + 1) % len(points)]
        chord = math.hypot(x2 - x1, y2 - y1)
        theta = 4.0 * math.atan(abs(bulge))
        radius = chord / (2.0 * math.sin(theta / 2.0))
        segment = radius * radius / 2.0 * (theta - math.sin(theta))
        extra += segment if bulge > 0 else -segment
    return extra


def net_areas(faces):
    results = []
    for face in faces:
        enclosed = [other for other in faces if face.contains(other)]
        immediate = [
            other
            for other in enclosed
            if not any(mid.contains(other) for mid in enclosed)
        ]
        results.append((face, face.area - sum(child.area for child in immediate)))
    return results


def match(value, candidates):
    for index, candidate in enumerate(candidates):
        if abs(candidate - value) <= AREA_TOLERANCE_MM2:
            return index
    return -1


def main():
    root = Path(__file__).resolve().parent.parent / "Fixtures"
    expected = json.loads((root / "expected.json").read_text(encoding="utf-8"))

    failures = 0
    for name, spec in expected["fixtures"].items():
        path = root / name
        if not path.exists():
            print(f"MISSING  {name}")
            failures += 1
            continue

        version, faces = read_faces(str(path))
        if not faces:
            print(f"\n{name}  [{version}]  no closed faces")
            print("    SKIP loose-segment fixture; verified by the C++ loop assembler tests")
            continue
        cells = net_areas(faces)
        available = [net for _, net in cells]

        print(f"\n{name}  [{version}]  {len(faces)} faces")
        for face, net in sorted(cells, key=lambda row: row[1]):
            curved = "  arc" if any(abs(b) > 1e-12 for b in face.bulges) else ""
            print(
                f"    net {net:>16,.3f} mm2   gross {face.area:>16,.3f}   "
                f"bbox ({face.min[0]:.0f},{face.min[1]:.0f})-"
                f"({face.max[0]:.0f},{face.max[1]:.0f}){curved}"
            )

        remaining = list(available)
        for room in spec["rooms"]:
            want = room["area_mm2"]
            index = match(want, remaining)
            if index < 0:
                print(f"    FAIL room area {want:,.3f} mm2 not present as a net cell")
                failures += 1
            else:
                print(f"    OK   room area {want:,.3f} mm2")
                remaining.pop(index)

        footprint = spec.get("wall_footprint_mm2")
        if footprint is not None:
            total = sum(net for _, net in cells) - sum(
                room["area_mm2"] for room in spec["rooms"]
            )
            if abs(total - footprint) <= AREA_TOLERANCE_MM2:
                print(f"    OK   wall footprint {footprint:,.3f} mm2")
            else:
                print(
                    f"    FAIL wall footprint expected {footprint:,.3f}, "
                    f"measured {total:,.3f} mm2"
                )
                failures += 1

    print(f"\n{failures} failure(s)")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
