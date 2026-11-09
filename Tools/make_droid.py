"""Blender (headless): procedurally model "Pip", the rolling droid, and export body + head as glTF.

Run:  blender -b --factory-startup --python Tools/make_droid.py -- <project_dir>

Original design: cream ball with a teal tennis-ball seam stripe, dark vents and glow dots;
squat dome head with a teal band, one big camera-lens eye (cyan iris + catch-light), a small
secondary lens and two antennas with glowing tips. Units: metres (Unreal import converts to cm).
Pivots: body at the ball centre, head at the centre of its flat base. Forward is +X.
"""
import math
import os
import sys

import bpy
from mathutils import Euler, Vector

PROJECT = os.path.abspath(sys.argv[sys.argv.index("--") + 1])
OUT = os.path.join(PROJECT, "SourceAssets", "prepared")

BODY_R = 0.28
HEAD_R = 0.17


_mats = {}


def reset():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    _mats.clear()   # materials die with the scene


def mat(name, color, metallic=0.0, rough=0.4, emission=None, strength=0.0):
    """Material named M_Pip_<name>; slot names drive the Unreal-side material assignment."""
    key = f"M_Pip_{name}"
    if key in _mats:
        return _mats[key]
    m = bpy.data.materials.new(key)
    m.use_nodes = True
    b = m.node_tree.nodes["Principled BSDF"]
    b.inputs["Base Color"].default_value = (*color, 1)
    b.inputs["Metallic"].default_value = metallic
    b.inputs["Roughness"].default_value = rough
    if emission:
        b.inputs["Emission Color"].default_value = (*emission, 1)
        b.inputs["Emission Strength"].default_value = strength
    _mats[key] = m
    return m


def paint(obj, m):
    obj.data.materials.clear()
    obj.data.materials.append(m)
    return obj


def smooth(obj):
    for p in obj.data.polygons:
        p.use_smooth = True
    return obj


def active():
    return bpy.context.view_layer.objects.active


def orient_to(obj, normal, roll=0.0):
    """Rotate obj so its local +Z points along `normal`."""
    q = Vector((0, 0, 1)).rotation_difference(Vector(normal).normalized())
    obj.rotation_mode = "QUATERNION"
    obj.rotation_quaternion = q @ Euler((0, 0, roll)).to_quaternion()


def join(objs, name):
    bpy.ops.object.select_all(action="DESELECT")
    for o in objs:
        o.select_set(True)
    bpy.context.view_layer.objects.active = objs[0]
    bpy.ops.object.transform_apply(location=False, rotation=True, scale=True)
    bpy.ops.object.join()
    o = active()
    o.name = name
    # bake remaining location into vertices so the pivot is the world origin
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    return o


def export(obj, name):
    d = os.path.join(OUT, name)
    os.makedirs(d, exist_ok=True)
    bpy.ops.object.select_all(action="DESELECT")
    obj.select_set(True)
    path = os.path.join(d, f"{name}.gltf")
    bpy.ops.export_scene.gltf(filepath=path, export_format="GLTF_SEPARATE", use_selection=True,
                              export_apply=True, export_yup=True)
    print(f"DROID {name}: {len(obj.data.polygons)} polys, materials={[m.name for m in obj.data.materials]}")


# ----------------------------------------------------------------------------- body

def seam_points(n=400, a=0.62):
    """Tennis-ball seam on the unit sphere (a + b = 1)."""
    b = 1 - a
    pts = []
    for i in range(n):
        t = 2 * math.pi * i / n
        p = Vector((a * math.cos(t) + b * math.cos(3 * t),
                    a * math.sin(t) - b * math.sin(3 * t),
                    2 * math.sqrt(a * b) * math.sin(2 * t)))
        pts.append(p.normalized())
    return pts


def curve_tube(points, radius, name, bevel):
    cu = bpy.data.curves.new(name, "CURVE")
    cu.dimensions = "3D"
    cu.bevel_depth = bevel
    cu.bevel_resolution = 3
    sp = cu.splines.new("POLY")
    sp.points.add(len(points) - 1)
    for p, q in zip(sp.points, points):
        p.co = (*(q * radius), 1)
    sp.use_cyclic_u = True
    ob = bpy.data.objects.new(name, cu)
    bpy.context.scene.collection.objects.link(ob)
    bpy.context.view_layer.objects.active = ob
    ob.select_set(True)
    bpy.ops.object.convert(target="MESH")
    return active()


def make_body():
    reset()
    cream = mat("Cream", (0.86, 0.82, 0.72), rough=0.35)
    teal = mat("Teal", (0.05, 0.42, 0.42), rough=0.3)
    dark = mat("Graphite", (0.04, 0.045, 0.05), metallic=0.6, rough=0.35)
    glow = mat("GlowCyan", (0.1, 0.9, 0.9), emission=(0.2, 1.0, 0.95), strength=8.0)

    parts = []
    bpy.ops.mesh.primitive_uv_sphere_add(segments=96, ring_count=48, radius=BODY_R)
    parts.append(paint(smooth(active()), cream))

    # teal seam stripe, two parallel thin tubes with a wider soft one between them
    seam = seam_points()
    parts.append(paint(smooth(curve_tube(seam, BODY_R * 1.004, "Seam", 0.014)), teal))

    # dark vents with a glowing dot, placed in the two lobes the seam leaves open
    for d in ((0, 0, 1), (0, 0, -1), (1, 0, 0.0), (-1, 0, 0.0)):
        n = Vector(d).normalized()
        bpy.ops.mesh.primitive_cylinder_add(vertices=48, radius=0.055, depth=0.02)
        v = active()
        orient_to(v, n)
        v.location = n * (BODY_R - 0.004)
        parts.append(paint(smooth(v), dark))
        for k in range(3):   # three vent slats
            bpy.ops.mesh.primitive_cube_add(size=1)
            s = active()
            s.scale = (0.07, 0.008, 0.006)
            orient_to(s, n)
            s.location = n * (BODY_R + 0.006) + (s.rotation_quaternion @ Vector((0, (k - 1) * 0.02, 0)))
            parts.append(paint(s, teal))
        bpy.ops.mesh.primitive_uv_sphere_add(segments=16, ring_count=8, radius=0.009)
        g = active()
        offset = n.cross(Vector((0.3, 0.7, 0.2))).normalized() * 0.075   # beside the vent
        g.location = (n * BODY_R + offset).normalized() * (BODY_R + 0.002)
        parts.append(paint(smooth(g), glow))

    return join(parts, "SM_PipBody")


# ----------------------------------------------------------------------------- head

def make_head():
    reset()
    cream = mat("Cream", (0.86, 0.82, 0.72), rough=0.35)
    teal = mat("Teal", (0.05, 0.42, 0.42), rough=0.3)
    dark = mat("Graphite", (0.04, 0.045, 0.05), metallic=0.6, rough=0.35)
    lens = mat("Lens", (0.005, 0.005, 0.008), metallic=0.0, rough=0.05)
    iris = mat("GlowCyan", (0.1, 0.9, 0.9), emission=(0.2, 1.0, 0.95), strength=8.0)
    spark = mat("Spark", (1, 1, 1), emission=(1, 1, 1), strength=12.0)
    bulb = mat("GlowWarm", (1.0, 0.75, 0.3), emission=(1.0, 0.7, 0.25), strength=10.0)

    parts = []
    squash = 0.82
    # dome: upper half of a sphere, squashed for a cute squat look
    bpy.ops.mesh.primitive_uv_sphere_add(segments=96, ring_count=48, radius=HEAD_R)
    dome = active()
    bpy.ops.object.mode_set(mode="EDIT")
    bpy.ops.mesh.select_all(action="DESELECT")
    bpy.ops.object.mode_set(mode="OBJECT")
    for v in dome.data.vertices:
        v.select = v.co.z < -1e-4
    bpy.ops.object.mode_set(mode="EDIT")
    bpy.ops.mesh.delete(type="VERT")
    bpy.ops.object.mode_set(mode="OBJECT")
    dome.scale = (1, 1, squash)
    dome.location.z = 0.03
    parts.append(paint(smooth(dome), cream))

    # graphite base ring + teal band
    bpy.ops.mesh.primitive_cylinder_add(vertices=96, radius=HEAD_R * 0.97, depth=0.03, location=(0, 0, 0.015))
    parts.append(paint(smooth(active()), dark))
    bpy.ops.mesh.primitive_torus_add(major_radius=HEAD_R * 0.985, minor_radius=0.011, major_segments=96,
                                     minor_segments=12, location=(0, 0, 0.055))
    t = active()
    t.scale = (1, 1, 1.4)
    parts.append(paint(smooth(t), teal))

    def on_dome(elev_deg, azim_deg, lift=0.0):
        e, a = math.radians(elev_deg), math.radians(azim_deg)
        n = Vector((math.cos(e) * math.cos(a), math.cos(e) * math.sin(a), math.sin(e) / squash)).normalized()
        p = Vector((HEAD_R * math.cos(e) * math.cos(a), HEAD_R * math.cos(e) * math.sin(a),
                    0.03 + HEAD_R * squash * math.sin(e)))
        return p + n * lift, n

    # big main eye: graphite housing, glossy black lens, glowing iris ring, catch-light sparkle
    p, n = on_dome(28, 0)
    bpy.ops.mesh.primitive_cylinder_add(vertices=64, radius=0.068, depth=0.05)
    h = active(); orient_to(h, n); h.location = p
    parts.append(paint(smooth(h), dark))
    bpy.ops.mesh.primitive_uv_sphere_add(segments=48, ring_count=24, radius=0.056)
    l = active(); l.scale = (1, 1, 0.55); orient_to(l, n); l.location = p + n * 0.026
    parts.append(paint(smooth(l), lens))
    bpy.ops.mesh.primitive_torus_add(major_radius=0.034, minor_radius=0.0045, major_segments=48, minor_segments=8)
    r = active(); orient_to(r, n); r.location = p + n * 0.052
    parts.append(paint(smooth(r), iris))
    bpy.ops.mesh.primitive_uv_sphere_add(segments=16, ring_count=8, radius=0.011)
    s = active()
    side = n.cross(Vector((0, 0, 1))).normalized()
    up = side.cross(n).normalized()
    s.location = p + n * 0.055 + up * 0.022 - side * 0.016
    parts.append(paint(smooth(s), spark))

    # small secondary lens, lower right
    p2, n2 = on_dome(8, -32)
    bpy.ops.mesh.primitive_cylinder_add(vertices=40, radius=0.026, depth=0.03)
    h2 = active(); orient_to(h2, n2); h2.location = p2
    parts.append(paint(smooth(h2), dark))
    bpy.ops.mesh.primitive_uv_sphere_add(segments=32, ring_count=16, radius=0.02)
    l2 = active(); l2.scale = (1, 1, 0.5); orient_to(l2, n2); l2.location = p2 + n2 * 0.016
    parts.append(paint(smooth(l2), lens))
    bpy.ops.mesh.primitive_torus_add(major_radius=0.012, minor_radius=0.0025, major_segments=32, minor_segments=6)
    r2 = active(); orient_to(r2, n2); r2.location = p2 + n2 * 0.026
    parts.append(paint(smooth(r2), bulb))

    # two antennas at the back, slightly splayed, with glowing tips
    for elev, azim, length, tilt in ((62, 150, 0.17, 0.25), (55, 205, 0.11, -0.2)):
        base, nb = on_dome(elev, azim)
        direction = (nb + Vector((0, 0, 1.6))).normalized()
        direction = (Euler((0, tilt, 0)).to_matrix() @ direction).normalized()
        bpy.ops.mesh.primitive_cylinder_add(vertices=16, radius=0.0055, depth=length)
        a = active(); orient_to(a, direction); a.location = base + direction * (length / 2)
        parts.append(paint(smooth(a), dark))
        bpy.ops.mesh.primitive_cylinder_add(vertices=24, radius=0.014, depth=0.012)
        c = active(); orient_to(c, nb); c.location = base
        parts.append(paint(smooth(c), teal))
        bpy.ops.mesh.primitive_uv_sphere_add(segments=20, ring_count=10, radius=0.016)
        tip = active(); tip.location = base + direction * length
        parts.append(paint(smooth(tip), bulb))

    return join(parts, "SM_PipHead")


if __name__ == "__main__":
    export(make_body(), "pip_body")
    export(make_head(), "pip_head")
    print("DROID DONE")
