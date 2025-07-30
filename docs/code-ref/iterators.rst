..
  Copyright Contributors to the compressed-image project.

.. _iterators:

iterators (cpp)
#################

.. note::

    This section only applies to the **C++ bindings**. The Python bindings do not support direct iteration. 
    For Python usage, please refer to :ref:`read_from_file` for iterating chunks through the API.

iterator
***********

The ``channel_iterator`` is the primary mechanism enabling range-based for loops over a ``compressed::channel<T>``. When iterating using:

.. code-block:: cpp

    for (auto chunk : channel)

the ``chunk`` is obtained by dereferencing the iterator (``operator*``), returning a :ref:`chunk_span` object that provides 
a view into the corresponding chunk of data in the channel. 


.. doxygenstruct:: NAMESPACE_COMPRESSED_IMAGE::channel_iterator
    :members:
    :undoc-members:


.. _chunk_span:

chunk span
***********

The ``container::chunk_span<T>`` is a lightweight view over a chunk in a ``channel<T>``. This object is returned when dereferencing a ``channel_iterator``.

Each ``chunk_span`` provides access to:

- The chunk's pixel data as a contiguous range.
- The ability to compute global ``(x, y)`` coordinates from local chunk indices.
- In-place read/write access to pixels.

Here is an example of how to iterate through a channel and compute pixel values based on global coordinates:

.. code-block:: cpp

    compressed::channel<T> my_channel = ...;

    for (compressed::container::chunk_span<T> chunk : my_channel)
    {
        for (auto& [index, pixel] : std::views::enumerate(chunk))
        {
            // The index is local to the chunk. Compute global coordinates:
            auto x = chunk.x(index);
            auto y = chunk.y(index);

            // Modify the pixel in-place
            pixel = static_cast<T>(x) * y;
        }
    }

.. warning::

    A `chunk_span` is only valid for the lifetime of the iterator it was obtained from. 
    Do **not** store or use a ``chunk_span`` after the loop or iterator scope has ended.

.. doxygenstruct:: NAMESPACE_COMPRESSED_IMAGE::container::chunk_span
    :members:
    :undoc-members:


zip channels
*************

The ``compressed::ranges::zip`` utility allows for synchronized iteration over multiple channels, similar to Python's ``zip``. 
This is particularly useful when working with multi-channel images (e.g., RGB), allowing direct access to corresponding pixels 
in each channel without manual chunk or pixel index management.

For best results, ensure that the zipped channels:

- Have the same number of chunks.
- Use identical chunk sizes and layouts.

Example usage:

.. code-block:: cpp

    compressed::image<T> image = ...;

    // Get references to the R, G, and B channels by index
    auto [r, g, b] = image.channels(0, 1, 2);

    // Iterate over all chunks simultaneously
    for (auto [r_chunk, g_chunk, b_chunk] : compressed::ranges::zip(r, g, b))
    {
        // Iterate over pixels in each chunk simultaneously
        for (auto [r_pixel, g_pixel, b_pixel] : compressed::ranges::zip(r_chunk, g_chunk, b_chunk))
        {
            // Modify each pixel, e.g., grayscale average
            auto gray = static_cast<T>((r_pixel + g_pixel + b_pixel) / 3);
            r_pixel = g_pixel = b_pixel = gray;
        }
    }


.. doxygenstruct:: compressed::ranges::zip
    :members:
    :undoc-members: