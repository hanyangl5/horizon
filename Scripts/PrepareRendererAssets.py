"""Convert the Cookbook rotation and blue-noise images to local Renderer DDS files."""

import argparse
from pathlib import Path

from PIL import Image


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("reference", type=Path, help="Cookbook repository (requires Pillow)")
    parser.add_argument("--output", type=Path, default=Path(__file__).resolve().parent.parent / "Examples/Renderer/Assets")
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    for source, destination in (
        ("rot_texture.bmp", "rotations.dds"),
        ("blue_noise_128x128x64.png", "blue_noise_128x128x64.dds"),
    ):
        with Image.open(args.reference / "data" / source) as image:
            image.convert("RGBA").save(args.output / destination)
        print(args.output / destination)


if __name__ == "__main__":
    main()
