#!/bin/bash

# ==============================================================================
# E-Book Librarian - Unified Enclosure Build Script
# ==============================================================================
#
# This script orchestrates the entire process of generating a 3D-printable,
# custom book enclosure. It calls a sequence of tools to:
#   1. Generate a flat lithophane from an image.
#   2. Generate the main book enclosure using OpenSCAD.
#   3. Bend the flat lithophane using Blender.
#   4. Combine the enclosure and the bent lithophane into a final model.
#
# --- Prerequisites ---
# - OpenSCAD:         Install from https://openscad.org/
# - Blender:          Install from https://www.blender.org/
# - Lithophane Tool:  You must provide your own tool for this step.
#                     A good open-source option is LithoMaker:
#                     https://github.com/muldjord/lithomaker
#
# Make sure 'openscad' and 'blender' are in your system's PATH.
#
# --- Usage ---
#   ./build_enclosure.sh -i <path_to_image>
#
# --- Arguments ---
#   -i, --image:  Required. The path to the input image for the lithophane.
#
# ==============================================================================

# --- Configuration ---

# Directory to store intermediate and final STL files.
BUILD_DIR="build_enclosure"

# Parameters for the OpenSCAD enclosure.
# These could also be exposed as command-line arguments.
INTERIOR_WIDTH=80
INTERIOR_DEPTH=120
INTERIOR_HEIGHT=25
WALL_THICKNESS=3

# Parameters for the Blender bend operation.
BEND_ANGLE=45 # Angle to bend the lithophane for the spine

# --- Script Logic ---

set -e # Exit immediately if a command exits with a non-zero status.

# --- Argument Parsing ---
INPUT_IMAGE=""
while [[ $# -gt 0 ]]; do
    key="$1"
    case $key in
        -i|--image)
        INPUT_IMAGE="$2"
        shift
        shift
        ;;
        *)
        echo "Unknown option: $1"
        exit 1
        ;;
    esac
done

if [ -z "$INPUT_IMAGE" ]; then
    echo "Error: Input image is required."
    echo "Usage: $0 -i <path_to_image>"
    exit 1
fi

# --- Main Functions ---

# Placeholder function for generating the lithophane.
# You will need to replace this with the actual command for your tool.
generate_flat_lithophane() {
    echo "--- [Step 1/4] Generating Flat Lithophane (Placeholder) ---"
    echo "This is a placeholder. Please replace this with your lithophane tool."

    # Example using a hypothetical tool 'lithomaker-cli':
    # lithomaker-cli --image "$1" --output "$2" --width $INTERIOR_DEPTH

    # For demonstration, we'll just create a dummy file.
    openscad -o "$2" -D 'cube([120, 80, 2]);'

    echo "Created dummy flat lithophane at $2"
    echo ""
}

# Bends the flat lithophane using the Blender script.
bend_the_lithophane() {
    echo "--- [Step 2/4] Bending Lithophane with Blender ---"

    blender --background --python bend_mesh.py -- \
        --input "$1" \
        --output "$2" \
        --angle $BEND_ANGLE \
        --axis "Y"

    echo "Bent lithophane saved to $2"
    echo ""
}

# Generates the 3D models using the OpenSCAD template.
generate_enclosure_models() {
    echo "--- [Step 3/4] Generating Enclosure Models with OpenSCAD ---"

    # Generate the individual parts laid out flat for printing
    echo "Generating individual parts file..."
    openscad -o "$BUILD_DIR/printable_parts.stl" \
        -D "build_mode=\"parts\"" \
        -D "interior_width=$INTERIOR_WIDTH" \
        -D "interior_depth=$INTERIOR_DEPTH" \
        -D "interior_height=$INTERIOR_HEIGHT" \
        -D "wall_thickness=$WALL_THICKNESS" \
        enclosure_template.scad

    echo "Individual parts saved to $BUILD_DIR/printable_parts.stl"
    echo ""
}

# Combines the enclosure parts and the bent lithophane.
combine_final_assembly() {
    echo "--- [Step 4/4] Combining Final Assembly with OpenSCAD ---"

    # Generate the final, fully assembled model
    echo "Generating final assembled model..."
    openscad -o "$BUILD_DIR/final_assembly.stl" \
        -D "build_mode=\"assembly\"" \
        -D "interior_width=$INTERIOR_WIDTH" \
        -D "interior_depth=$INTERIOR_DEPTH" \
        -D "interior_height=$INTERIOR_HEIGHT" \
        -D "wall_thickness=$WALL_THICKNESS" \
        -D "bent_litho_stl_path=\"$1\"" \
        enclosure_template.scad

    echo "Final assembled model saved to $BUILD_DIR/final_assembly.stl"
    echo ""
}


# --- Execution ---

# Create the build directory if it doesn't exist.
mkdir -p $BUILD_DIR

FLAT_LITHO_STL="$BUILD_DIR/flat_litho.stl"
BENT_LITHO_STL="$BUILD_DIR/bent_litho.stl"

generate_flat_lithophane "$INPUT_IMAGE" "$FLAT_LITHO_STL"
bend_the_lithophane "$FLAT_LITHO_STL" "$BENT_LITHO_STL"
generate_enclosure_models
combine_final_assembly "$BENT_LITHO_STL"

echo "Build process complete. Final files are in the '$BUILD_DIR' directory."
