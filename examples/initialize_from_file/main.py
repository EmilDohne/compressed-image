import os

import compressed_image as compressed


filepath = os.path.join(os.path.dirname(__file__), "uv_grid_2048x2048.jpg")

# Read the image in chunks without having to load the whole file into memory, this is roughly as fast 
# or only slightly slower than reading the image data raw through OpenImageIO while taking only a fraction
# of the memory.
image = compressed.Image.read(filepath)

# Get references to the channels
r, g, b = image.channels("R", "G", "B")

# Now iterate e.g. channel r and modify its value (it is perfectly valid to modify multiple
# channels at once).
for chunk_index in range(r.num_chunks()):
    chunk_r = r.get_chunk(chunk_index)

    chunk_r[:] = 25

    r.set_chunk(chunk_index, chunk_r)