"""Exports a .blend file's meshes as one .glb for the engine (Blender script).

Run headless, with options after `--`:
    blender -b IN.blend --python tools/assets/export_glb.py -- OUT.glb \
        [--length 0.2] [--x-forward]

--length scales the meshes uniformly so their X extent is that many metres,
for packs modelled at arbitrary scale. --x-forward turns models whose front
points along Blender +X so it points along the engine's forward (glTF -Z).
The transform is baked into the mesh, so the exported node is identity and
the object's origin stays the mesh origin (a gun's grip, for instance).

Exports +Y up with modifiers applied, and no animations, cameras or lights.
Untextured Principled BSDF materials are rebuilt so their colours export.
Prints the bounds in glTF space and the centre of the front face, which for
a gun is its muzzle.
"""

from __future__ import annotations

import argparse
import math
import sys
from dataclasses import dataclass

import bpy
from mathutils import Matrix, Vector


@dataclass
class Options:
    """Command-line options given after Blender's `--`."""

    output: str
    length: float | None
    x_forward: bool


def parse_options() -> Options:
    """Parses the arguments after `--` on Blender's command line."""
    argv = sys.argv[sys.argv.index("--") + 1 :] if "--" in sys.argv else []
    parser = argparse.ArgumentParser(prog="export_glb.py")
    parser.add_argument("output", help=".glb to write")
    parser.add_argument("--length", type=float, help="X extent in m")
    parser.add_argument("--x-forward", action="store_true", help="+X → -Z")
    args = parser.parse_args(argv)
    return Options(args.output, args.length, args.x_forward)


def mesh_objects() -> list[bpy.types.Object]:
    """Every mesh object in the file."""
    return [o for o in bpy.data.objects if o.type == "MESH"]


def world_bounds(objects: list[bpy.types.Object]) -> tuple[Vector, Vector]:
    """Box around every vertex of @p objects, in Blender world space."""
    points = [o.matrix_world @ v.co for o in objects for v in o.data.vertices]
    low = Vector(tuple(min(p[i] for p in points) for i in range(3)))
    high = Vector(tuple(max(p[i] for p in points) for i in range(3)))
    return low, high


def bake(objects: list[bpy.types.Object], options: Options) -> None:
    """Scales and turns @p objects as asked, baked into their mesh data."""
    low, high = world_bounds(objects)
    scale = 1.0 if options.length is None else options.length / (high.x - low.x)
    # Blender +X turned to +Y, which exports as glTF -Z
    turn = Matrix.Rotation(math.pi / 2, 4, "Z") if options.x_forward else Matrix()
    transform = turn @ Matrix.Scale(scale, 4)
    for obj in objects:
        obj.data.transform(transform @ obj.matrix_world)
        obj.matrix_world = Matrix()


def rebuild_factor_materials() -> None:
    """Rebuilds each untextured Principled BSDF material as a bare
    Principled BSDF → Material Output pair.

    Some packs (Quaternius' guns) add a second output fed by a Diffuse BSDF,
    and the glTF exporter then writes no colours at all. Copied: base colour,
    metallic and roughness; materials with any linked input are left alone.
    """
    for material in bpy.data.materials:
        if material.node_tree is None:
            continue
        nodes = material.node_tree.nodes
        source = next((n for n in nodes if n.type == "BSDF_PRINCIPLED"), None)
        if source is None or any(i.links for i in source.inputs):
            continue
        base_color = tuple(source.inputs["Base Color"].default_value)
        metallic = source.inputs["Metallic"].default_value
        roughness = source.inputs["Roughness"].default_value

        nodes.clear()
        bsdf = nodes.new("ShaderNodeBsdfPrincipled")
        output = nodes.new("ShaderNodeOutputMaterial")
        bsdf.inputs["Base Color"].default_value = base_color
        bsdf.inputs["Metallic"].default_value = metallic
        bsdf.inputs["Roughness"].default_value = roughness
        material.node_tree.links.new(bsdf.outputs["BSDF"], output.inputs["Surface"])


def report(objects: list[bpy.types.Object]) -> None:
    """Prints bounds and the front-face centre in glTF axes (+Y up, -Z front)."""
    low, high = world_bounds(objects)

    # glTF (x, y, z) is Blender (x, z, -y)
    def gltf(v: Vector) -> tuple[float, float, float]:
        return (round(v.x, 4), round(v.z, 4), round(-v.y, 4))

    print(f"bounds min {gltf(Vector((low.x, high.y, low.z)))}")
    print(f"bounds max {gltf(Vector((high.x, low.y, high.z)))}")
    # Front face: vertices within 1 mm of the largest Blender Y (glTF -Z)
    points = [o.matrix_world @ v.co for o in objects for v in o.data.vertices]
    front = [p for p in points if p.y > high.y - 1e-3]
    centre = sum(front, Vector()) / len(front)
    print(f"front centre {gltf(centre)}")


def main() -> None:
    """Bakes the requested transform and writes the .glb."""
    options = parse_options()
    objects = mesh_objects()
    if not objects:
        raise SystemExit("no mesh objects in the file")
    bake(objects, options)
    rebuild_factor_materials()
    report(objects)
    bpy.ops.export_scene.gltf(
        filepath=options.output,
        export_format="GLB",
        export_yup=True,
        export_apply=True,
        export_animations=False,
        export_cameras=False,
        export_lights=False,
    )


main()
