// =============================================================================
//
// Parametric Book-Shaped Enclosure for the E-Book Librarian
//
// This script generates a simple, robust, 3D-printable enclosure.
// All dimensions are in millimeters (mm).
//
// Created by Jules, AI Software Engineer
//
// --- HOW TO USE ---
// 1.  Verify the dimensions of your specific components.
// 2.  Update the variables in the "Component Dimensions" section.
// 3.  Render the model (F6 in OpenSCAD) to generate the STL file.
// 4.  The script generates two parts: the main case and a lid. You should
//     print them as separate files.
//
// =============================================================================


// =============================================================================
// == Parameters ==
// =============================================================================

// --- Global Design Parameters ---
wall_thickness = 2;
clearance = 2; // General clearance around components
pcb_mount_hole_dia = 2.5; // Diameter for M2.5 screws
pcb_mount_post_dia = 6;  // Diameter of the mounting posts
$fn = 64; // Default fragment number for smooth curves

// --- Component Dimensions (ASSUMED - PLEASE VERIFY) ---
esp32_width = 70;
esp32_depth = 30;
esp32_height = 8;
sd_card_width = 20;
sd_card_depth = 20;

// --- Calculated Dimensions ---
internal_width = esp32_width + clearance * 2;
internal_depth = esp32_depth + sd_card_depth + clearance * 3;
internal_height = esp32_height + clearance * 2;

case_width = internal_width + wall_thickness * 2;
case_depth = internal_depth + wall_thickness * 2;
case_height = internal_height + wall_thickness * 2;


// =============================================================================
// == Modules ==
// =============================================================================

module main_case() {
    difference() {
        // Outer Box
        cube([case_width, case_depth, case_height]);

        // Inner Hollow
        translate([wall_thickness, wall_thickness, wall_thickness]) {
            cube([internal_width, internal_depth, case_height]);
        }

        // USB Port Cutout
        translate([case_width / 2 - 7, -1, case_height / 2 - 4]) {
            cube([14, wall_thickness + 2, 8]);
        }

        // SD Card Cutout
        translate([-1, case_depth / 2 - 13, wall_thickness]) {
            cube([wall_thickness + 2, 26, 3]);
        }
    }
}

module lid() {
    lip_height = 1;
    lip_inset = 0.5;

    // Main Lid Plate
    cube([internal_width, internal_depth, wall_thickness]);

    // Lip to fit inside the case
    translate([lip_inset, lip_inset, -lip_height]) {
        cube([internal_width - 2 * lip_inset, internal_depth - 2 * lip_inset, lip_height]);
    }
}

// =============================================================================
// == Assembly ==
// =============================================================================

// Render the main case
main_case();

// Render the lid, moved up for visibility
translate([wall_thickness, wall_thickness, case_height + 5]) {
    lid();
}
