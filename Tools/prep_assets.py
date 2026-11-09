"""Blender (headless) prep of the Poly Haven rocks used (recoloured) on the alien planet.

Run:  blender -b --factory-startup --python Tools/prep_assets.py -- <project_dir>

- Splits the rock sets into one mesh per rock (each pivoted at its own bottom centre).
- Re-pivots the boulder at its bottom centre.
- Exports glTF (separate .bin + textures) into SourceAssets/prepared/<name>/<name>.gltf and runs
  Tools/gltf_fix.py (alphaMode BLEND -> MASK, texture de-dup).
"""
import bpy, bmesh, math, os, sys
from mathutils import Matrix, Vector

PROJECT = os.path.abspath(sys.argv[sys.argv.index("--") + 1])
SRC = os.path.join(PROJECT, "SourceAssets", "polyhaven")
OUT = os.path.join(PROJECT, "SourceAssets", "prepared")
sys.path.insert(0, os.path.join(PROJECT, "Tools"))
import gltf_fix  # noqa: E402  (BLEND->MASK + texture de-dup)

JOIN = ["boulder_01"]
SPLIT = ["rock_moss_set_01", "rock_moss_set_02"]


def reset():
    bpy.ops.wm.read_factory_settings(use_empty=True)


def import_gltf(name):
    bpy.ops.import_scene.gltf(filepath=os.path.join(SRC, name, f"{name}.gltf"))
    return [o for o in bpy.context.scene.objects if o.type == "MESH"]


def apply_transforms(objs):
    bpy.ops.object.select_all(action="DESELECT")
    for o in objs:
        o.select_set(True)
    bpy.context.view_layer.objects.active = objs[0]
    # clear parents keeping world transform, then bake transforms into vertices
    bpy.ops.object.parent_clear(type="CLEAR_KEEP_TRANSFORM")
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)


def pivot_bottom_center(obj):
    """Move geometry so its bounding-box bottom centre sits at the object origin, origin at world 0."""
    xs = [v.co.x for v in obj.data.vertices]
    ys = [v.co.y for v in obj.data.vertices]
    zs = [v.co.z for v in obj.data.vertices]
    offset = Vector(((min(xs) + max(xs)) / 2, (min(ys) + max(ys)) / 2, min(zs)))
    obj.data.transform(Matrix.Translation(-offset))
    obj.location = (0, 0, 0)


def export(objs, name):
    d = os.path.join(OUT, name)
    os.makedirs(d, exist_ok=True)
    bpy.ops.object.select_all(action="DESELECT")
    for o in objs:
        o.select_set(True)
    path = os.path.join(d, f"{name}.gltf")
    bpy.ops.export_scene.gltf(filepath=path, export_format="GLTF_SEPARATE", use_selection=True,
                              export_texture_dir="textures", export_image_format="AUTO",
                              export_apply=True, export_yup=True)
    gltf_fix.fix_gltf(path)
    tris = sum(len(o.data.polygons) for o in objs)
    print(f"PREP {name}: {len(objs)} object(s), {tris} polys -> {path}")


def strip_non_mesh():
    for o in list(bpy.context.scene.objects):
        if o.type != "MESH":
            # keep children's world transforms when deleting empties
            for c in o.children:
                mw = c.matrix_world.copy()
                c.parent = None
                c.matrix_world = mw
            bpy.data.objects.remove(o, do_unlink=True)


def prep_join(name):
    reset()
    import_gltf(name)
    strip_non_mesh()
    objs = [o for o in bpy.context.scene.objects if o.type == "MESH"]
    apply_transforms(objs)
    bpy.ops.object.select_all(action="DESELECT")
    for o in objs:
        o.select_set(True)
    bpy.context.view_layer.objects.active = objs[0]
    if len(objs) > 1:
        bpy.ops.object.join()
    obj = bpy.context.view_layer.objects.active
    obj.name = name
    pivot_bottom_center(obj)
    export([obj], name)


def prep_split(name):
    reset()
    import_gltf(name)
    strip_non_mesh()
    objs = [o for o in bpy.context.scene.objects if o.type == "MESH"]
    apply_transforms(objs)
    for i, obj in enumerate(sorted(objs, key=lambda o: o.name), start=1):
        part = f"{name}_{i:02d}"
        obj.name = part
        pivot_bottom_center(obj)
        export([obj], part)


if __name__ == "__main__":
    for n in JOIN:
        prep_join(n)
    for n in SPLIT:
        prep_split(n)
    print("PREP DONE")
