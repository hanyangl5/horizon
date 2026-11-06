"""Restore cutout material metadata lost by the Bistro FBX2glTF export."""

import argparse
import json
import shutil
from pathlib import Path


MASK_MATERIALS = (
    "Foliage_Flowers.DoubleSided",
    "MASTER_Forge_Metal",
    "MASTER_Side_Letters",
    "Foliage_Linde_Tree_Large_Orange_Leaves.DoubleSided",
    "Foliage_Linde_Tree_Large_Green_Leaves.DoubleSided",
    "Foliage_Ivy_leaf_a.DoubleSided",
    "Foliage_Bux_Hedges46.DoubleSided",
    "Foliage_Leaves.DoubleSided",
)


REFERENCE_MASKS = {
    "Streetlight_Chains": "PropTextures/Street/Paris_StreetLight_01/Street_decals_mask.png",
    "Foliage_Paris_Flowers": "PropTextures/Natural/Paris_flowers_01a/Paris_flowers_01a_mask.png",
}


def set_mask(material):
    before = dict(material)
    material["alphaMode"] = "MASK"
    material["alphaCutoff"] = 0.5
    if material["name"].endswith(".DoubleSided"):
        material["doubleSided"] = True
    return material != before


def restore_opacity_maps(scene, materials, source, reference):
    from PIL import Image

    changed = []
    for name, mask in REFERENCE_MASKS.items():
        material = materials[name]
        texture = scene["textures"][material["pbrMetallicRoughness"]["baseColorTexture"]["index"]]
        image = scene["images"][texture["source"]]
        output = source.parent / (name + "_BaseColor.masked.png")
        with Image.open(source.parent / (name + "_BaseColor.dds")) as diffuse:
            color = diffuse.convert("RGBA")
        with Image.open(reference / "deps/src/bistro" / mask) as opacity:
            alpha = opacity.convert("L").resize(color.size, Image.Resampling.BILINEAR)
        color.putalpha(alpha)
        color.save(output)
        material_changed = set_mask(material)
        if image["uri"] != output.name or material_changed:
            changed.append(name)
        image["uri"] = output.name
        print(f"Restored {name} opacity from {reference / 'deps/src/bistro' / mask}")
    return changed


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("scene", type=Path, help="Path to BistroExterior.gltf")
    parser.add_argument("--reference", type=Path, help="Cookbook repository; restores two missing opacity maps (requires Pillow)")
    args = parser.parse_args()
    source = args.scene.resolve()
    scene = json.loads(source.read_text(encoding="utf-8"))
    materials = {material.get("name"): material for material in scene["materials"]}
    expected = set(MASK_MATERIALS)
    if args.reference:
        expected.update(REFERENCE_MASKS)
    missing = expected - materials.keys()
    if missing:
        parser.error("Expected the Bistro FBX2glTF materials; missing: " + ", ".join(sorted(missing)))

    changed = []
    for name in MASK_MATERIALS:
        if set_mask(materials[name]):
            changed.append(name)
    if args.reference:
        changed.extend(restore_opacity_maps(scene, materials, source, args.reference.resolve()))

    if not changed:
        print("Bistro cutout materials are already configured.")
        return

    backup = source.with_suffix(".gltf.before-mask-fix")
    if not backup.exists():
        shutil.copy2(source, backup)
    temporary = source.with_suffix(".gltf.material-fix.tmp")
    temporary.write_text(json.dumps(scene, indent=4) + "\n", encoding="utf-8")
    temporary.replace(source)
    print(f"Updated {len(changed)} cutout materials in {source}")
    print(f"Original metadata backup: {backup}")
    for name in changed:
        print("  " + name)


if __name__ == "__main__":
    main()
