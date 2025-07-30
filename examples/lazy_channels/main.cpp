
#include <string>
#include <filesystem>
#include <algorithm>
#include <execution>

#include <compressed/channel.h>


auto main() -> int
{
	// The compressed_image API provides multiple ways of generating lazy chunks to represent sparse data. This generates
	// chunks represented by a single value that take up just a couple of bytes. This method is especially useful when you
	// are planning to fill the channel with sparse data to then pass along to an image or somewhere else.
	auto channel_zeros = compressed::channel<uint16_t>::zeros(1920, 1080);
	auto channel_full = compressed::channel<uint16_t>::full(1920, 1080, 65535 /* fill value */);

	// We can also directly mirror another channel, this doesn't have to be a lazy channel!
	auto channel_zeros_like = compressed::channel<uint16_t>::zeros_like(channel_zeros);
	auto channel_full_like = compressed::channel<uint16_t>::full_like(channel_full, 24 /* fill value */);

	// When working with these lazy channels one has to slightly rethink how they approach modifying chunks within a 
	// channel. This is because the usual `set_chunk` method will actually trigger a non-lazy chunk to be generated using
	// up more memory and being slower
	//
	// So instead of:
	for ([[maybe_unused]] auto chunk : channel_zeros)
	{
		// modify the chunk
	}

	// One should instead do the following:

	// Generate a vector with uninitialized data since we'll set it directly after.
	compressed::util::default_init_vector<uint16_t> chunk_buffer(channel_zeros.chunk_size());

	for (size_t chunk_idx = 0; chunk_idx < channel_zeros.num_chunks(); ++chunk_idx)
	{
		// Only conditionally modify the chunk, do this to avoid breaking the laziness of chunks unless necessary. 
		if (true /*some arbitrary condition*/)
		{
			// Note: we need to ensure this is set to chunk_elems(chunk_idx) as the last chunk of an channel may be smaller
			// than the rest of the chunks in the channel, this way we don't have to worry about the chunk size.
			std::span<uint16_t> chunk_span(chunk_buffer.data(), channel_zeros.chunk_elems(chunk_idx));

			channel_zeros.get_chunk(chunk_span, chunk_idx);

			// modify the data to your hearts content

			channel_zeros.set_chunk(chunk_span, chunk_idx);
		}
	}

	// While lazy chunks are mentioned as a good way of generating sparse data they are also generally the fastest way to 
	// initialize a channel you are planning to populate fully as it is very cheap to instantiate and you only pay the 
	// memory price as you go!
}