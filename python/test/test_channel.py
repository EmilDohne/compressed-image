import unittest
import pytest

import numpy as np
import numpy.typing as npt

import compressed_image

# Parametrize over all supported dtypes by compressed_image.Channel as well as a
# variety of shapes to ensure we cover edge cases.
@pytest.mark.parametrize("width, height", [
    (64, 64),
    (123, 456),
    (2048, 16),
    (1920, 1080),
    (4096, 4096),
])
@pytest.mark.parametrize("dtype", [np.uint8, np.int8, np.uint16, np.int16, np.uint32, np.int32, np.float32, np.float64])
class TestCompressedChannel:

    def test_initialization(self, width: int, height: int, dtype: npt.DTypeLike):
        arr = np.ndarray((height, width), dtype)
        channel = compressed_image.Channel(
            arr,
            width,
            height,
        )

    def test_initialization_with_small_chunksize(self, width: int, height: int, dtype: npt.DTypeLike):
        arr = np.ndarray((height, width), dtype)
        channel = compressed_image.Channel(
            arr,
            width,
            height,
            chunk_size = width * np.dtype(dtype).itemsize,
            compression_codec = compressed_image.Codec.blosclz,
            compression_level = 2,
        )

        assert channel.num_chunks() == height
        assert channel.chunk_size() == width * np.dtype(dtype).itemsize
        assert channel.height == height
        assert channel.width == width
        assert channel.shape == (height, width)

        assert channel.uncompressed_size() == width * height
        assert channel.compression() == compressed_image.Codec.blosclz
        assert channel.compression_level() == 2

    def test_full(self, width: int, height: int, dtype: npt.DTypeLike):
        channel = compressed_image.Channel.full(
            dtype,
            25,
            width,
            height,
            compression_codec = compressed_image.Codec.blosclz,
            compression_level = 2,
        )

        assert channel.height == height
        assert channel.width == width
        assert channel.shape == (height, width)

        assert channel.uncompressed_size() == width * height
        assert channel.compression() == compressed_image.Codec.blosclz
        assert channel.compression_level() == 2
        
        decompressed = channel.get_decompressed()
        assert decompressed.shape == channel.shape
        # Even though we compare floating point values, as we set these values
        # directly these should still be identical, no need to check for 
        # np.allclose
        assert np.array_equal(decompressed, np.full((height, width), 25, dtype))

    def test_zeros(self, width: int, height: int, dtype: npt.DTypeLike):
        channel = compressed_image.Channel.zeros(
            dtype,
            width,
            height,
            compression_codec = compressed_image.Codec.blosclz,
            compression_level = 2,
        )

        assert channel.height == height
        assert channel.width == width
        assert channel.shape == (height, width)

        assert channel.uncompressed_size() == width * height
        assert channel.compression() == compressed_image.Codec.blosclz
        assert channel.compression_level() == 2
        
        decompressed = channel.get_decompressed()
        assert decompressed.shape == channel.shape
        # Even though we compare floating point values, as we set these values
        # directly these should still be identical, no need to check for 
        # np.allclose
        assert np.array_equal(decompressed, np.full((height, width), 0, dtype))

    def test_full_like(self, width: int, height: int, dtype: npt.DTypeLike):
        arr = np.ndarray((height, width), dtype)
        channel_compare = compressed_image.Channel(
            arr,
            width,
            height,
        )

        channel = compressed_image.Channel.full_like(
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