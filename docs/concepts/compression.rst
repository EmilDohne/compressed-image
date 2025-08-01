..
  Copyright Contributors to the compressed-image project.

.. _in_mem_compression:

in-memory compression
#######################

The ``compressed-image`` project uses lossless in-memory compression to make handling large amounts of data more manageable
both in memory-constrained environments and also in systems with abundant memory.

It does this by using fast, lossless compression codecs to achieve moderate compression ratios while not compromising on
performance. In some cases even outperforming uncompressed operations due to limiting memory allocation (see :ref:`benchmarks`
for more information).

The compressed data is stored as a 3-dimensional ndarray that descends as follows

- channel
- chunk
- block

where a ``block`` is the unit being compressed, this is typically quite small, by default set to 32KB. A ``chunk`` is the
highest level that is transparent to the user, this is the container that you as a user would set. While a channel contains
all the ``chunks`` for a single channel.

This compression/decompression logic is handled by `c-blosc2 <https://github.com/Blosc/c-blosc2>`_.


Compression codecs
*******************

The ``compressed-image`` api currently exposes the following compression codecs (with more to potentially follow in the 
future):

- ``blosclz``
	- Custom flavor of lz (Lempel-Ziv) based on FastLZ.
- ``lz4`` 
	- LZ4 is a compression algorithm optimized for encoding/decoding speed with moderate compression ratios. This is the
	  default used by the ``compressed-image`` api (but that is user configurable)
- ``lz4hc``
	- This compression codec is an extension of ``lz4`` trying to squeeze more compression out of the data at a lower compression
	  speed but matching ``lz4`` in decompression speed.
- ``zstd``
	- Unlike the other codecs mentioned here, zstd optimizes for compression ratio, trading off speed. This should be used
	  when in a heavily memory-constrained environment where performance is less important. You can expect
	  ~5-10x compression ratios at low-moderate compression/decompression speed.

Blocks and Chunks
*******************

While going through the ``compressed::image`` and ``compressed::channel`` methods you will often see a mention of **blocks** 
and **chunks**.

As hinted above, these are small sub-buffers in the larger compressed channel. Only the chunks are exposed while the blocks
are an internal implementation detail allowing for parallelization over blocks within chunks.

Typically, you will not have to tweak either the block or chunk size as it has been tweaked to work well across a variety 
of images. But as always, measure, don't rely on intuition!

Lossy Compression
*******************

The ``compressed-image`` api does not currently support lossy compression as the goal is to provide a 1-to-1 interface 
between the uncompressed and compressed data, allowing for frequent de- and re-compression.

If you feel like a compression codec is missing, please don't hesitate to create an issue on the github page for it.