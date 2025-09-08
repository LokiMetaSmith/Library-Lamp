// =============================================================================
//
// Parametric Book-Shaped Enclosure for the E-Book Librarian
//
// This script generates a 3D-printable enclosure for the E-Book Librarian project.
// It is designed to be easily customizable by changing the parameters in the
// "Parameters" section below.
//
// Created by Jules, AI Software Engineer
//
// --- HOW TO USE ---
// 1.  Verify the dimensions of your specific components (ESP32 board, SD module).
// 2.  Update the variables in the "Assumed Component Dimensions" section.
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
$fn = 32; // Default fragment number for smooth curves

// --- Assumed Component Dimensions ---
//
// !! IMPORTANT !!
// These dimensions are based on common dev board sizes and should be
// verified with your specific components before printing.
//

// ESP32-S3-USB-OTG Board
esp32_width = 70;
esp32_depth = 30;
esp32_height = 8; // Height of the board + tallest components (e.g., USB port)
esp32_mount_hole_spacing_x = esp32_width - 5; // Assumed distance between hole centers
esp32_mount_hole_spacing_y = esp32_depth - 5; // Assumed distance between hole centers

// HiLetgo MicroSD Card Module
sd_card_width = 20;
sd_card_depth = 20;
sd_card_height = 6;
sd_card_mount_hole_spacing_x = sd_card_width - 4; // Assumed
sd_card_mount_hole_spacing_y = sd_card_depth - 4; // Assumed

// LED Strip
led_strip_width = 10;
led_strip_height = 3;

// --- Book Dimensions ---
// These are calculated automatically based on component sizes.
book_width = esp32_width + 4 * wall_thickness;
book_depth = esp32_depth + sd_card_depth + 4 * wall_thickness;
book_height = esp32_height + 2 * wall_thickness + pcb_mount_post_dia; // Make room for posts
spine_radius = 5; // To make the spine rounded

// --- Cutout Dimensions ---
sd_card_slot_width = 26; // Wider than a standard SD card
sd_card_slot_height = 3;
usb_a_port_width = 14;
usb_a_port_height = 7;
led_wire_hole_dia = 4;


// =============================================================================
// == Modules (Reusable Components) ==
// =============================================================================

// --- Main Case Module ---
// This module creates the main body of the book enclosure.
module book_case() {
    difference() {
        // --- 1. The Main Solid Body of the Book (with mounts) ---
        union() {
            // Main box shape
            difference() {
                union() {
                    // Main rectangular part
                    cube([book_width - spine_radius, book_depth, book_height]);
                    // Rounded spine
                    translate([book_width - spine_radius, spine_radius, 0]) {
                        cylinder(h = book_height, r = spine_radius);
                    }
                }
                // Hollow out the inside
                translate([wall_thickness, wall_thickness, wall_thickness]) {
                    cube([book_width - 2 * wall_thickness, book_depth - 2 * wall_thickness, book_height + 1]);
                }
            }

            // --- Add Mounting Posts ---
            post_height = wall_thickness + 2;
            esp32_x_offset = (book_width - esp32_width) / 2;
            esp32_y_offset = wall_thickness + clearance;

            translate([esp32_x_offset, esp32_y_offset, 0]) cylinder(h = post_height, d = pcb_mount_post_dia);
            translate([esp32_x_offset + esp32_mount_hole_spacing_x, esp32_y_offset, 0]) cylinder(h = post_height, d = pcb_mount_post_dia);
            translate([esp32_x_offset, esp32_y_offset + esp32_mount_hole_spacing_y, 0]) cylinder(h = post_height, d = pcb_mount_post_dia);
            translate([esp32_x_offset + esp32_mount_hole_spacing_x, esp32_y_offset + esp32_mount_hole_spacing_y, 0]) cylinder(h = post_height, d = pcb_mount_post_dia);

            sd_x_offset = (book_width - sd_card_width) / 2;
            sd_y_offset = esp32_y_offset + esp32_depth + clearance;

            translate([sd_x_offset, sd_y_offset, 0]) cylinder(h = post_height, d = pcb_mount_post_dia);
            translate([sd_x_offset + sd_card_mount_hole_spacing_x, sd_y_offset, 0]) cylinder(h = post_height, d = pcb_mount_post_dia);
            translate([sd_x_offset, sd_y_offset + sd_card_mount_hole_spacing_y, 0]) cylinder(h = post_height, d = pcb_mount_post_dia);
            translate([sd_x_offset + sd_card_mount_hole_spacing_x, sd_y_offset + sd_card_mount_hole_spacing_y, 0]) cylinder(h = post_height, d = pcb_mount_post_dia);
        }

        // --- 2. Cutouts ---
        // Screw holes for mounts
        hole_height = wall_thickness + 3;
        translate([esp32_x_offset, esp32_y_offset, -1]) cylinder(h = hole_height, d = pcb_mount_hole_dia);
        translate([esp32_x_offset + esp32_mount_hole_spacing_x, esp32_y_offset, -1]) cylinder(h = hole_height, d = pcb_mount_hole_dia);
        translate([esp32_x_offset, esp32_y_offset + esp32_mount_hole_spacing_y, -1]) cylinder(h = hole_height, d = pcb_mount_hole_dia);
        translate([esp32_x_offset + esp32_mount_hole_spacing_x, esp32_y_offset + esp32_mount_hole_spacing_y, -1]) cylinder(h = hole_height, d = pcb_mount_hole_dia);

        translate([sd_x_offset, sd_y_offset, -1]) cylinder(h = hole_height, d = pcb_mount_hole_dia);
        translate([sd_x_offset + sd_card_mount_hole_spacing_x, sd_y_offset, -1]) cylinder(h = hole_height, d = pcb_mount_hole_dia);
        translate([sd_x_offset, sd_y_offset + sd_card_mount_hole_spacing_y, -1]) cylinder(h = hole_height, d = pcb_mount_hole_dia);
        translate([sd_x_offset + sd_card_mount_hole_spacing_x, sd_y_offset + sd_card_mount_hole_spacing_y, -1]) cylinder(h = hole_height, d = pcb_mount_hole_dia);

        // SD Card Slot Cutout (on the spine)
        translate([book_width, (book_depth / 2) - (sd_card_slot_width / 2), wall_thickness + 2]) {
            rotate([0, 90, 0])
            cube([wall_thickness + 2, sd_card_slot_width, sd_card_slot_height]);
        }

        // USB Host Port Cutout (on the front)
        translate([(book_width / 2) - (usb_a_port_width / 2), -1, (book_height / 2) - (usb_a_port_height / 2)]) {
           cube([usb_a_port_width, wall_thickness + 2, usb_a_port_height]);
        }

        // LED Channel (at the bottom)
        translate([-1, wall_thickness, -1]) {
            cube([book_width + 2, led_strip_width, led_strip_height + 1]);
        }

        // LED Wire Hole
        translate([(book_width / 2), wall_thickness, led_strip_height]) {
            rotate([90, 0, 0])
            cylinder(h = wall_thickness + 2, d = led_wire_hole_dia);
        }
    }
}

// --- Lid Module ---
// This module creates the lid for the enclosure.
module book_lid() {
    lip_depth = 1; // How deep the inset lip is

    difference() {
        // Main lid body
        cube([book_width - 2 * wall_thickness, book_depth - 2 * wall_thickness, wall_thickness]);

        // Inner lip for a snug fit. This part goes inside the main case walls.
        translate([lip_depth, lip_depth, -1]) {
            cube([
                book_width - 2 * wall_thickness - 2 * lip_depth,
                book_depth - 2 * wall_thickness - 2 * lip_depth,
                wall_thickness + 2
            ]);
        }
    }
}


// =============================================================================
// == Main Assembly ==
// =============================================================================

// This section controls what is rendered.
// By default, it shows both parts separated for printing.
// You can comment out one or the other to render a single part for export.

// Render the main case
book_case();

// Render the lid, moved up along the Z-axis for visibility
translate([0, 0, book_height + 5]) {
    book_lid();
}
