..
  Copyright Contributors to the compressed-image project.

.. _compressed_image:

image
########

The ``compressed::image<T>`` is a container of multiple :ref:`channel_struct`, it maps these channels by channel name
and can additionally store arbitrary image metadata. It also supports reading directly into the ``compressed::image<T>``
for very memory efficient (and fast) file loading (with file writes TBD).


Reading Images
***************


The ``compressed::image<T>`` class (or ``compressed.Image``) provides a convenient ``read`` function which not only abstracts
the file reads of multiple formats, but also reads the data in chunks before compressing leading to much greater performance
and memory efficiency than doing this yourself. For details visit the :ref:`benchmarks` section.

Reading files this way additionally allows for automatic translation of the image metadata into the `metadata` field of the
compressed image. 

These read functions provide a variety of overloads for reading subimages, only specific channels as well as supporting 
scanline and tiled image formats. 

.. tab:: c++

    .. code-block:: cpp

        compressed::image<T> image = compressed::image<T>::read("path/to/my/file");

.. tab:: python

    .. code-block:: python
        
        import compressed_image as compressed


        image = compressed.Image.read(np.uint8, "path/to/my/file")

We can also do more advanced things such as specifying the subimage, channels and compression settings:

.. tab:: c++

    .. code-block:: cpp
        
        // Note: this will convert to the specified dtype on-read, so it doesn't even have to be the dtype of the image!
        compressed::image<T> image = compressed::image<T>::read(
                                        "path/to/my/file" /* path */,
                                        0 /* subimage */,
                                        compressed::enums::codec::blosclz /* compression codec */,
                                        5 /* compression level */
                                    );

.. tab:: python

    .. code-block:: python
        
        import compressed_image as compressed


        image = compressed.Image.read(
                    dtype = compressed.Image.dtype_from_file("path/to/my/file"), 
                    filepath = "path/to/my/file",
                    subimage = 0,
                    channel_names = ["R", "G", "B", "A"],
                    compression_codec = compressed.enums.Codec.blosclz,
                    compression_level = 5
                )


.. _image_postprocesses:

Using postprocesses
===================

Often times it is helpful to apply a postprocess to the image data directly on read. For example for color space conversions
or to e.g. calculate statistics such as average, psnr etc. Since the image data is read first, and then compressed in chunks, we provide
a callback to the read functions that operates on one of these read chunks before it is compressed. This allows for very
efficient modification on read as it doesn't have the overhead of having to de- and re-compress the image data at a later
point.

.. note::

    At the moment only the C++ api supports postprocess callbacks, although this is intended to come in a future release.



.. code-block:: cpp

    std::filesystem::path filepath = "image.exr";
	
	// Read an image file and apply a post-process which adds 1 to the pixel value for all RGB channels (0, 1, 2).
	auto postprocess = [](size_t channel_idx, std::span<T> chunk)
		{
			if (channel_idx > 2)
			{
				return;
			}
	
			std::for_each(std::execution::par_unseq, chunk.begin(), chunk.end(), [](T& value)
			{
				value += 1;
			}
		};
	
	auto img = compressed::image::read<uint8_t>(
		filepath, 
		std::forward(postprocess),
		0, // subimage
		compressed::enums::codec::lz4, // compression_code
		5 // compression_level
	);

A note on color spaces
***********************

The ``compressed::image<T>`` struct (and the ``compressed::channel<T>`` struct) have no notion of color spaces, color
management etc. This is by design as it is meant to simply be a wrapper around a compressed buffer with image-specific
utilities, but not a full on image processing library. 

If you wish to encode color information on the ``compressed::image<T>`` you should do this via the ``metadata``.

For example:

.. tab:: c++

    .. code-block:: cpp

        compressed::image<T> image = ...;
        auto& metadata = image.metadata();
    
        // Set the source and destination space metadata, this is not managed by the compressed-image API, it also has no
        // special meaning and any metadata is ignored entirely during parsing. This would just be for your convenience
        metadata["OCIO_source_space"] = "linear";
        metadata["OCIO_destination_space"] = "sRGB";

.. tab:: python

    .. code-block:: python
        
        import compressed_image as compressed


        image: compressed.Image = ...

        // Set the source and destination space metadata, this is not managed by the compressed-image API, it also has no
        // special meaning and any metadata is ignored entirely during parsing. This would just be for your convenience
        image.set_metadata(
            {
                "OCIO_source_space": "linear",
                "OCIO_destination_space": "sRGB",
            }
        )


.. note:: 

    For performing color space transformations on read, it is recommended to do this a postprocess during the read 
    function as that is the most efficient way of doing this (rather than having to decompress the data again). See
    :ref:`image_postprocesses` for more!

Image Struct 
**************

.. tab:: c++


    .. doxygenstruct:: compressed::image
            :members:
            :undoc-members:

.. tab:: python

    .. autoclass:: compressed_image.Image
        :members:
        :inherited-members:

        .. automethod:: __init__