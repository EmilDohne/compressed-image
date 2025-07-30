import pytest

import numpy as np
import numpy.typing as npt

import compressed_image as compressed

class TestCompressedChannel:
    
    def test_invalid_dtype(self):
        with pytest.raises(ValueError):
            compressed.Channel.zeros(np.bool, 1, 1)
        with pytest.raises(ValueError):
            compressed.Channel.full(np.bool, 100, 1, 1)
        with pytest.raises(ValueError):
            array = np.array((1, 1), np.bool)
            compressed.Channel(array, 1, 1)

# Parametrize over all supported dtypes by compressed.Channel as well as a
# variety of shapes to ensure we cover edge cases.
@pytest.mark.parametrize("width, height", [
    (64, 64),
    (123, 456),
    (2048, 16),
    (1920, 1080),
    (4096, 4096),
])
@pytest.mark.parametrize("dtype", 
    [
        np.uint8, 
        np.int8, 
        np.uint16, 
        np.int16, 
        np.uint32, 
        np.int32,
        np.float16,
        np.float32
    ]
)
class TestCompressedChannelParametrized:

    def test_initialization(self, width: int, height: int, dtype: npt.DTypeLike):
        arr = np.ndarray((height, width), dtype)
        channel = compressed.Channel(
            arr,
            width,
            height,
        )

    def test_initialization_with_small_chunksize(self, width: int, height: int, dtype: npt.DTypeLike):
        arr = np.ndarray((height, width), dtype)
        channel = compressed.Channel(
            arr,
            width,
            height,
            chunk_size = width * np.dtype(dtype).itemsize,
            compression_codec = compressed.Codec.blosclz,
            compression_level = 2,
        )

        assert channel.num_chunks() == height
        assert channel.chunk_size() == width * np.dtype(dtype).itemsize
        assert channel.height == height
        assert channel.width == width
        assert channel.shape == (height, width)

        assert channel.uncompressed_size() == width * height
        assert channel.compression() == compressed.Codec.blosclz
        assert channel.compression_level() == 2

    def test_full(self, width: int, height: int, dtype: npt.DTypeLike):
        channel = compressed.Channel.full(
            dtype,
            25,
            width,
            height,
            compression_codec = compressed.Codec.blosclz,
            compression_level = 2,
        )

        assert channel.height == height
        assert channel.width == width
        assert channel.shape == (height, width)

        assert channel.uncompressed_size() == width * height
        assert channel.compression() == compressed.Codec.blosclz
        assert channel.compression_level() == 2
        
        decompressed = channel.get_decompressed()
        assert decompressed.shape == channel.shape
        # Even though we compare floating point values, as we set these values
        # directly these should still be identical, no need to check for 
        # np.allclose
        assert np.array_equal(decompressed, np.full((height, width), 25, dtype))

    def test_zeros(self, width: int, height: int, dtype: npt.DTypeLike):
        channel = compressed.Channel.zeros(
            dtype,
            width,
            height,
            compression_codec = compressed.Codec.blosclz,
            compression_level = 2,
        )

        assert channel.height == height
        assert channel.width == width
        assert channel.shape == (height, width)

        assert channel.uncompressed_size() == width * height
        assert channel.compression() == compressed.Codec.blosclz
        assert channel.compression_level() == 2
        
        decompressed = channel.get_decompressed()
        assert decompressed.shape == channel.shape
        # Even though we compare floating point values, as we set these values
        # directly these should still be identical, no need to check for 
        # np.allclose
        assert np.array_equal(decompressed, np.full((height, width), 0, dtype))

    def test_full_like(self, width: int, height: int, dtype: npt.DTypeLike):
        arr = np.ndarray((height, width), dtype)
        channel_compare = compressed.Channel(
            arr,
            width,
            height,
        )

        channel = compressed.Channel.full_like(
            channel_compare,
            25
        )

        assert channel.height == channel_compare.height
        assert channel.width == channel_compare.width
        assert channel.shape == channel_compare.shape

        assert channel.uncompressed_size() == channel_compare.uncompressed_size()
        assert channel.compression() == channel_compare.compression()
        assert channel.compression_level() == channel_compare.compression_level()
        
        decompressed = channel.get_decompressed()
        assert decompressed.shape == channel.shape
        # Even though we compare floating point values, as we set these values
        # directly these should still be identical, no need to check for 
        # np.allclose
        assert np.array_equal(decompressed, np.full((height, width), 25, dtype))

    def test_zeros_like(self, width: int, height: int, dtype: npt.DTypeLike):
        arr = np.ndarray((height, width), dtype)
        channel_compare = compressed.Channel(
            arr,
            width,
            height,
        )

        channel = compressed.Channel.zeros_like(channel_compare)

        assert channel.height == channel_compare.height
        assert channel.width == channel_compare.width
        assert channel.shape == channel_compare.shape

        assert channel.uncompressed_size() == channel_compare.uncompressed_size()
        assert channel.compression() == channel_compare.compression()
        assert channel.compression_level() == channel_compare.compression_level()
        
        decompressed = channel.get_decompressed()
        assert decompressed.shape == channel.shape
        # Even though we compare floating point values, as we set these values
        # directly these should still be identical, no need to check for 
        # np.allclose
        assert np.array_equal(decompressed, np.full((height, width), 0, dtype))

    def test_modify_chunk(self, width: int, height: int, dtype: npt.DTypeLike):
        arr = np.zeros((height, width), dtype)
        channel = compressed.Channel(
            arr,
            width,
            height,
            chunk_size = width * np.dtype(dtype).itemsize,
        )

        chunk_0 = channel.get_chunk(0)
        assert channel.chunk_size() == width * np.dtype(dtype).itemsize
        assert chunk_0.dtype == dtype
        assert chunk_0.shape == (width,)
        assert np.array_equal(chunk_0, np.zeros_like(chunk_0, dtype))

        channel.set_chunk(0, np.full_like(chunk_0, 100, dtype))

        assert np.array_equal(channel.get_chunk(0), np.full_like(chunk_0, 100, dtype))

    def test_set_invalid_chunk_size(self, width: int, height: int, dtype: npt.DTypeLike):
        # Test that we can correctly handle invalid arguments being passed to the set_chunk
        # function
        arr = np.zeros((height, width), dtype)
        channel = compressed.Channel(
            arr,
            width,
            height,
            chunk_size = width * np.dtype(dtype).itemsize,
        )

        # Invalid shape dimensions but correct number of elements
        with pytest.raises(ValueError):
            channel.set_chunk(0, np.zeros((width, 1), dtype))

        # Incorrect number of elements
        with pytest.raises(ValueError):
            channel.set_chunk(0, np.zeros((width + 20), dtype))

        # Invalid chunk index
        with pytest.raises(IndexError):
            channel.set_chunk(height + 100, np.zeros((width), dtype))

    def test_iter_over_channel(self, width: int, height: int, dtype: npt.DTypeLike):
        arr = np.zeros((height, width), dtype)
        channel = compressed.Channel(
            arr,
            width,
            height,
        )
        
        buffer = np.ndarray((channel.chunk_elems(),), dtype= channel.dtype)
        for i in range(channel.num_chunks() - 1):
            channel.get_chunk(i, buffer)
            buffer[:] = i
            channel.set_chunk(i, buffer)

        final_buffer = channel.get_chunk(channel.num_chunks() -1)
        final_buffer[:] = channel.num_chunks() - 1
        channel.set_chunk(channel.num_chunks() -1, final_buffer)


        # check that it is all correct
        for i in range(channel.num_chunks()):
            chunk = channel.get_chunk(i)

            assert np.all(chunk == i)