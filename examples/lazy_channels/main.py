import compressed_image as compressed
import numpy as np


# The compressed_image API provides multiple ways of generating lazy chunks to represent sparse data. This generates
# chunks represented by a single value that take up just a couple of bytes. This method is especially useful when you
# are planning to fill the channel with sparse data to then pass along to an image or somewhere else.
channel_zeros = compressed.Channel.zeros(np.uint16, 1920, 1080)
channel_full = compressed.Channel.full(np.uint16, fill_value=65535, width=1920, height=1080)

# We can also directly mirror another channel, this doesn't have to be a lazy channel!
channel_zeros_like = compressed.Channel.zeros_like(channel_zeros);
channel_full_like = compressed.Channel.full_like(channel_full, fill_value=24);

# When working with these lazy channels one has to slightly rethink how they approach modifying chunks within a 
# channel. This is because the usual `set_chunk` method will actually trigger a non-lazy chunk to be generated using
# up more memory and being slower
#
# So instead of:
for chunk_index in range(channel_zeros.num_chunks()):
    chunk = channel_zeros.get_chunk(chunk_index)
    # Modify channel
    channel_zeros.set_chunk(chunk_index, chunk)

# One should instead do the following:

for chunk_index in range(channel_zeros.num_chunks()):
	# Only conditionally modify the chunk, do this to avoid breaking the laziness of chunks unless necessary. 
	if True: # some condition
		chunk = channel_zeros.get_chunk(chunk_index)
		# Modify channel
		channel_zeros.set_chunk(chunk_index, chunk)

# While lazy chunks are mentioned as a good way of generating sparse data they are also generally the fastest way to 
# initialize a channel you are planning to populate fully as it is very cheap to instantiate and you only pay the 
# memory price as you go!