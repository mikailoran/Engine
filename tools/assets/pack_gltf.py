#!/usr/bin/env python3
"""Packs a glTF asset into one compact .glb for the engine.

Reads a .gltf (external .bin and image files, or data URIs) or a .glb and
writes a .glb whose single buffer holds only what the output references.
Nodes, skins and animations are copied unchanged, so a rig keeps its exact
rest pose (Blender round trips would not guarantee that).

Usage:
    tools/assets/pack_gltf.py IN.gltf OUT.glb [--max-texture 1024]
    tools/assets/pack_gltf.py IN.glb OUT.glb --keep-animations Idle_Loop Walk_Loop

--max-texture downscales larger images (Pillow) and re-encodes them as PNG.
--keep-animations drops every other clip; naming one that is missing fails.
Image files that don't exist are dropped with a warning, along with the
textures and material slots using them.
"""

from __future__ import annotations

import argparse
import base64
import io
import json
import struct
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Any

from PIL import Image

GLB_MAGIC = 0x46546C67
CHUNK_JSON = 0x4E4F534A
CHUNK_BIN = 0x004E4942


@dataclass
class Document:
    """A parsed glTF: its JSON and the bytes of each of its buffers."""

    gltf: dict[str, Any]
    buffers: list[bytes]
    base_dir: Path


@dataclass
class Options:
    """What to change while packing."""

    max_texture: int | None
    keep_animations: list[str] | None


def read_uri(uri: str, base_dir: Path) -> bytes:
    """Returns the bytes a glTF URI names: a data URI or a relative file."""
    if uri.startswith("data:"):
        return base64.b64decode(uri.split(",", 1)[1])
    return (base_dir / uri).read_bytes()


def load(path: Path) -> Document:
    """Reads a .gltf or .glb file and every buffer it uses."""
    data = path.read_bytes()
    if path.suffix.lower() == ".glb":
        magic, _version, _length = struct.unpack_from("<III", data, 0)
        if magic != GLB_MAGIC:
            raise ValueError(f"{path}: not a GLB file")
        offset = 12
        gltf: dict[str, Any] = {}
        bin_chunk = b""
        while offset < len(data):
            length, kind = struct.unpack_from("<II", data, offset)
            chunk = data[offset + 8 : offset + 8 + length]
            if kind == CHUNK_JSON:
                gltf = json.loads(chunk)
            elif kind == CHUNK_BIN:
                bin_chunk = chunk
            offset += 8 + length
        buffers = [
            bin_chunk if "uri" not in buffer else read_uri(buffer["uri"], path.parent)
            for buffer in gltf.get("buffers", [])
        ]
    else:
        gltf = json.loads(data)
        buffers = [read_uri(b["uri"], path.parent) for b in gltf.get("buffers", [])]
    return Document(gltf=gltf, buffers=buffers, base_dir=path.parent)


def view_bytes(doc: Document, view_index: int) -> bytes:
    """Returns the bytes of one buffer view."""
    view = doc.gltf["bufferViews"][view_index]
    start = view.get("byteOffset", 0)
    return doc.buffers[view["buffer"]][start : start + view["byteLength"]]


def keep_animations(gltf: dict[str, Any], names: list[str]) -> None:
    """Drops every animation not named in @p names, in place."""
    animations = gltf.get("animations", [])
    available = {a.get("name") for a in animations}
    missing = [n for n in names if n not in available]
    if missing:
        raise ValueError(f"animations not found: {', '.join(missing)}")
    gltf["animations"] = [a for a in animations if a.get("name") in names]
    if not gltf["animations"]:
        del gltf["animations"]


def drop_missing_images(doc: Document) -> None:
    """Drops images whose file is missing, with their textures and uses.

    Material slots (any `*Texture` object) naming a dropped texture are
    removed, so the material falls back to its factors.
    """
    gltf = doc.gltf
    images = gltf.get("images", [])
    missing = {
        i
        for i, image in enumerate(images)
        if "uri" in image
        and not image["uri"].startswith("data:")
        and not (doc.base_dir / image["uri"]).is_file()
    }
    if not missing:
        return
    for i in sorted(missing):
        print(f"warning: dropping missing image {images[i]['uri']}", file=sys.stderr)

    image_map = {}
    for old in range(len(images)):
        if old not in missing:
            image_map[old] = len(image_map)
    gltf["images"] = [im for i, im in enumerate(images) if i not in missing]

    textures = gltf.get("textures", [])
    texture_map = {}
    kept_textures = []
    for old, texture in enumerate(textures):
        if texture.get("source") in missing:
            continue
        if "source" in texture:
            texture["source"] = image_map[texture["source"]]
        texture_map[old] = len(kept_textures)
        kept_textures.append(texture)
    gltf["textures"] = kept_textures

    def fix_slots(node: dict[str, Any]) -> None:
        for key in list(node):
            value = node[key]
            if key.endswith("Texture") and isinstance(value, dict):
                if value["index"] in texture_map:
                    value["index"] = texture_map[value["index"]]
                else:
                    del node[key]
            elif isinstance(value, dict):
                fix_slots(value)

    for material in gltf.get("materials", []):
        fix_slots(material)


def used_accessors(gltf: dict[str, Any]) -> set[int]:
    """Indices of the accessors meshes, skins and animations reference."""
    used: set[int] = set()
    for mesh in gltf.get("meshes", []):
        for primitive in mesh["primitives"]:
            used.update(primitive["attributes"].values())
            if "indices" in primitive:
                used.add(primitive["indices"])
            for target in primitive.get("targets", []):
                used.update(target.values())
    for skin in gltf.get("skins", []):
        if "inverseBindMatrices" in skin:
            used.add(skin["inverseBindMatrices"])
    for animation in gltf.get("animations", []):
        for sampler in animation["samplers"]:
            used.add(sampler["input"])
            used.add(sampler["output"])
    return used


def encode_image(data: bytes, max_size: int | None) -> tuple[bytes, str]:
    """Returns @p data as PNG, downscaled so neither side exceeds max_size."""
    image = Image.open(io.BytesIO(data))
    if max_size is not None and max(image.size) > max_size:
        scale = max_size / max(image.size)
        size = (max(1, round(image.width * scale)), max(1, round(image.height * scale)))
        image = image.resize(size, Image.Resampling.LANCZOS)
    out = io.BytesIO()
    image.save(out, format="PNG", optimize=True)
    return out.getvalue(), "image/png"


class BinWriter:
    """Builds the output's single binary buffer and its buffer views."""

    def __init__(self) -> None:
        self.data = bytearray()
        self.views: list[dict[str, Any]] = []

    def add(self, payload: bytes, template: dict[str, Any] | None = None) -> int:
        """Appends @p payload 4-byte aligned; returns its new view index."""
        self.data.extend(b"\0" * (-len(self.data) % 4))
        view: dict[str, Any] = {
            "buffer": 0,
            "byteOffset": len(self.data),
            "byteLength": len(payload),
        }
        for key in ("byteStride", "target"):
            if template is not None and key in template:
                view[key] = template[key]
        self.data.extend(payload)
        self.views.append(view)
        return len(self.views) - 1


def pack(doc: Document, options: Options) -> bytes:
    """Returns the GLB bytes for @p doc with @p options applied."""
    gltf = doc.gltf
    drop_missing_images(doc)
    if options.keep_animations is not None:
        keep_animations(gltf, options.keep_animations)

    writer = BinWriter()
    view_map: dict[int, int] = {}

    def remap_view(old: int) -> int:
        if old not in view_map:
            view_map[old] = writer.add(view_bytes(doc, old), gltf["bufferViews"][old])
        return view_map[old]

    # Kept accessors, renumbered in their original order
    old_accessors = gltf.get("accessors", [])
    used = sorted(used_accessors(gltf))
    accessor_map = {old: new for new, old in enumerate(used)}
    accessors = []
    for old in used:
        accessor = dict(old_accessors[old])
        if "sparse" in accessor:
            raise ValueError("sparse accessors are unsupported")
        if "bufferView" in accessor:
            accessor["bufferView"] = remap_view(accessor["bufferView"])
        accessors.append(accessor)

    # Images are embedded, optionally downscaled
    for image in gltf.get("images", []):
        if "uri" in image:
            raw = read_uri(image.pop("uri"), doc.base_dir)
        else:
            raw = view_bytes(doc, image["bufferView"])
        payload, mime = encode_image(raw, options.max_texture)
        image["bufferView"] = writer.add(payload)
        image["mimeType"] = mime

    for mesh in gltf.get("meshes", []):
        for primitive in mesh["primitives"]:
            attributes = primitive["attributes"]
            for key, value in attributes.items():
                attributes[key] = accessor_map[value]
            if "indices" in primitive:
                primitive["indices"] = accessor_map[primitive["indices"]]
            for target in primitive.get("targets", []):
                for key, value in target.items():
                    target[key] = accessor_map[value]
    for skin in gltf.get("skins", []):
        if "inverseBindMatrices" in skin:
            skin["inverseBindMatrices"] = accessor_map[skin["inverseBindMatrices"]]
    for animation in gltf.get("animations", []):
        for sampler in animation["samplers"]:
            sampler["input"] = accessor_map[sampler["input"]]
            sampler["output"] = accessor_map[sampler["output"]]

    gltf["accessors"] = accessors
    gltf["bufferViews"] = writer.views
    writer.data.extend(b"\0" * (-len(writer.data) % 4))
    gltf["buffers"] = [{"byteLength": len(writer.data)}]

    json_bytes = json.dumps(gltf, separators=(",", ":")).encode()
    json_bytes += b" " * (-len(json_bytes) % 4)
    total = 12 + 8 + len(json_bytes) + 8 + len(writer.data)
    return b"".join(
        [
            struct.pack("<III", GLB_MAGIC, 2, total),
            struct.pack("<II", len(json_bytes), CHUNK_JSON),
            json_bytes,
            struct.pack("<II", len(writer.data), CHUNK_BIN),
            bytes(writer.data),
        ]
    )


def main(argv: list[str]) -> int:
    """Parses arguments, packs the input and writes the output."""
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("input", type=Path, help=".gltf or .glb to read")
    parser.add_argument("output", type=Path, help=".glb to write")
    parser.add_argument("--max-texture", type=int, help="largest image side, px")
    parser.add_argument(
        "--keep-animations", nargs="+", metavar="NAME", help="clips to keep"
    )
    args = parser.parse_args(argv)

    doc = load(args.input)
    data = pack(doc, Options(args.max_texture, args.keep_animations))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(data)
    print(f"{args.output}: {len(data) / 1e6:.2f} MB")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
