"""Shape of the small alien planet, shared by Blender (mesh) and the Unreal build script (placement).

Pure Python, no Blender/Unreal imports. Coordinates are Unreal's (centimetres, left-handed, Z up), with
the planet centred on the origin. Blender works in metres with Y mirrored: a Blender vertex direction
(bx, by, bz) is the Unreal direction (bx, -by, bz) -- see make_alien.py.

The north pole (0, 0, 1) is the flat landing site where the ship stands and KDS-3-21 spawns.
"""
import math

RADIUS_CM = 4500.0     # 45 m: ~280 m around, the horizon is ~15 m away at droid height
SUBDIV = 8             # Blender icosphere subdivisions (1 = icosahedron) -> 327k triangles, ~40 cm edges
UV_TILE_CM = 400.0     # ground texture repeats every 4 m
LANDING_ANGLE = 0.16   # radians around the north pole kept flat (~7 m)

# (direction, angular radius in rad, depth cm). Fixed list so Blender and Unreal agree.
CRATERS = [
    ((0.62, 0.35, 0.70), 0.20, 170),
    ((-0.55, 0.62, 0.55), 0.15, 130),
    ((0.10, -0.85, 0.50), 0.24, 200),
    ((-0.80, -0.30, 0.10), 0.18, 150),
    ((0.90, 0.10, -0.20), 0.26, 220),
    ((0.20, 0.95, -0.15), 0.14, 110),
    ((-0.30, 0.40, -0.85), 0.30, 240),
    ((0.45, -0.50, -0.70), 0.17, 140),
    ((-0.70, -0.65, -0.30), 0.12, 100),
    ((0.05, 0.10, -1.00), 0.10, 90),
    ((-0.95, 0.20, -0.25), 0.11, 95),
    ((0.65, 0.70, 0.05), 0.09, 80),
]


def _norm(v):
    l = math.sqrt(v[0] ** 2 + v[1] ** 2 + v[2] ** 2)
    return (v[0] / l, v[1] / l, v[2] / l)


_CRATERS = [(_norm(d), a, depth) for d, a, depth in CRATERS]


def _smoothstep(a, b, t):
    t = max(0.0, min(1.0, (t - a) / (b - a)))
    return t * t * (3 - 2 * t)


def _hills(x, y, z):
    """Smooth rolling relief from a few products of sines (cheap, deterministic 3D 'noise')."""
    return (110 * math.sin(2.1 * x + 0.4) * math.cos(1.7 * y - 0.8) * math.sin(1.9 * z + 1.1)
            + 55 * math.sin(4.3 * y + 1.3) * math.cos(3.7 * z - 0.2)
            + 40 * math.cos(5.1 * x - 0.7) * math.sin(4.7 * z + 0.9)
            + 22 * math.sin(9.3 * x + 2.0) * math.sin(8.9 * y - 1.0) * math.cos(9.7 * z)
            + 10 * math.sin(17.0 * x + 0.3) * math.cos(15.0 * y + 1.7))


def offset_cm(d):
    """Surface height above RADIUS_CM along unit direction d."""
    x, y, z = d
    h = _hills(x, y, z)
    for c, a, depth in _CRATERS:
        ang = math.acos(max(-1.0, min(1.0, x * c[0] + y * c[1] + z * c[2])))
        if ang > a * 1.6:
            continue
        t = ang / a
        if t < 1.0:
            h -= depth * (1 - t * t) ** 1.5          # bowl
        h += depth * 0.35 * math.exp(-((t - 1.0) / 0.18) ** 2)   # raised rim
    pole = math.acos(max(-1.0, min(1.0, z)))
    h *= _smoothstep(LANDING_ANGLE * 0.6, LANDING_ANGLE * 2.2, pole)   # flat landing site
    return h


def radius_cm(d):
    d = _norm(d)
    return RADIUS_CM + offset_cm(d)


def surface(d):
    """(point, normal) on the surface along direction d (any length)."""
    d = _norm(d)
    r = radius_cm(d)
    p = (d[0] * r, d[1] * r, d[2] * r)
    # normal from two tangent finite differences
    ref = (1.0, 0.0, 0.0) if abs(d[0]) < 0.9 else (0.0, 1.0, 0.0)
    t1 = _norm((d[1] * ref[2] - d[2] * ref[1], d[2] * ref[0] - d[0] * ref[2], d[0] * ref[1] - d[1] * ref[0]))
    t2 = (d[1] * t1[2] - d[2] * t1[1], d[2] * t1[0] - d[0] * t1[2], d[0] * t1[1] - d[1] * t1[0])
    e = 0.004

    def at(a, b):
        q = _norm((d[0] + a * t1[0] + b * t2[0], d[1] + a * t1[1] + b * t2[1], d[2] + a * t1[2] + b * t2[2]))
        rr = radius_cm(q)
        return (q[0] * rr, q[1] * rr, q[2] * rr)

    pa, pb, pc, pd = at(e, 0), at(-e, 0), at(0, e), at(0, -e)
    u = (pa[0] - pb[0], pa[1] - pb[1], pa[2] - pb[2])
    v = (pc[0] - pd[0], pc[1] - pd[1], pc[2] - pd[2])
    n = _norm((u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0]))
    if n[0] * d[0] + n[1] * d[1] + n[2] * d[2] < 0:
        n = (-n[0], -n[1], -n[2])
    return p, n
