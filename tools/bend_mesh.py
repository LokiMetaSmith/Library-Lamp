# Blender Mesh Bending Script
#
# This script is designed to be run with Blender from the command line to
# programmatically apply a bend deformation to an STL mesh.
#
# Author: Jules
#
# --- How to Run ---
# 1. Make sure you have Blender installed and accessible from your command line.
# 2. Run the script using the following command format:
#
#    blender --background --python bend_mesh.py -- --input <input.stl> --output <output.stl> --angle <degrees> [--axis <X|Y|Z>]
#
# --- Command-Line Arguments ---
#   --input:    Required. Path to the input STL file.
#   --output:   Required. Path to save the output (bent) STL file.
#   --angle:    Required. The angle of the bend in degrees (e.g., 90).
#   --axis:     Optional. The axis to bend along. Defaults to 'X'.
#
# --- Example ---
#   blender --background --python bend_mesh.py -- --input flat_litho.stl --output bent_litho.stl --angle 90 --axis Y
#

import bpy
import sys
import argparse

def bend_mesh(input_path, output_path, angle_degrees, bend_axis):
    """
    Loads an STL mesh, applies a simple bend modifier, and exports the result.
    """
    # --- 1. Scene Setup ---
    # Clear the existing scene (deletes the default cube, camera, and light)
    bpy.ops.object.select_all(action='SELECT')
    bpy.ops.object.delete(use_global=False)

    # --- 2. Import the STL ---
    try:
        bpy.ops.import_mesh.stl(filepath=input_path)
    except Exception as e:
        print(f"Error: Failed to import STL file at '{input_path}'")
        print(e)
        sys.exit(1)

    # Get the imported object (it should be the only one in the scene)
    obj = bpy.context.selected_objects[0]
    bpy.context.view_layer.objects.active = obj

    print(f"Successfully imported '{input_path}'")

    # --- 3. Add and Configure the Bend Modifier ---
    print(f"Applying a {angle_degrees}-degree bend along the {bend_axis}-axis...")

    # Add the "Simple Deform" modifier
    bend_modifier = obj.modifiers.new(name="Bend", type='SIMPLE_DEFORM')

    # Set the modifier type to 'BEND'
    bend_modifier.deform_method = 'BEND'

    # Set the bend angle (convert degrees to radians for Blender)
    bend_modifier.angle = angle_degrees * (3.14159 / 180.0)

    # Set the axis of deformation
    bend_modifier.deform_axis = bend_axis

    # --- 4. Apply the Modifier ---
    # This makes the deformation permanent.
    bpy.ops.object.modifier_apply(modifier=bend_modifier.name)

    print("Modifier applied successfully.")

    # --- 5. Export the Result ---
    try:
        bpy.ops.export_mesh.stl(filepath=output_path, use_selection=True)
    except Exception as e:
        print(f"Error: Failed to export STL file to '{output_path}'")
        print(e)
        sys.exit(1)

    print(f"Successfully exported bent mesh to '{output_path}'")


if __name__ == "__main__":
    # --- 6. Command-Line Argument Parsing ---
    # Blender's Python environment doesn't use sys.argv in the standard way.
    # Arguments passed after '--' are available in sys.argv.
    try:
        argv = sys.argv[sys.argv.index("--") + 1:]
    except ValueError:
        argv = []

    parser = argparse.ArgumentParser(description="Bend an STL mesh using Blender.")
    parser.add_argument("--input", required=True, help="Path to the input STL file.")
    parser.add_argument("--output", required=True, help="Path to save the output STL file.")
    parser.add_argument("--angle", type=float, required=True, help="Bend angle in degrees.")
    parser.add_argument("--axis", choices=['X', 'Y', 'Z'], default='X', help="Axis to bend along.")

    args = parser.parse_args(argv)

    # Call the main function with the parsed arguments
    bend_mesh(args.input, args.output, args.angle, args.axis)

    print("Script finished.")
