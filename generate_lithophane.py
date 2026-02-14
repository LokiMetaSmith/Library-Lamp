import argparse
import sys
from PIL import Image
import numpy as np
import struct

def write_stl(triangles, output_path):
    """
    Writes a list of triangles to a binary STL file.
    triangles: list or numpy array of shape (N, 3, 3) where each triangle has 3 vertices (x, y, z).
    """
    num_triangles = len(triangles)

    with open(output_path, 'wb') as f:
        # 80 bytes header
        f.write(b'\0' * 80)
        # 4 bytes number of triangles
        f.write(struct.pack('<I', num_triangles))

        # Prepare data for writing
        # Each triangle: normal (3f), v1 (3f), v2 (3f), v3 (3f), attr (2b)
        # Total 50 bytes per triangle

        # We can construct a structured array for efficient writing
        dtype = np.dtype([
            ('normal', '<f4', (3,)),
            ('v1', '<f4', (3,)),
            ('v2', '<f4', (3,)),
            ('v3', '<f4', (3,)),
            ('attr', '<u2')
        ])

        data = np.zeros(num_triangles, dtype=dtype)

        # Convert triangles to numpy array if not already
        tris = np.array(triangles, dtype='<f4')

        data['v1'] = tris[:, 0, :]
        data['v2'] = tris[:, 1, :]
        data['v3'] = tris[:, 2, :]

        # Normal vector calculation (optional, many viewers ignore it or recalculate)
        # For simplicity, we leave it as (0,0,0)

        f.write(data.tobytes())

def generate_mesh(width, height, thickness_grid):
    """
    Generates a closed mesh (volume) for the lithophane.
    thickness_grid: 2D numpy array of thickness values.
    width: physical width of the lithophane.
    height: physical height of the lithophane.
    """
    rows, cols = thickness_grid.shape
    dx = width / (cols - 1) if cols > 1 else width
    dy = height / (rows - 1) if rows > 1 else height

    triangles = []

    # Pre-calculate coordinates
    x = np.linspace(0, width, cols)
    y = np.linspace(0, height, rows)
    xv, yv = np.meshgrid(x, y) # shape (rows, cols)

    # Top surface (z = thickness)
    # Bottom surface (z = 0)

    # We iterate over quads (cells)
    for r in range(rows - 1):
        for c in range(cols - 1):
            # Indices for the quad
            # (r, c) -> (r, c+1) -> (r+1, c+1) -> (r+1, c)

            # Coordinates
            x0 = xv[r, c]
            x1 = xv[r, c+1]
            y0 = yv[r, c]
            y1 = yv[r+1, c]

            # Top Z
            zt00 = thickness_grid[r, c]
            zt01 = thickness_grid[r, c+1]
            zt11 = thickness_grid[r+1, c+1]
            zt10 = thickness_grid[r+1, c]

            # Top vertices
            vt00 = (x0, y0, zt00)
            vt01 = (x1, y0, zt01)
            vt11 = (x1, y1, zt11)
            vt10 = (x0, y1, zt10)

            # Bottom vertices (z=0)
            vb00 = (x0, y0, 0)
            vb01 = (x1, y0, 0)
            vb11 = (x1, y1, 0)
            vb10 = (x0, y1, 0)

            # Top Surface (CCW)
            # Tri 1: vt00, vt01, vt11
            triangles.append([vt00, vt01, vt11])
            # Tri 2: vt00, vt11, vt10
            triangles.append([vt00, vt11, vt10])

            # Bottom Surface (CW to point down)
            # Tri 1: vb00, vb10, vb11
            triangles.append([vb00, vb10, vb11])
            # Tri 2: vb00, vb11, vb01
            triangles.append([vb00, vb11, vb01])

            # Side Walls
            # We only need to generate walls at the boundaries of the grid

    # Generate side walls by iterating over boundary edges

    # Top edge (r=0)
    for c in range(cols - 1):
        x0 = xv[0, c]
        x1 = xv[0, c+1]
        y = yv[0, c] # should be 0
        z0 = thickness_grid[0, c]
        z1 = thickness_grid[0, c+1]

        vt0 = (x0, y, z0)
        vt1 = (x1, y, z1)
        vb0 = (x0, y, 0)
        vb1 = (x1, y, 0)

        # Face pointing -Y (CW if looking from -Y?)
        # Normal should be (0, -1, 0)
        # Sequence: vt0, vt1, vb1
        #           vt0, vb1, vb0
        # Let's check CCW:
        # vt0 -> vt1 -> vb1 : vector (dx, 0, dz), vector (0, 0, -z). Cross prod ~ (0, +y, 0)?
        # Wait. (1,0,0) x (0,0,-1) = (0, 1, 0). Points +Y (into the model). Bad.
        # We want normal pointing OUT (-Y).
        # So we need clockwise order if viewed from outside?
        # Or CCW order if viewed from outside.
        # Viewed from front (-Y), x goes left to right. z goes up.
        # vt0 (left top), vt1 (right top), vb1 (right bottom), vb0 (left bottom).
        # CCW: vt0, vb0, vb1, vt1.
        # Tri 1: vt0, vb0, vb1
        # Tri 2: vt0, vb1, vt1

        triangles.append([vt0, vb0, vb1])
        triangles.append([vt0, vb1, vt1])

    # Bottom edge (r=rows-1)
    for c in range(cols - 1):
        x0 = xv[rows-1, c]
        x1 = xv[rows-1, c+1]
        y = yv[rows-1, c] # should be height
        z0 = thickness_grid[rows-1, c]
        z1 = thickness_grid[rows-1, c+1]

        vt0 = (x0, y, z0)
        vt1 = (x1, y, z1)
        vb0 = (x0, y, 0)
        vb1 = (x1, y, 0)

        # Face pointing +Y
        # Viewed from back (+Y), x goes right to left? No, x is same.
        # Normal should be (0, 1, 0).
        # CCW: vt1, vb1, vb0, vt0.
        # Tri 1: vt1, vb1, vb0
        # Tri 2: vt1, vb0, vt0

        triangles.append([vt1, vb0, vb1])
        triangles.append([vt1, vb0, vt0])

    # Left edge (c=0)
    for r in range(rows - 1):
        x = xv[r, 0] # should be 0
        y0 = yv[r, 0]
        y1 = yv[r+1, 0]
        z0 = thickness_grid[r, 0]
        z1 = thickness_grid[r+1, 0]

        vt0 = (x, y0, z0)
        vt1 = (x, y1, z1)
        vb0 = (x, y0, 0)
        vb1 = (x, y1, 0)

        # Face pointing -X
        # Normal (-1, 0, 0)
        # CCW: vt1, vb1, vb0, vt0
        # Tri 1: vt1, vb1, vb0
        # Tri 2: vt1, vb0, vt0

        triangles.append([vt1, vb1, vb0])
        triangles.append([vt1, vb0, vt0])

    # Right edge (c=cols-1)
    for r in range(rows - 1):
        x = xv[r, cols-1] # should be width
        y0 = yv[r, cols-1]
        y1 = yv[r+1, cols-1]
        z0 = thickness_grid[r, cols-1]
        z1 = thickness_grid[r+1, cols-1]

        vt0 = (x, y0, z0)
        vt1 = (x, y1, z1)
        vb0 = (x, y0, 0)
        vb1 = (x, y1, 0)

        # Face pointing +X
        # Normal (1, 0, 0)
        # CCW: vt0, vb0, vb1, vt1
        # Tri 1: vt0, vb0, vb1
        # Tri 2: vt0, vb1, vt1

        triangles.append([vt0, vb0, vb1])
        triangles.append([vt0, vb1, vt1])

    return triangles

def main():
    parser = argparse.ArgumentParser(description='Generate a lithophane STL from an image.')
    parser.add_argument('--image', required=True, help='Input image path')
    parser.add_argument('--output', required=True, help='Output STL path')
    parser.add_argument('--width', type=float, default=100.0, help='Width of the lithophane in mm')
    parser.add_argument('--height', type=float, default=100.0, help='Height of the lithophane in mm')
    parser.add_argument('--min-thickness', type=float, default=0.8, help='Minimum thickness in mm')
    parser.add_argument('--max-thickness', type=float, default=3.0, help='Maximum thickness in mm')
    parser.add_argument('--resolution', type=float, default=0.2, help='Pixel size in mm')

    args = parser.parse_args()

    try:
        img = Image.open(args.image).convert('L')
    except Exception as e:
        print(f"Error loading image: {e}")
        sys.exit(1)

    # Calculate target resolution
    target_cols = int(args.width / args.resolution)
    target_rows = int(args.height / args.resolution)

    if target_cols < 2 or target_rows < 2:
        print("Error: Resolution too low (dimensions too small).")
        sys.exit(1)

    print(f"Resizing image to {target_cols}x{target_rows}...")
    img = img.resize((target_cols, target_rows), Image.Resampling.LANCZOS)

    pixels = np.array(img)
    # Normalize 0-1
    norm_pixels = pixels / 255.0

    # Calculate thickness
    # Darker (0) -> Thicker (max)
    # Lighter (1) -> Thinner (min)
    thickness_grid = args.max_thickness - (norm_pixels * (args.max_thickness - args.min_thickness))

    print("Generating mesh...")
    triangles = generate_mesh(args.width, args.height, thickness_grid)

    print(f"Writing STL to {args.output} ({len(triangles)} triangles)...")
    write_stl(triangles, args.output)

    print("Done.")

if __name__ == "__main__":
    main()
