"""Blender (headless): model the alien planet, its flora, the crashed ship and its 5 parts; export glTF.

Run:  blender -b --factory-startup --python Tools/make_alien.py -- <project_dir> [only_name ...]

Everything is procedural and original. Units are metres (Unreal import -> cm). Material slot names
(M_Alien_<Name>) drive the Unreal-side material assignment in Tools/build_alien.py.
Output: SourceAssets/prepared/<name>/<name>.gltf, plus SourceAssets/prepared/ship_sockets.json.
"""
import json
import math
import os
import random
import sys

import bmesh
import bpy
from mathutils import Euler, Matrix, Vector

TOOLS = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, TOOLS)
import planet  # noqa: E402

ARGS = sys.argv[sys.argv.index("--") + 1:]
PROJECT = os.path.abspath(ARGS[0])
ONLY = set(ARGS[1:])
OUT = os.path.join(PROJECT, "SourceAssets", "prepared")

_mats = {}


def reset():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    _mats.clear()


def mat(name):
    key = f"M_Alien_{name}"
    if key not in _mats:
        _mats[key] = bpy.data.materials.new(key)
    return _mats[key]


def active():
    return bpy.context.view_layer.objects.active


def paint(obj, name):
    obj.data.materials.clear()
    obj.data.materials.append(mat(name))
    return obj


def smooth(obj):
    for p in obj.data.polygons:
        p.use_smooth = True
    return obj


def orient(obj, direction, roll=0.0):
    q = Vector((0, 0, 1)).rotation_difference(Vector(direction).normalized())
    obj.rotation_mode = "QUATERNION"
    obj.rotation_quaternion = q @ Euler((0, 0, roll)).to_quaternion()


def join(objs, name):
    bpy.ops.object.select_all(action="DESELECT")
    for o in objs:
        o.select_set(True)
    bpy.context.view_layer.objects.active = objs[0]
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    if len(objs) > 1:
        bpy.ops.object.join()
    o = active()
    o.name = name
    return o


def export(obj, name):
    if ONLY and name not in ONLY:
        return
    d = os.path.join(OUT, name)
    os.makedirs(d, exist_ok=True)
    bpy.ops.object.select_all(action="DESELECT")
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    bpy.ops.export_scene.gltf(filepath=os.path.join(d, f"{name}.gltf"), export_format="GLTF_SEPARATE",
                              use_selection=True, export_apply=True, export_yup=True)
    print(f"ALIEN {name}: {len(obj.data.polygons)} polys, materials={[m.name for m in obj.data.materials]}")


def cyl(r, h, z0=0.0, verts=16, r2=None):
    """Cylinder/cone standing on z0 along +Z."""
    if r2 is None:
        bpy.ops.mesh.primitive_cylinder_add(vertices=verts, radius=r, depth=h, location=(0, 0, z0 + h / 2))
    else:
        bpy.ops.mesh.primitive_cone_add(vertices=verts, radius1=r, radius2=r2, depth=h, location=(0, 0, z0 + h / 2))
    return active()


def sphere(r, loc=(0, 0, 0), seg=16, rings=8):
    bpy.ops.mesh.primitive_uv_sphere_add(segments=seg, ring_count=rings, radius=r, location=loc)
    return active()


def place(obj, direction=(0, 0, 1), offset=(0, 0, 0), roll=0.0):
    """Rotate obj (built standing on the origin) to `direction`, then move by offset; bake transform."""
    orient(obj, direction, roll)
    obj.location = offset
    return join([obj], obj.name)


def tilted(rng, max_deg):
    a, b = math.radians(rng.uniform(0, max_deg)), rng.uniform(0, 2 * math.pi)
    return Vector((math.sin(a) * math.cos(b), math.sin(a) * math.sin(b), math.cos(a)))


def tube(points, radii, name, res=10):
    """Open poly-curve tube through points with per-point radius (metres)."""
    cu = bpy.data.curves.new(name, "CURVE")
    cu.dimensions = "3D"
    cu.bevel_depth = 1.0
    cu.bevel_resolution = 2
    cu.use_fill_caps = True
    sp = cu.splines.new("POLY")
    sp.points.add(len(points) - 1)
    for p, q, r in zip(sp.points, points, radii):
        p.co = (*q, 1)
        p.radius = r
    ob = bpy.data.objects.new(name, cu)
    bpy.context.scene.collection.objects.link(ob)
    bpy.ops.object.select_all(action="DESELECT")
    bpy.context.view_layer.objects.active = ob
    ob.select_set(True)
    bpy.ops.object.convert(target="MESH")
    return active()


# ----------------------------------------------------------------------------- planet

def make_planet():
    reset()
    bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=planet.SUBDIV, radius=1.0)
    ob = active()
    me = ob.data
    for v in me.vertices:
        d = v.co.normalized()
        v.co = d * (planet.radius_cm((d.x, -d.y, d.z)) / 100.0)   # Blender Y = -Unreal Y
    me.update()
    smooth(ob)
    # box-projected UVs: each face uses the two axes perpendicular to its dominant normal axis
    bm = bmesh.new()
    bm.from_mesh(me)
    uv = bm.loops.layers.uv.new("UVMap")
    tile = planet.UV_TILE_CM / 100.0
    for f in bm.faces:
        n = f.normal
        ax = max(range(3), key=lambda i: abs(n[i]))
        a, b = [i for i in range(3) if i != ax]
        for loop in f.loops:
            co = loop.vert.co
            loop[uv].uv = (co[a] / tile, co[b] / tile)
    bm.to_mesh(me)
    bm.free()
    paint(ob, "Ground")
    ob.name = "planet"
    return ob


# ----------------------------------------------------------------------------- flora

def make_crystals(name, glow, seed):
    reset()
    rng = random.Random(seed)
    parts = []
    n = rng.randint(6, 9)
    for i in range(n):
        h = rng.uniform(1.1, 1.8) if i == 0 else rng.uniform(0.35, 1.2)
        r = h * rng.uniform(0.09, 0.14)
        prism = cyl(r, h, z0=-0.1, verts=6)
        tip = cyl(r, r * 2.4, z0=h - 0.1, verts=6, r2=0.0)
        c = join([prism, tip], f"c{i}")
        paint(c, glow)
        off = Vector((rng.uniform(-1, 1), rng.uniform(-1, 1), 0)).normalized() * (0 if i == 0 else rng.uniform(0.12, 0.45))
        parts.append(place(c, tilted(rng, 8 if i == 0 else 38), off, rng.uniform(0, 1)))
    base = sphere(0.45, seg=10, rings=6)
    base.scale = (1.1, 0.9, 0.35)
    parts.append(paint(base, "RockDark"))
    return join(parts, name)


def make_shrooms(seed):
    reset()
    rng = random.Random(seed)
    parts = []
    for i in range(rng.randint(3, 5)):
        h = rng.uniform(0.9, 1.6) if i == 0 else rng.uniform(0.3, 0.9)
        cap_r = h * rng.uniform(0.35, 0.5)
        stalk = smooth(cyl(h * 0.07, h, verts=12, r2=h * 0.045))
        paint(stalk, "Stalk")
        cap = sphere(cap_r, loc=(0, 0, h), seg=24, rings=12)
        cap.scale = (1, 1, 0.5)
        smooth(paint(cap, "Cap"))
        gills = cyl(cap_r * 0.92, 0.02, z0=h - 0.015, verts=24)
        paint(gills, "GlowCyan")
        bits = [stalk, cap, gills]
        for k in range(rng.randint(5, 9)):   # glowing spots on the cap
            a, e = rng.uniform(0, 2 * math.pi), rng.uniform(0.25, 1.2)
            p = Vector((cap_r * math.cos(e) * math.cos(a), cap_r * math.cos(e) * math.sin(a),
                        h + cap_r * 0.5 * math.sin(e)))
            bits.append(paint(sphere(cap_r * rng.uniform(0.06, 0.11), loc=p, seg=8, rings=4), "GlowCyan"))
        m = join(bits, f"m{i}")
        off = Vector((rng.uniform(-1, 1), rng.uniform(-1, 1), 0)).normalized() * (0 if i == 0 else rng.uniform(0.25, 0.6))
        parts.append(place(m, tilted(rng, 4 if i == 0 else 18), off, rng.uniform(0, 6)))
    return join(parts, "alien_shrooms")


def make_tubes(seed):
    reset()
    rng = random.Random(seed)
    parts = []
    for i in range(rng.randint(5, 9)):
        h, r = rng.uniform(0.35, 1.4), rng.uniform(0.045, 0.09)
        body = smooth(paint(cyl(r, h, z0=-0.05, verts=14, r2=r * 1.25), "TubeSkin"))
        bpy.ops.mesh.primitive_torus_add(major_radius=r * 1.25, minor_radius=r * 0.22, major_segments=14,
                                         minor_segments=6, location=(0, 0, h - 0.05))
        rim = smooth(paint(active(), "GlowLime"))
        inner = paint(cyl(r * 1.1, 0.01, z0=h - 0.08, verts=14), "TubeInner")
        t = join([body, rim, inner], f"t{i}")
        off = Vector((rng.uniform(-1, 1), rng.uniform(-1, 1), 0)).normalized() * rng.uniform(0.0, 0.3)
        parts.append(place(t, tilted(rng, 22), off, 0))
    return join(parts, "alien_tubes")


def make_tree(seed):
    reset()
    rng = random.Random(seed)
    h = rng.uniform(4.0, 5.5)
    lean = Vector((rng.uniform(-1, 1), rng.uniform(-1, 1), 0)).normalized() * rng.uniform(0.6, 1.2)
    pts, radii = [], []
    for k in range(13):
        t = k / 12
        p = Vector((0, 0, h * t)) + lean * (math.sin(t * math.pi * 0.8) * 0.9 - 0.2 * t) \
            + Vector((math.sin(t * 9 + seed) * 0.08, math.cos(t * 7 + seed) * 0.08, 0))
        pts.append(p)
        radii.append(0.22 * (1 - t) + 0.07)
    trunk = smooth(paint(tube(pts, radii, "trunk"), "Bark"))
    top = pts[-1]
    pod = sphere(0.45, loc=top + Vector((0, 0, 0.25)), seg=24, rings=12)
    pod.scale = (1, 1, 1.25)
    smooth(paint(pod, "GlowPod"))
    parts = [trunk, pod]
    for k in range(rng.randint(5, 7)):   # ribbed leaves curling out under the pod
        a = 2 * math.pi * k / 6 + rng.uniform(-0.3, 0.3)
        out = Vector((math.cos(a), math.sin(a), 0))
        lp = [top + out * (0.25 + 0.5 * s) + Vector((0, 0, 0.35 * math.sin(s * math.pi) - 0.9 * s * s))
              for s in (0, 0.2, 0.4, 0.6, 0.8, 1.0)]
        parts.append(smooth(paint(tube(lp, [0.09, 0.11, 0.1, 0.08, 0.05, 0.02], f"leaf{k}"), "Leaf")))
        parts.append(paint(sphere(0.05, loc=lp[-1], seg=8, rings=4), "GlowPink"))
    for k in range(3):   # roots
        a = 2 * math.pi * k / 3 + rng.uniform(-0.4, 0.4)
        out = Vector((math.cos(a), math.sin(a), 0))
        rp = [out * 0.05 + Vector((0, 0, 0.5)), out * 0.35 + Vector((0, 0, 0.2)), out * 0.7 + Vector((0, 0, -0.1))]
        parts.append(smooth(paint(tube(rp, [0.12, 0.09, 0.05], f"root{k}"), "Bark")))
    return join(parts, f"alien_tree_{seed}")


def make_spikes(seed):
    reset()
    rng = random.Random(seed)
    parts = []
    for i in range(rng.randint(9, 14)):
        h = rng.uniform(0.18, 0.5)
        c = paint(cyl(0.018, h, z0=-0.02, verts=4, r2=0.0), "Blade")
        off = Vector((rng.uniform(-0.12, 0.12), rng.uniform(-0.12, 0.12), 0))
        parts.append(place(c, tilted(rng, 35), off, rng.uniform(0, 3)))
    return join(parts, f"alien_spikes")


# ----------------------------------------------------------------------------- ship + parts

HULL = [(0.0, 0.72), (0.55, 0.72), (0.88, 1.0), (1.04, 1.6), (1.1, 2.4), (1.02, 3.2),
        (0.84, 3.95), (0.56, 4.6), (0.26, 5.1), (0.0, 5.38)]


def hull_r(z):
    for (r0, z0), (r1, z1) in zip(HULL, HULL[1:]):
        if z0 <= z <= z1 and z1 > z0:
            return r0 + (r1 - r0) * (z - z0) / (z1 - z0)
    return 0.0


def lathe(profile, segs, name):
    me = bpy.data.meshes.new(name)
    bm = bmesh.new()
    rings = []
    for r, z in profile:
        ring = []
        for s in range(segs):
            a = 2 * math.pi * s / segs
            ring.append(bm.verts.new((r * math.cos(a), r * math.sin(a), z)))
        rings.append(ring)
    for ra, rb in zip(rings, rings[1:]):
        for s in range(segs):
            t = (s + 1) % segs
            try:
                bm.faces.new((ra[s], ra[t], rb[t], rb[s]))
            except ValueError:
                pass
    bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=1e-5)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    bm.to_mesh(me)
    bm.free()
    ob = bpy.data.objects.new(name, me)
    bpy.context.scene.collection.objects.link(ob)
    bpy.context.view_layer.objects.active = ob
    return smooth(ob)


# (part asset, display angle deg around the hull in Blender, height m). Index order = C++ part index.
SOCKETS = [("part_power_core", 90.0, 2.05), ("part_nav_computer", 210.0, 2.7), ("part_thruster", 330.0, 1.45),
           ("part_fuel_cell", 150.0, 1.55), ("part_hyper_crystal", None, 5.55)]


def socket_transform(angle, z):
    if angle is None:
        return Vector((0, 0, z)), 0.0
    a = math.radians(angle)
    r = hull_r(z) + 0.2
    return Vector((r * math.cos(a), r * math.sin(a), z)), a


def make_ship():
    reset()
    parts = [paint(lathe(HULL, 64, "hull"), "ShipHull")]
    for z, w in ((1.6, 0.05), (3.2, 0.05), (4.85, 0.04)):
        bpy.ops.mesh.primitive_torus_add(major_radius=hull_r(z) + 0.01, minor_radius=w, major_segments=64,
                                         minor_segments=8, location=(0, 0, z))
        parts.append(smooth(paint(active(), "ShipTrim")))
    # porthole facing +X
    p = Vector((hull_r(3.6), 0, 3.6))
    n = Vector((1, 0, 0.35)).normalized()
    bpy.ops.mesh.primitive_torus_add(major_radius=0.3, minor_radius=0.06, major_segments=32, minor_segments=8)
    ring = active(); orient(ring, n); ring.location = p
    parts.append(smooth(paint(ring, "ShipDark")))
    glass = sphere(0.27, seg=24, rings=12); glass.scale = (1, 1, 0.35); orient(glass, n); glass.location = p
    parts.append(smooth(paint(glass, "ShipGlass")))
    # fins + legs
    for k in range(3):
        a = math.radians(k * 120 + 30)
        bpy.ops.mesh.primitive_cube_add(size=1)
        fin = active(); fin.scale = (0.85, 0.08, 1.3)
        fin.rotation_euler = (0, math.radians(-18), a)
        fin.location = (math.cos(a) * 1.25, math.sin(a) * 1.25, 1.25)
        parts.append(paint(fin, "ShipTrim"))
        b = math.radians(k * 120 + 90)
        top = Vector((math.cos(b) * 0.85, math.sin(b) * 0.85, 1.05))
        foot = Vector((math.cos(b) * 1.75, math.sin(b) * 1.75, 0.06))
        parts.append(smooth(paint(tube([top, (top + foot) / 2 + Vector((0, 0, 0.1)), foot], [0.07, 0.065, 0.06],
                                       f"leg{k}"), "ShipDark")))
        f = cyl(0.22, 0.08, verts=16); f.location = foot - Vector((0, 0, 0.06))
        parts.append(paint(f, "ShipDark"))
    nozzle = smooth(cyl(0.6, 0.5, z0=0.25, verts=32, r2=0.38))
    parts.append(paint(nozzle, "ShipDark"))
    # empty part bays: dark recessed panels with a red warning light
    for name, angle, z in SOCKETS:
        pos, a = socket_transform(angle, z)
        if angle is None:
            continue
        bpy.ops.mesh.primitive_cube_add(size=1)
        bay = active(); bay.scale = (0.08, 0.55, 0.55)
        bay.rotation_euler = (0, 0, a)
        bay.location = Vector((math.cos(a), math.sin(a), 0)) * (hull_r(z) - 0.02) + Vector((0, 0, z))
        parts.append(paint(bay, "ShipDark"))
    ship = join(parts, "ship")
    sockets = []
    for name, angle, z in SOCKETS:
        pos, a = socket_transform(angle, z)
        # Unreal: cm, Y mirrored, yaw mirrored
        sockets.append({"part": name, "loc_cm": [pos.x * 100, -pos.y * 100, pos.z * 100],
                        "yaw_deg": -math.degrees(a)})
    with open(os.path.join(OUT, "ship_sockets.json"), "w") as fh:
        json.dump(sockets, fh, indent=1)
    return ship


def make_flame():
    reset()
    f = smooth(cyl(0.0, 2.4, z0=-2.4, verts=24, r2=0.5))   # wide end at z=0, tip 2.4 m below
    f.name = "ship_flame"
    return paint(f, "Flame")


def make_parts():
    # 1 power core: glowing cell in a cage
    reset()
    bits = [paint(smooth(cyl(0.11, 0.38, z0=-0.19, verts=20)), "GlowCyan")]
    for z in (-0.22, 0.19):
        bits.append(paint(cyl(0.17, 0.04, z0=z, verts=24), "ShipTrim"))
    for k in range(4):
        a = k * math.pi / 2
        b = cyl(0.018, 0.4, z0=-0.2, verts=8); b.location = (0.15 * math.cos(a), 0.15 * math.sin(a), 0)
        bits.append(paint(b, "ShipDark"))
    export(join(bits, "part_power_core"), "part_power_core")
    # 2 nav computer: box with a screen and an antenna
    reset()
    bpy.ops.mesh.primitive_cube_add(size=1); box = active(); box.scale = (0.32, 0.42, 0.3)
    bits = [paint(box, "ShipHull")]
    bpy.ops.mesh.primitive_cube_add(size=1); scr = active(); scr.scale = (0.02, 0.32, 0.18); scr.location = (0.165, 0, 0.02)
    bits.append(paint(scr, "GlowLime"))
    ant = cyl(0.01, 0.3, z0=0.15, verts=8); ant.location.y = 0.14
    bits.append(paint(ant, "ShipDark"))
    bits.append(paint(sphere(0.035, loc=(0, 0.14, 0.47), seg=10, rings=5), "GlowPink"))
    export(join(bits, "part_nav_computer"), "part_nav_computer")
    # 3 thruster nozzle: bell with a hot core
    reset()
    bits = [paint(smooth(lathe([(0.1, 0.2), (0.12, 0.12), (0.18, 0.0), (0.27, -0.2), (0.24, -0.2), (0.15, 0.0),
                                (0.09, 0.12), (0.0, 0.12)], 32, "bell")), "ShipDark")]
    bpy.ops.mesh.primitive_torus_add(major_radius=0.14, minor_radius=0.025, major_segments=24, minor_segments=6,
                                     location=(0, 0, 0.05))
    bits.append(paint(active(), "ShipTrim"))
    bits.append(paint(cyl(0.2, 0.01, z0=-0.12, verts=24), "GlowOrange"))
    export(join(bits, "part_thruster"), "part_thruster")
    # 4 fuel cell: capsule tank with glowing bands
    reset()
    body = smooth(cyl(0.14, 0.4, z0=-0.2, verts=24))
    ends = [smooth(sphere(0.14, loc=(0, 0, z), seg=24, rings=12)) for z in (-0.2, 0.2)]
    bits = [paint(o, "ShipHull") for o in [body] + ends]
    for z in (-0.1, 0.0, 0.1):
        bpy.ops.mesh.primitive_torus_add(major_radius=0.142, minor_radius=0.014, major_segments=24,
                                         minor_segments=6, location=(0, 0, z))
        bits.append(paint(active(), "GlowLime"))
    export(join(bits, "part_fuel_cell"), "part_fuel_cell")
    # 5 hyperdrive crystal: double-pointed gem in a holder ring
    reset()
    up = cyl(0.13, 0.32, z0=0.0, verts=6, r2=0.0)
    dn = cyl(0.13, 0.22, z0=-0.22, verts=6, r2=0.0); dn.scale = (1, 1, 1)
    dn.rotation_euler = (math.pi, 0, 0); dn.location = (0, 0, -0.11)
    bits = [paint(up, "GlowViolet"), paint(dn, "GlowViolet")]
    bpy.ops.mesh.primitive_torus_add(major_radius=0.15, minor_radius=0.02, major_segments=24, minor_segments=6)
    bits.append(paint(active(), "ShipTrim"))
    export(join(bits, "part_hyper_crystal"), "part_hyper_crystal")



def make_gas_giant():
    reset()
    bpy.ops.mesh.primitive_uv_sphere_add(segments=96, ring_count=48, radius=1.0)
    g = smooth(paint(active(), "GasGiant"))
    g.name = "gas_giant"
    return g


def make_ring():
    reset()
    me = bpy.data.meshes.new("gas_ring")
    bm = bmesh.new()
    uv = bm.loops.layers.uv.new("UVMap")
    segs, r0, r1 = 128, 1.35, 2.3
    inner = [bm.verts.new((r0 * math.cos(2 * math.pi * s / segs), r0 * math.sin(2 * math.pi * s / segs), 0)) for s in range(segs)]
    outer = [bm.verts.new((r1 * math.cos(2 * math.pi * s / segs), r1 * math.sin(2 * math.pi * s / segs), 0)) for s in range(segs)]
    for s in range(segs):
        t = (s + 1) % segs
        f = bm.faces.new((inner[s], inner[t], outer[t], outer[s]))
        for loop in f.loops:
            loop[uv].uv = (0.0 if loop.vert in inner else 1.0, s / segs)
    bm.to_mesh(me)
    bm.free()
    ob = bpy.data.objects.new("gas_ring", me)
    bpy.context.scene.collection.objects.link(ob)
    bpy.context.view_layer.objects.active = ob
    return paint(ob, "GasRing")


# ----------------------------------------------------------------------------- hazards + power-ups

def ribbed(r, h, z0=0.0, ribs=8, depth=0.18):
    """Cactus-like ribbed column with a rounded top."""
    c = cyl(r, h, z0=z0, verts=ribs * 2)
    for v in c.data.vertices:
        a = math.atan2(v.co.y, v.co.x)
        k = 1.0 - depth * (0.5 + 0.5 * math.cos(a * ribs))
        v.co.x *= k
        v.co.y *= k
    top = sphere(r * (1 - depth * 0.5), loc=(0, 0, z0 + h), seg=ribs * 2, rings=8)
    top.scale = (1, 1, 0.7)
    return join([c, top], "rib")


def make_cactus(seed):
    reset()
    rng = random.Random(seed)
    h, r = rng.uniform(1.4, 1.9), rng.uniform(0.2, 0.26)
    body = [smooth(ribbed(r, h, z0=-0.05))]
    for k in range(rng.randint(1, 2)):   # arms: out, then up
        a = rng.uniform(0, 2 * math.pi) + k * math.pi
        z = rng.uniform(0.5, 0.9)
        d = Vector((math.cos(a), math.sin(a), 0))
        ar = r * 0.6
        arm_out = smooth(cyl(ar, 0.35, verts=12))
        orient(arm_out, d); arm_out.location = d * (r * 0.6) + Vector((0, 0, z))
        arm_up = smooth(ribbed(ar, rng.uniform(0.35, 0.6), z0=0.0))
        arm_up.location = d * (r * 0.6 + 0.35) + Vector((0, 0, z - ar * 0.3))
        body += [join([arm_out], "ao"), join([arm_up], "au")]
    body = join(body, "cactus_body")
    paint(body, "CactusSkin")
    parts = [body]
    # spines: small cones pointing out of the surface (sampled from body vertices)
    verts = [v.co.copy() for v in body.data.vertices]
    norms = [v.normal.copy() for v in body.data.vertices]
    for i in rng.sample(range(len(verts)), min(70, len(verts))):
        n = norms[i]
        if n.length < 0.5:
            continue
        sp = cyl(0.012, rng.uniform(0.07, 0.12), verts=3, r2=0.0)
        parts.append(paint(place(sp, n, verts[i] - n * 0.01, 0), "Spine"))
    bud = sphere(0.07, loc=(0, 0, h + r * 0.55), seg=12, rings=6)
    parts.append(paint(bud, "GlowPink"))
    return join(parts, f"alien_cactus_{seed}")


def make_spike_cluster(seed):
    reset()
    rng = random.Random(seed)
    parts = []
    for i in range(rng.randint(7, 11)):
        h = rng.uniform(0.25, 0.55)
        dark = paint(cyl(0.035, h * 0.8, z0=-0.03, verts=5, r2=0.012), "SpikeDark")
        tip = paint(cyl(0.012, h * 0.2, z0=h * 0.8 - 0.03, verts=5, r2=0.0), "GlowRed")
        s = join([dark, tip], f"s{i}")
        off = Vector((rng.uniform(-0.25, 0.25), rng.uniform(-0.25, 0.25), 0))
        parts.append(place(s, tilted(rng, 30), off, rng.uniform(0, 3)))
    base = sphere(0.3, seg=10, rings=5); base.scale = (1, 1, 0.25)
    parts.append(paint(base, "SpikeDark"))
    return join(parts, "spike_cluster")


def make_lava_pool():
    reset()
    bpy.ops.mesh.primitive_circle_add(vertices=48, radius=1.0, fill_type="TRIFAN")
    lava = active()
    bpy.ops.object.mode_set(mode="EDIT")
    bpy.ops.mesh.subdivide(number_cuts=3)
    bpy.ops.object.mode_set(mode="OBJECT")
    bm = bmesh.new(); bm.from_mesh(lava.data)
    uv = bm.loops.layers.uv.new("UVMap")
    for f in bm.faces:
        for loop in f.loops:
            loop[uv].uv = (loop.vert.co.x * 0.5 + 0.5, loop.vert.co.y * 0.5 + 0.5)
    bm.to_mesh(lava.data); bm.free()
    paint(lava, "Lava")
    lava.location.z = 0.04
    rng = random.Random(9)
    crust = []
    for k in range(20):   # broken dark crust around the rim
        a = 2 * math.pi * k / 20
        b = sphere(rng.uniform(0.13, 0.2), seg=8, rings=4)
        b.scale = (1.6, 1.0, 0.45)
        b.rotation_euler = (0, 0, a)
        b.location = (math.cos(a) * 1.0, math.sin(a) * 1.0, 0.03)
        crust.append(paint(b, "Crust"))
    return join([lava] + crust, "lava_pool")


def make_wall():
    reset()
    rng = random.Random(10)
    parts = []
    x = -1.5
    while x < 1.5:
        r = rng.uniform(0.2, 0.27)
        h = rng.uniform(0.7, 0.95)
        c = cyl(r, h, z0=-0.15, verts=6)
        c.location = (x + r, rng.uniform(-0.12, 0.12), 0)
        c.rotation_euler = (0, 0, rng.uniform(0, 1))
        parts.append(paint(c, "Basalt"))
        cap = cyl(r * 0.92, 0.03, z0=h - 0.15, verts=6)
        cap.location = c.location; cap.rotation_euler = c.rotation_euler
        parts.append(paint(cap, "GlowCyan"))
        x += 2 * r * 0.9
    return join(parts, "basalt_wall")


def make_vent():
    reset()
    mound = smooth(cyl(0.95, 0.28, z0=-0.05, verts=32, r2=0.38))
    paint(mound, "VentRock")
    core = paint(cyl(0.36, 0.02, z0=0.2, verts=24), "GlowOrange")
    bpy.ops.mesh.primitive_torus_add(major_radius=0.38, minor_radius=0.05, major_segments=24, minor_segments=6,
                                     location=(0, 0, 0.23))
    rim = paint(active(), "Crust")
    return join([mound, core, rim], "vent")


def make_powerups():
    reset()   # heal: glowing plus
    a = cyl(0.07, 0.42, z0=-0.21, verts=12); a.rotation_euler = (math.pi / 2, 0, 0)
    bpy.ops.mesh.primitive_cube_add(size=1); b = active(); b.scale = (0.42, 0.14, 0.14)
    bpy.ops.mesh.primitive_cube_add(size=1); c = active(); c.scale = (0.14, 0.14, 0.42)
    export(join([paint(b, "GlowGreen"), paint(c, "GlowGreen")], "pu_heal"), "pu_heal")
    reset()   # shield: hexagon emblem with a ring
    h = cyl(0.24, 0.06, z0=-0.03, verts=6); h.rotation_euler = (math.pi / 2, 0, 0)
    bpy.ops.mesh.primitive_torus_add(major_radius=0.3, minor_radius=0.025, major_segments=32, minor_segments=6,
                                     rotation=(math.pi / 2, 0, 0))
    export(join([paint(h, "GlowBlue"), paint(active(), "ShipHull")], "pu_shield"), "pu_shield")
    reset()   # blaster: little ray gun
    bpy.ops.mesh.primitive_cube_add(size=1); body = active(); body.scale = (0.32, 0.12, 0.16)
    grip = cyl(0.05, 0.2, z0=-0.28, verts=8); grip.location.x = -0.08
    barrel = cyl(0.04, 0.3, z0=-0.15, verts=12); barrel.rotation_euler = (0, math.pi / 2, 0); barrel.location.x = 0.2
    bpy.ops.mesh.primitive_torus_add(major_radius=0.06, minor_radius=0.015, major_segments=16, minor_segments=6,
                                     rotation=(0, math.pi / 2, 0), location=(0.33, 0, 0))
    ring = active()
    export(join([paint(body, "ShipTrim"), paint(grip, "ShipDark"), paint(barrel, "ShipDark"),
                 paint(ring, "GlowOrange")], "pu_blaster"), "pu_blaster")


if __name__ == "__main__":
    os.makedirs(OUT, exist_ok=True)
    export(make_planet(), "planet")
    export(make_crystals("alien_crystal_pink", "GlowPink", 1), "alien_crystal_pink")
    export(make_crystals("alien_crystal_cyan", "GlowCyan", 2), "alien_crystal_cyan")
    export(make_shrooms(3), "alien_shrooms")
    export(make_tubes(4), "alien_tubes")
    for s in (5, 6, 7):
        export(make_tree(s), f"alien_tree_{s}")
    export(make_spikes(8), "alien_spikes")
    export(make_ship(), "ship")
    export(make_flame(), "ship_flame")
    make_parts()   # exports each part as it is built
    export(make_gas_giant(), "gas_giant")
    export(make_ring(), "gas_ring")
    export(make_cactus(11), "alien_cactus_11")
    export(make_cactus(12), "alien_cactus_12")
    export(make_spike_cluster(13), "spike_cluster")
    export(make_lava_pool(), "lava_pool")
    export(make_wall(), "basalt_wall")
    export(make_vent(), "vent")
    make_powerups()   # exports pu_heal, pu_shield, pu_blaster
    print("ALIEN DONE")
