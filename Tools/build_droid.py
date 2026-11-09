"""Unreal editor Python: import Pip (the rolling droid) and set up its materials.

Run with the editor open:  python3 Tools/ue_remote.py Tools/build_droid.py
Inputs: SourceAssets/prepared/pip_body|pip_head (from Tools/make_droid.py).
Outputs: /Game/Characters/Pip/SM_PipBody, SM_PipHead, M_PipPaint (master) and MI_Pip_<part>.
The C++ ARollerDroid loads the two meshes by these paths and animates the glow parameters
("EmissiveStrength") on the MI_Pip_Glow* slots.
"""
import os

import unreal

PROJECT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
PREP = os.path.join(PROJECT, "SourceAssets", "prepared")
DEST = "/Game/Characters/Pip"

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary

# slot name (from Blender material) -> (base colour, metallic, roughness, emissive colour, emissive strength)
PAINT = {
    "M_Pip_Cream":    ((0.86, 0.82, 0.72), 0.0, 0.35, (0, 0, 0), 0.0),
    "M_Pip_Teal":     ((0.05, 0.42, 0.42), 0.0, 0.30, (0, 0, 0), 0.0),
    "M_Pip_Graphite": ((0.04, 0.045, 0.05), 0.6, 0.35, (0, 0, 0), 0.0),
    "M_Pip_Lens":     ((0.005, 0.005, 0.008), 0.0, 0.04, (0, 0, 0), 0.0),
    "M_Pip_GlowCyan": ((0.1, 0.9, 0.9), 0.0, 0.3, (0.2, 1.0, 0.95), 30.0),
    "M_Pip_GlowWarm": ((1.0, 0.75, 0.3), 0.0, 0.3, (1.0, 0.7, 0.25), 30.0),
    "M_Pip_Spark":    ((1, 1, 1), 0.0, 0.2, (1, 1, 1), 60.0),
}


def log(msg):
    unreal.log(f"[Pip] {msg}")


def lc(rgb):
    return unreal.LinearColor(r=rgb[0], g=rgb[1], b=rgb[2], a=1.0)


def import_mesh(name, folder, final_name):
    final = f"{folder}/{final_name}"
    if eal.does_asset_exist(final):
        return eal.load_asset(final)
    task = unreal.AssetImportTask()
    task.filename = os.path.join(PREP, name, f"{name}.gltf")
    task.destination_path = folder
    task.automated = True
    task.replace_existing = True
    task.save = False
    asset_tools.import_asset_tasks([task])
    meshes = [p for p in eal.list_assets(folder, include_folder=False)
              if isinstance(eal.load_asset(p), unreal.StaticMesh)]
    if not meshes:
        raise RuntimeError(f"no mesh imported from {name}")
    eal.rename_asset(meshes[0].split(".")[0], final)   # Interchange names assets after the source
    return eal.load_asset(final)


def master_material():
    path = f"{DEST}/M_PipPaint"
    if eal.does_asset_exist(path):
        return eal.load_asset(path)
    m = asset_tools.create_asset("M_PipPaint", DEST, unreal.Material, unreal.MaterialFactoryNew())
    expr = mel.create_material_expression

    def vec(name, default, y):
        e = expr(m, unreal.MaterialExpressionVectorParameter, -600, y)
        e.set_editor_property("parameter_name", name)
        e.set_editor_property("default_value", lc(default))
        return e

    def scal(name, default, y):
        e = expr(m, unreal.MaterialExpressionScalarParameter, -600, y)
        e.set_editor_property("parameter_name", name)
        e.set_editor_property("default_value", default)
        return e

    P = unreal.MaterialProperty
    mel.connect_material_property(vec("BaseColor", (0.8, 0.8, 0.8), -300), "", P.MP_BASE_COLOR)
    mel.connect_material_property(scal("Metallic", 0.0, -150), "", P.MP_METALLIC)
    mel.connect_material_property(scal("Roughness", 0.4, 0), "", P.MP_ROUGHNESS)
    mul = expr(m, unreal.MaterialExpressionMultiply, -300, 250)
    mel.connect_material_expressions(vec("EmissiveColor", (0, 0, 0), 200), "", mul, "A")
    mel.connect_material_expressions(scal("EmissiveStrength", 0.0, 350), "", mul, "B")
    mel.connect_material_property(mul, "", P.MP_EMISSIVE_COLOR)
    mel.recompile_material(m)
    return m


def paint_instance(slot, master):
    name = slot.replace("M_Pip_", "MI_Pip_")
    path = f"{DEST}/{name}"
    mi = eal.load_asset(path) if eal.does_asset_exist(path) else asset_tools.create_asset(
        name, DEST, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(mi, master)
    base, metal, rough, emis, strength = PAINT[slot]
    mel.set_material_instance_vector_parameter_value(mi, "BaseColor", lc(base))
    mel.set_material_instance_scalar_parameter_value(mi, "Metallic", metal)
    mel.set_material_instance_scalar_parameter_value(mi, "Roughness", rough)
    mel.set_material_instance_vector_parameter_value(mi, "EmissiveColor", lc(emis))
    mel.set_material_instance_scalar_parameter_value(mi, "EmissiveStrength", strength)
    return mi


def assign(mesh, master):
    for i, sm in enumerate(mesh.get_editor_property("static_materials")):
        slot = str(sm.get_editor_property("material_slot_name"))
        key = next((k for k in PAINT if slot.startswith(k)), None)
        if key is None:
            unreal.log_warning(f"[Pip] {mesh.get_name()}: unknown slot {slot}")
            continue
        mesh.set_material(i, paint_instance(key, master))
    ns = mesh.get_editor_property("nanite_settings")   # small, smooth meshes: no Nanite needed
    if ns.enabled:
        ns.enabled = False
        mesh.set_editor_property("nanite_settings", ns)


def main():
    master = master_material()
    body = import_mesh("pip_body", f"{DEST}/Import/Body", "SM_PipBody")
    head = import_mesh("pip_head", f"{DEST}/Import/Head", "SM_PipHead")
    # move the renamed meshes up next to the materials
    for mesh, name in ((body, "SM_PipBody"), (head, "SM_PipHead")):
        if not mesh.get_path_name().startswith(f"{DEST}/{name}."):
            eal.rename_asset(mesh.get_path_name().split(".")[0], f"{DEST}/{name}")
    body, head = eal.load_asset(f"{DEST}/SM_PipBody"), eal.load_asset(f"{DEST}/SM_PipHead")
    for mesh in (body, head):
        assign(mesh, master)
        b = mesh.get_bounds()
        log(f"{mesh.get_name()}: extent {b.box_extent}, origin {b.origin}")
    # Interchange's own per-slot materials are unused once the MI_Pip_* instances are assigned
    imp = f"{DEST}/Import"
    if eal.does_directory_exist(imp):
        outside = [r for a in eal.list_assets(imp, recursive=True, include_folder=False)
                   for r in eal.find_package_referencers_for_asset(a.split(".")[0]) if not r.startswith(imp)]
        if not outside:
            eal.delete_directory(imp)
    eal.save_directory(DEST, only_if_is_dirty=False, recursive=True)
    log("DROID ASSETS OK")


main()
