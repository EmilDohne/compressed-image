..
  Copyright Contributors to the compressed-image project.

.. _compressed_channel:

channel
########

The ``compressed::channel<T>`` represents a single channel which is either part of a ``compressed::image<T>`` or
standalone. It stores its data as a compressed buffer split up into smaller chunks. This allows for very
memory efficient storage and modification as we only ever need to decompress one chunk at a time for which we 
can reuse the same memory buffer.

Iterating over a channel
*************************

The C++ class exposes methods for handling this decompression/recompression for you. It allows for iteration over
a channel almost like you would iterate over a regular std::vector<T> without having to worry about the decompression
or recompression.

.. tab:: c++

    .. code-block:: cpp

        compressed::channel<T> my_channel = ...;

        // This will iterate through all the chunks of the channel, decompressing and recompressing
        // them, reusing the same internal buffer. On dereference you get a chunk
        // span which is a std::span<T> with additional metadata for e.g. finding the x and y coordinate
        for (compressed::container::chunk_span<T> chunk : channel)
        {
            for (auto& [index, pixel] : std::views::enumerate(chunk))
            {
                // The index here is local to the chunk itself, we will take care
                // of computing its global coordinate.
                auto x = chunk.x(index);
                auto y = chunk.y(index);

                // You can now modify the pixel in-place
                pixel = static_cast<T>(x) * y;
            }
        }
        // Note that the chunk_span is only valid as long as the iterator is alive!


.. tab:: python

    .. code-block:: python

        import compressed_image as compressed
        
        channel: compressed.Channel = ...

        buffer = np.ndarray((channel.chunk_elems(),), dtype = channel.dtype)
        for i in range(channel.num_chunks() - 1):
            channel.get_chunk(i, buffer)
            buffer[:] = i
            channel.set_chunk(i, buffer)

        final_buffer = channel.get_chunk(channel.num_chunks() -1)
        final_buffer[:] = channel.num_chunks() - 1
        channel.set_chunk(channel.num_chunks() -1, final_buffer)


Lazy channels
**************

The ``compressed::channel<T>`` additionally has support for lazy channels. These can be created via e.g.

``compressed::channel<T>::zeros``
``compressed::channel<T>::zeros_like``
``compressed::channel<T>::full``
``compressed::channel<T>::full_like``

Lazy channels are channels which store a single value per-chunk until that chunk is filled. This is especially efficient
for largely sparse data and is used extensively by the `cryptomatte-api <https://github.com/EmilDohne/cryptomatte-api>`_. 

When using lazily generated channels, it is usually advised to not iterate over them as shown above (although it is
entirely valid) as that will store explicit data on each of the chunks making the further memory savings negligible. 
Instead, it is recommended to explicitly set the chunks that will contain data.

Take for example a mask channel which will only cover a small portion of the image, rather than having to explicitly store
compressed data for the whole image (which won't compress 100% and will have a non-trivial overhead), 
we can just store a single value for all chunks outside of this mask area. 

.. tab:: c++

    .. code-block:: cpp

        auto lazy_channel = compressed::channel<T>::zeros(2048 /*width*/, 2048 /*height*/);

        // Generate a buffer which will hold the chunk data, we never have to allocate the whole image
        std::vector<T> buffer(lazy_channel.chunk_elems());

        // Iterate over all the chunks and modify only specific ones.
        for (size_t i = 0; i < lazy_channel.num_chunks())
        {
            if (/* some arbitrary condition is true */)
            {
                // Note that we need to take a subspan of the buffer here. All chunks within a 
                // channel are guaranteed to be the same size, except for the last chunk which
                // may be smaller, we do not pad up to the chunk size so we need to ensure 
                // we do not try to set too much data.
                std::span<T> chunk_span(buffer.begin(), buffer.begin() + lazy_channel.chunk_elems(i));

                // Setting this to a span which does not have the size compressed::channel<T>::chun_elems(index)
                // would raise an exception here.
                lazy_channel.set_chunk(chunk_span, i);
            }
        }

    .. note::

        Even though you may provide your own chunk size, we internally ensure that this is always a multiple of sizeof(T)
        so you can always safely convert from ``chunk_size`` -> ``chunk_elems``.


.. tab:: python

    .. code-block:: python

        import compressed_image as compressed
        
        channel = compressed.Channel.zeros(np.float16, width = 2048, height = 2048)

        # Now we can iterate over all the channels and only modify specific ones
        for chunk_idx in range(channel.num_chunks())
            if some_condition:
                decompressed = channel.get_chunk(chunk_idx)

                # Do some modifications
                decompressed[:] = 100

                # Now ensure we set back the chunk, as otherwise the data will not update!
                channel.set_chunk(chnk_ix, decompressed)

    

Memory layout
****************

``compressed::channel<T>`` are internally stored as chunks of scanline data, with each chunk representing n scanlines
(this may also include partial scanlines, although in most cases it will be aligned to scanlines). If we visualize this,
a chunk could therefore look like this:

.. image:: ../images/chunked_image.jpg

As either the chunk size grows, or the image shrinks, the chunk will take up more or less vertical space in the image.
This is important to know as unlike e.g. tiled images this means that if you wish to access a vertical slice in an image,
this will result in you having to decompress the entire image.

The chunk size is additionally capped such that if you have a channel that takes up less bytes than ``chunk_size``,
the chunk size will be adjusted to be == width * height * sizeof(T). Therefore there is no need to modify the chunk size
if you plan on only compressing smaller images.

Block size
============

You may notice the constructor of channels taking a ``block_size`` parameter. This parameter controls the size of blocks
within chunks. The compressed data is stored in 3 levels with the ``blocks`` being the lowest level. It goes channels >
chunks > blocks. 

While the user can set the block size, they cannot extract just a single block of data from a chunk, and it is also not
transparent where a block starts or ends. 

The main thing to worry about when it comes to block size is knowing that:

**A**, it is the smallest unit and is what will be compressed in the end (chunks simply hold collections of blocks).

**B**, it should roughly fit into the L1 cache of your CPU for better data throughput.

These implementation details are usually tackled just fine with ``compressed::s_default_blocksize``


.. _channel_struct:

Channel Struct 
**************

.. tab:: c++


    .. doxygenstruct:: compressed::channel
            :members:
            :undoc-members:

.. tab:: python

    .. autoclass:: compressed_image.Channel
        :members:
        :inherited-members:

        .. automethod:: __init__