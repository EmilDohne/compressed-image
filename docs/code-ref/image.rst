..
  Copyright Contributors to the compressed-image project.

.. _compressed_image:

image
########

The ``compressed::image<T>`` is a container of multiple :ref:`channel_struct`, it maps these channels by channel name
and can additionally store arbitrary image metadata. It also supports reading directly into the ``compressed::channel<T>``
for very memory efficient (and fast) file loading.



Reading Images
***************


Using postprocesses
===================


A note on color spaces
***********************

The ``compressed::image<T>`` struct (and the ``compressed::channel<T>`` struct) have no notion of color spaces, color
management etc. This is by design as it is meant to simply be a wrapper around a compressed buffer with image-specific
utilities, but not a full on image processing library. 

If you wish to encode color information on the ``compressed::image<T>`` you should do this via the ``metadata``.

For example:

.. code-block:: cpp

    compressed::image<T> image = ...;
    auto& metadata = image.metadata();
    
    // Set the source and destination space metadata, this is not managed by the compressed-image API, it also has no
    // special meaning and any metadata is ignored entirely during parsing
    metadata["OCIO_source_space"] = "linear";
    metadata["OCIO_destination_space"] = "sRGB";


Image Struct 
**************

.. tab:: c++


    .. doxygenstruct:: compressed::image
            :members:
            :undoc-members:

.. tab:: python

    tbd