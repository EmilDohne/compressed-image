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



.. _channel_struct:

channel 
************

.. tab:: c++


    .. doxygenstruct:: compressed::channel
            :members:
            :undoc-members:

.. tab:: python

    tbd