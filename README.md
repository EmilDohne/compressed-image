# compressed-image

``compressed-image`` is a header-only library for reading, storing and modifying images and channels as losslessly compressed in-memory buffers
allowing for very memory-efficient representations of these. This library acts as a stop-gap between storing images purely
in-memory and reading them from disk on-demand. 

It trades a bit of performance for significantly lower memory usage. For more detailed
information please have a look at the benchmark section on [readthedocs](https://compressed-image.readthedocs.io/).

It is written entirely using C++20 while also providing pre-built python binaries that are pip-installable.

## Features

- Storing images as compressed buffers
- Support for multiple compressions algorithms such as `lz4`, `zstd`, `blosclz` etc.
- Reading files from disk in compressed chunks for low memory usage
- Random access decompression without having to decompress the entire image
- Lazy image/channel representations with extremely low memory footprints
- Cross-platform support
- Interoperability with NumPy


## When should you use this?

This library is for you if you want to optimize memory usage in your application and still work 
efficiently with image data. It's especially useful when:

- You need to keep many images in memory at once but don't want to pay the full memory cost.
- You're working with high-resolution images or large datasets and want to avoid I/O bottlenecks.
- You want fast, random access to image chunks without having to decompress the entire image up front.
- You're building tools, pipelines, or applications that process images frequently but can't afford the memory footprint of uncompressed data.
- You're using Python or C++ and want seamless, cross-language support without changing your workflow too much.

If you're hitting memory limits or just want a smarter way to handle images 
in-memory without sacrificing speed, `compressed-image` was built for that.


## Performance

The `compressed-image` library is aimed at high performance computing allowing you to store many images in-memory
simultaneously while not having to pay the associated memory cost. It performs equal or faster during image reads
with significantly lower memory usage

![A graph showing the relationship of decoding speed using OpenImageIO vs compressed::image showing roughly equal performance](docs/images/bench/oiio_vs_compressed.png)

![A graph showing the relationship of memory usage during read using OpenImageIO vs compressed::image showing significantly lower usage with compressed::image](docs/images/bench/oiio_vs_compressed_mem_usage.png)


## Quickstart

This is a simple example of getting you up and running with compressed-image, loading a file from disk and then interacting
with the channels

### cpp
```cpp
#include <compress/image.h>

auto image = compressed::image<uint8_t>::read("/some/file/path");
// Retrieve the red channel from the image
auto channel_r = image.channel("R"); // could also get it via image.channel(0)

// Iterate over the chunks in the channel, this will decompress the 
// chunk and recompress it on the fly.
for (auto chunk_r : channel_r)
{
   std::for_each(std::execution::par_unseq, chunk_r.begin(), chunk_r.end(), [](auto& pixel)
      {
         // perform some computation on the chunk in parallel
      });
}

// Otherwise you can also get the image data directly as a full channel. Although this is less memory efficient
std::vector<uint8_t> channel_r_decompressed = channel_r.get_decompressed();
```

### python
```py    
import compressed_image as compressed

image = compressed.Image.read(np.float16, "/some/file/path", subimage=0)
channel_r: compressed.Channel = image[0]

# Iterate over the chunks in the channel, this is more memory efficient than decompressing all the data
# at once
for chunk_idx in range(channel_r.num_chunks()):
   chunk = channel_r.get_chunk(chunk_idx)

   # modify chunk as you wish

   # set the chunk back again, this will recompress the data
   channel_r.set_chunk(chunk, chunk_idx)

# If you instead wish to decompress the buffer as one you can do that too:
channel_r_decompressed = channel_r.get_decompressed()
```

### Documentation

The complete user-facing documentation is hosted on [readthedocs](https://compressed-image.readthedocs.io/) where you can additionally find more information on benchmarks and technical class documentation.