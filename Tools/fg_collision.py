"""Unreal editor Python: persist material usage flags (imported by Tools/build_alien.py).

Run on its own:  python3 Tools/ue_remote.py Tools/fg_collision.py
"""
import unreal

eal = unreal.EditorAssetLibrary
ENV = "/Game/Alien"


def fix_material_usage(root=ENV):
    """Persist the material usage flags Nanite/instanced meshes need.

    The editor sets bUsedWithNanite / bUsedWithInstancedStaticMeshes on the fly when it first renders a
    mesh but does not save them, so a standalone game (-game, packaged) falls back to the default
    material: grey rocks and invisible foliage. Setting and saving them here fixes that.
    """
    fixed = []
    for p in eal.list_assets(root, recursive=True, include_folder=False):
        a = eal.load_asset(p)
        if not isinstance(a, unreal.Material):
            continue
        changed = False
        for flag in ("used_with_nanite", "used_with_instanced_static_meshes"):
            try:
                if not a.get_editor_property(flag):
                    a.set_editor_property(flag, True)
                    changed = True
            except Exception:
                pass
        if changed:
            unreal.MaterialEditingLibrary.recompile_material(a)
            eal.save_loaded_asset(a)
            fixed.append(a.get_name())
    unreal.log(f"[DroidGalaxy] material usage flags set on {len(fixed)} material(s)")
    return fixed


if __name__ == "__main__":
    print(fix_material_usage("/Game/Alien"), fix_material_usage("/Game/Characters"))
