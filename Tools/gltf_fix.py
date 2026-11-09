"""Post-process glTF files for Unreal import (plain Python, no Blender needed).

- alphaMode BLEND -> MASK (cutoff 0.5): Unreal would otherwise create translucent foliage materials.
- De-duplicate images/textures: Blender's exporter writes one image entry per material that uses a file,
  which makes Unreal import the same texture several times (T_x, T_x1, T_x12, ...).

Usage: python3 Tools/gltf_fix.py SourceAssets/prepared/*/*.gltf
"""
import json
import sys


def _remap_texture_refs(node, tex_map, key=""):
    if isinstance(node, dict):
        if key.endswith("Texture") and "index" in node:
            node["index"] = tex_map[node["index"]]
        for k, v in node.items():
            _remap_texture_refs(v, tex_map, k)
    elif isinstance(node, list):
        for v in node:
            _remap_texture_refs(v, tex_map, key)


def fix_gltf(path):
    with open(path) as f:
        g = json.load(f)

    for m in g.get("materials", []):
        if m.get("alphaMode") == "BLEND":
            m["alphaMode"] = "MASK"
            m["alphaCutoff"] = 0.5

    images, textures = g.get("images", []), g.get("textures", [])
    if images and textures:
        # unique images by uri (or bufferView for embedded images)
        new_images, img_key_to_new, img_map = [], {}, {}
        for i, img in enumerate(images):
            k = img.get("uri") or ("bv", img.get("bufferView"))
            if k not in img_key_to_new:
                img_key_to_new[k] = len(new_images)
                new_images.append(img)
            img_map[i] = img_key_to_new[k]
        # unique textures by (image, sampler)
        new_textures, tex_key_to_new, tex_map = [], {}, {}
        for i, t in enumerate(textures):
            t = dict(t)
            if "source" in t:
                t["source"] = img_map[t["source"]]
            k = (t.get("source"), t.get("sampler"))
            if k not in tex_key_to_new:
                tex_key_to_new[k] = len(new_textures)
                new_textures.append(t)
            tex_map[i] = tex_key_to_new[k]
        _remap_texture_refs(g.get("materials", []), tex_map)
        g["images"], g["textures"] = new_images, new_textures

    with open(path, "w") as f:
        json.dump(g, f, indent=1)
    return len(g.get("images", []))


if __name__ == "__main__":
    for p in sys.argv[1:]:
        print(f"{p}: {fix_gltf(p)} unique image(s)")
