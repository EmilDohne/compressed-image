import os
import gc

import pytest

from typing import Tuple

import numpy as np
import numpy.typing as npt

import compressed_image as compressed

# The base dir to the cpp test images, so we don't have to copy them over to our python test suite.
_BASE_IMAGE_PATH_ABS = os.path.abspath("../../test/images")
_BASE_IMAGE_PATH_ABS = os.path.abspath("test/images")


class TestCompressedImage:

    def test_modify_metadata(self):
        image = compressed.Image()
        metadata = {"my_key": "my_val"}
        image.set_metadata(metadata)

        print(image.get_metadata())
        assert image.get_metadata() == {"my_key": "my_val"}

    def test_modify_channelnames(self):
        img_path = os.path.join(_BASE_IMAGE_PATH_ABS, "multilayer_1920x1080.exr")
        image = compressed.Image.read(np.float16, img_path, subimage = 0, channel_names = ["R", "G", "B", "A"])

        with pytest.raises(ValueError):
            # Too many elements
            image.set_channel_names(["1", "2", "3", "4", "5"])
        with pytest.raises(TypeError):
            # Incorrect type
            image.set_channel_names([1, 2, 3, 4])

        # modify in place
        chnames = image.get_channel_names()
        chnames = ["red", "G", "B", "A"]
        image.set_channel_names(chnames)
        assert image.get_channel_names() == chnames


# Parametrize over all supported dtypes by compressed.Image
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
class TestCompressedImageParametrized:

    def test_read_overloads(self, dtype: npt.DTypeLike):

        # This image is a single subimage but with a total of 23 channels
        img_path = os.path.join(_BASE_IMAGE_PATH_ABS, "multilayer_2560x1440.exr")

        # Read all channels from subimage 0
        image_all_channels = compressed.Image.read(dtype, img_path, subimage = 0)

        # Read only R, G, B, A channels from subimage 0
        image_rgba_indices = compressed.Image.read(dtype, img_path, subimage = 0, channel_indices = [0, 1, 2, 3])

        # Read only R, G, B, A channels from subimage 0 but now by name
        image_rgba_names = compressed.Image.read(dtype, img_path, subimage = 0, channel_names = ["R", "G", "B", "A"])

        assert np.array_equal(image_all_channels[0].get_decompressed(), image_rgba_indices[0].get_decompressed())
        assert np.array_equal(image_all_channels[1].get_decompressed(), image_rgba_indices[1].get_decompressed())
        assert np.array_equal(image_all_channels[2].get_decompressed(), image_rgba_indices[2].get_decompressed())
        assert np.array_equal(image_all_channels[3].get_decompressed(), image_rgba_indices[3].get_decompressed())

        assert np.array_equal(image_all_channels[0].get_decompressed(), image_rgba_names[0].get_decompressed())
        assert np.array_equal(image_all_channels[1].get_decompressed(), image_rgba_names[1].get_decompressed())
        assert np.array_equal(image_all_channels[2].get_decompressed(), image_rgba_names[2].get_decompressed())
        assert np.array_equal(image_all_channels[3].get_decompressed(), image_rgba_names[3].get_decompressed())

        assert len(image_all_channels) == 23
        assert len(image_rgba_indices) == 4
        assert len(image_rgba_names) == 4

        assert image_all_channels.shape == (23, 1440, 2560)
        assert image_rgba_indices.shape == (4, 1440, 2560)
        assert image_rgba_names.shape == (4, 1440, 2560)

        assert image_rgba_indices.get_channel_names() == ["R", "G", "B", "A"]
        assert image_rgba_names.get_channel_names() == ["R", "G", "B", "A"]

    def test_aliasing_pointer_lifetime(self, dtype: npt.DTypeLike):
        # This is something the average python dev will not have to worry about
        # but because when we construct our dynamic_channel from our dynamic_image
        # we create a shared_ptr pointing to a compressed::channel<T> but with
        # its lifetime bound to the dynamic_image.
        # This test asserts that we can construct an image with 
        
        def get_channel(path: str) -> compressed.Channel:
            # Read all channels from subimage 0
            img = compressed.Image.read(dtype, path, subimage = 0)
            return img[0]

        # This image is a single subimage but with a total of 23 channels
        img_path = os.path.join(_BASE_IMAGE_PATH_ABS, "multilayer_2560x1440.exr")
        channel = get_channel(path=img_path)

        # This should not clean up the image as we still hold an implicit reference to it.
        gc.collect()

        img_data = channel.get_decompressed()
        assert channel.shape == (1440, 2560)
      
    def test_chunk_size_guarantee(self, dtype: npt.DTypeLike):

        img_path = os.path.join(_BASE_IMAGE_PATH_ABS, "multilayer_1920x1080.exr")
        image = compressed.Image.read(dtype, img_path, subimage = 0)

        image_chunk_size = image.chunk_size()

        for channel in image.channels():
            assert channel.chunk_size() == image_chunk_size

    def test_channel_num(self, dtype: npt.DTypeLike):
        img_path = os.path.join(_BASE_IMAGE_PATH_ABS, "multilayer_1920x1080.exr")
        image = compressed.Image.read(np.float16, img_path, subimage = 0, channel_names = ["R", "G", "B", "A"])

        assert image.num_channels == 4
        assert image.get_channel_index("R") == 0
        assert image.get_channel_index("G") == 1
        assert image.get_channel_index("B") == 2
        assert image.get_channel_index("A") == 3
        assert len(image) == 4

    @pytest.mark.parametrize("width, height", [
        (64, 64),
        (123, 456),
        (2048, 16),
        (1920, 1080),
        (4096, 4096),
    ])
    def test_add_channel(self, dtype: npt.DTypeLike, width: int, height: int):
        image = compressed.Image(dtype, [], width, height)

        for i in range(4):
            image.add_channel(np.full((height, width), 90, dtype), width, height)

        assert image.num_channels == 4
        assert np.all(image[0].get_decompressed() == 90)
        assert np.all(image[1].get_decompressed() == 90)
        assert np.all(image[2].get_decompressed() == 90)
        assert np.all(image[3].get_decompressed() == 90)

    def test_add_channel_named(self, dtype: npt.DTypeLike):
        image = compressed.Image(dtype, [], 64, 64)

        image.add_channel(np.full((64, 64), 90, dtype), 64, 64, "R")
        image.add_channel(np.full((64, 64), 90, dtype), 64, 64, "G")
        image.add_channel(np.full((64, 64), 90, dtype), 64, 64, "B")
        image.add_channel(np.full((64, 64), 90, dtype), 64, 64, "A")

        assert image.num_channels == 4
        assert image.get_channel_names() == ["R", "G", "B", "A"]
        assert np.all(image[0].get_decompressed() == 90)
        assert np.all(image[1].get_decompressed() == 90)
        assert np.all(image[2].get_decompressed() == 90)
        assert np.all(image[3].get_decompressed() == 90)

    def test_add_channel_invalid_dtype(self, dtype: npt.DTypeLike):
        image = compressed.Image(dtype, [], 64, 64)

        # We don't and wont support np.bool as its not a typical image format.
        with pytest.raises(ValueError):
            image.add_channel(np.full((64, 64), 90, np.bool), 64, 64)

    def test_add_channel_invalid_dimensions(self, dtype: npt.DTypeLike):
        image = compressed.Image(dtype, [], 64, 64)

        # Invalid passed width
        with pytest.raises(ValueError):
            image.add_channel(np.full((64, 64), 90, np.bool), 32, 64)
        # Invalid passed height
        with pytest.raises(ValueError):
            image.add_channel(np.full((64, 64), 90, np.bool), 64, 32)

        # Invalid np array width
        with pytest.raises(ValueError):
            image.add_channel(np.full((64, 32), 90, np.bool), 64, 64)
        # Invalid np array height
        with pytest.raises(ValueError):
            image.add_channel(np.full((32, 64), 90, np.bool), 64, 64)