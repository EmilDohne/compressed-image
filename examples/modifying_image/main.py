import os

import compressed_image as compressed
import numpy as np


filepath = os.path.join(os.path.dirname(__file__), "images/uv_grid_2048x2048.jpg")

# Read the image in chunks without having to load the whole file into memory, this is roughly as fast 
# or only slightly slower than reading the image data raw through OpenImageIO while taking only a fraction
# of the memory.
image = compressed.Image.read(np.uint8, filepath, subimage=0)

# Now that we have the image, we might want to add another channel into the mix, (note that block and chunk size
# must be identical!):
image.add_channel(
	np.full((image.height, image.width), 255, np.uint8), 
	image.width, 
	image.height,
	"Z",
	compressed.Codec.zstd, # compression codec, channels within an image may have different compression codecs
	5,	# compression level, this may be different across channels in the image
	image.block_size(), # block size, this must match the rest of the image as otherwise we will  raise an error!
	image.chunk_size(),  # chunk size, this must match the rest of the image as otherwise we will raise an error!
)

# The compressed_image api doesn't validate what kind of channels you add into the image, so if you e.g.
# add the channel 'R' twice that is perfectly valid and accepted.

# If instead you wish to remove a channel you can call either:
image.remove_channel("R")

# or 
image.remove_channel(0)