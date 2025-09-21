// E-Book Librarian - OpenSCAD Enclosure Template
//
// This script provides a basic template for creating a custom, book-shaped
// enclosure for the E-Book Librarian project. You can modify the parameters
// in the "User-Configurable Variables" section to fit your specific
// components and desired book size.
//
// How to Use:
// 1. Install OpenSCAD from https://openscad.org/
// 2. Open this file in the OpenSCAD editor.
// 3. Modify the variables below to your liking.
// 4. Press F5 to render a preview.
// 5. Once you are happy with the design, press F6 to render the final model.
// 6. Export the model as an STL file (File > Export > Export as STL...).

// --- User-Configurable Variables ---

// --- Internal Dimensions ---
// Measure your assembled electronics (ESP32, SD module, etc.) and add some tolerance.
interior_width = 80;  // (mm)
interior_depth = 120; // (mm)
interior_height = 25; // (mm)

// --- Book Dimensions ---
wall_thickness = 3;   // Thickness of the book's walls
cover_overhang = 5;   // How much the "cover" extends beyond the "pages"
spine_radius = 5;     // The rounding of the book's spine

// --- Lithophane (Optional) ---
// If you have an STL file for a lithophane you want to put on the cover.
litho_stl_path = "path/to/your/lithophane.stl"; // Change this to the actual path
litho_enabled = false; // Set to true to see the placeholder lithophane

// --- Helper variables (do not modify) ---
outer_width = interior_width + (wall_thickness * 2);
outer_depth = interior_depth + (wall_thickness * 2);
outer_height = interior_height + wall_thickness; // Bottom wall only, top is open

cover_width = outer_width + (cover_overhang * 2);
cover_depth = outer_depth + cover_overhang; // Overhang on front edge only
cover_height = wall_thickness;

// --- Main Module ---

// Render the two main parts of the enclosure: the box and the cover.
// You can comment out one or the other to render them separately for printing.
book_box();
translate([0, 0, outer_height]) cover();


// --- Component Modules ---

// Creates the main hollow box for the electronics (the "pages" of the book)
module book_box() {
    difference() {
        // Outer shape (the block of pages)
        book_block(outer_width, outer_depth, outer_height, spine_radius);

        // Subtract the hollow interior
        translate([wall_thickness, wall_thickness, wall_thickness]) {
            cube([interior_width, interior_depth, interior_height]);
        }
    }
}

// Creates the top cover of the book
module cover() {
    difference() {
        // Main cover shape
        book_block(cover_width, cover_depth, cover_height, spine_radius + cover_overhang);

        // Inset the "pages" area to create a lip
        translate([cover_overhang, cover_overhang, -1]) {
             book_block(outer_width, outer_depth, cover_height + 2, spine_radius);
        }
    }

    // Placeholder for a lithophane on the cover
    if (litho_enabled) {
        // This is where you would import your lithophane STL.
        // You will need to translate, rotate, and scale it to fit your cover.
        // Example:
        // translate([cover_width/2, cover_depth/2, cover_height]) {
        //   scale([0.5, 0.5, 0.5]) { // Scale to fit
        //     import(litho_stl_path);
        //   }
        // }

        // For demonstration, we'll just show a simple shape.
        translate([20, 20, cover_height]) {
            linear_extrude(height=1) {
                text("Your Litho Here", size=10);
            }
        }
    }
}


// Helper module to create the basic rounded-spine block shape
module book_block(width, depth, height, radius) {
    hull() {
        // Four points to define the main rectangular part
        cube([width - radius, depth, height]);
        translate([width - radius, 0, 0]) {
            cylinder(h=height, r=radius, $fn=100);
        }
    }
}
