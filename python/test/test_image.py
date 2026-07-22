import os
import gc

import pytest

from typing import Tuple

import numpy as np
import numpy.typing as npt

import compressed_image as compressed

# The base dir to the cpp test images, so we don't have to copy them over to our python test suite.
_BASE_IMAGE_PATH_ABS = os.path.join(os.path.dirname(__file__), "../../test/images")


class TestCompressedImage:

    def test_modify_metadata(self):
        image = compressed.Image(np.uint8, [], 64, 64)
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

    def test_dtype_from_file(self):
        img_path = os.path.join(_BASE_IMAGE_PATH_ABS, "multilayer_1920x1080.exr")
        dtype = compressed.Image.dtype_from_file(img_path)

        assert dtype == np.dtype(np.float32)

    def test_write_roundtrip(self, tmp_path):
        width, height = 32, 16
        image = compressed.Image(np.float32, [], width, height)

        values = [0.25, 0.5, 0.75]
        for name, val in zip(["R", "G", "B"], values):
            image.add_channel(np.full((height, width), val, np.float32), width, height, name)

        out_path = str(tmp_path / "roundtrip.exr")
        image.write(out_path)
        assert os.path.exists(out_path)

        read_back = compressed.Image.read(np.float32, out_path, subimage=0)
        assert read_back.shape == (3, height, width)
        assert read_back.get_channel_names() == ["R", "G", "B"]
        for i, val in enumerate(values):
            assert np.allclose(read_back[i].get_decompressed(), val)

    def test_write_no_channels_raises(self, tmp_path):
        image = compressed.Image(np.uint8, [], 16, 16)
        with pytest.raises(RuntimeError):
            image.write(str(tmp_path / "empty.tif"))

    def test_read_from_memory(self):
        img_path = os.path.join(_BASE_IMAGE_PATH_ABS, "multilayer_1920x1080.exr")
        with open(img_path, "rb") as f:
            data = f.read()

        from_disk = compressed.Image.read(np.float16, img_path, subimage=0)
        from_mem = compressed.Image.read_from_memory(np.float16, data, "exr", subimage=0)

        assert from_mem.num_channels == from_disk.num_channels
        assert from_mem.shape == from_disk.shape
        for i in range(from_disk.num_channels):
            assert np.array_equal(from_mem[i].get_decompressed(), from_disk[i].get_decompressed())
        
    def test_dtypes_from_file_multi_dtype(self):
        img_path = os.path.join(_BASE_IMAGE_PATH_ABS, "multilayer_1920x1080.exr")
        dtypes = compressed.Image.dtypes_from_file(img_path)

        print(dtypes)
        assert dtypes == [
            np.dtype(np.float16), np.dtype(np.float16), np.dtype(np.float16), np.dtype(np.float16), # RGBA F16
            np.dtype(np.float16), np.dtype(np.float16), np.dtype(np.float16), # GI.RGB F16
            np.dtype(np.float16), np.dtype(np.float16), np.dtype(np.float16), # SSS.RGB F16
            np.dtype(np.float16), np.dtype(np.float16), np.dtype(np.float16), # atmosphere.RGB F16
            np.dtype(np.float16), np.dtype(np.float16), np.dtype(np.float16), # background.RGB F16
            np.dtype(np.float16), np.dtype(np.float16), np.dtype(np.float16), # caustics.RGB F16
            np.dtype(np.float16), np.dtype(np.float16), np.dtype(np.float16), np.dtype(np.float16), # cryptomatte.RGBA F16
            np.dtype(np.float32), np.dtype(np.float32), np.dtype(np.float32), np.dtype(np.float32), # cryptomatte00.RGBA F32
            np.dtype(np.float32), np.dtype(np.float32), np.dtype(np.float32), np.dtype(np.float32), # cryptomatte01.RGBA F32
            np.dtype(np.float32), np.dtype(np.float32), np.dtype(np.float32), np.dtype(np.float32), # cryptomatte02.RGBA F32
            np.dtype(np.float16), np.dtype(np.float16), np.dtype(np.float16), # lighting.RGB F16
            np.dtype(np.float16), np.dtype(np.float16), np.dtype(np.float16), # reflect.RGB F16
            np.dtype(np.float16), np.dtype(np.float16), np.dtype(np.float16), # refract.RGB F16
            np.dtype(np.float16), np.dtype(np.float16), np.dtype(np.float16), # selfIllum.RGB F16
            np.dtype(np.float16), np.dtype(np.float16), np.dtype(np.float16), # specular.RGB F16          
        ]


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

    def test_remove_channel(self, dtype: npt.DTypeLike):
        img_path = os.path.join(_BASE_IMAGE_PATH_ABS, "multilayer_1920x1080.exr")
        image = compressed.Image.read(np.float16, img_path, subimage = 0, channel_names = ["R", "G", "B", "A"])

        image.remove_channel("R")
        # Since the indices shift this is now channel B
        image.remove_channel(1)

        assert len(image) == 2
        assert image.get_channel_names() == ["G", "A"]
        assert image.num_channels == 2

    def test_dtype_property(self, dtype: npt.DTypeLike):
        image = compressed.Image(dtype, [], 64, 64)
        image.add_channel(np.full((64, 64), 3, dtype), 64, 64)

        assert image.dtype == np.dtype(dtype)
        # The channel dtype must agree with its containing image.
        assert image[0].dtype == np.dtype(dtype)

    def test_channels_subset(self, dtype: npt.DTypeLike):
        img_path = os.path.join(_BASE_IMAGE_PATH_ABS, "multilayer_1920x1080.exr")
        image = compressed.Image.read(dtype, img_path, subimage = 0, channel_names = ["R", "G", "B", "A"])

        by_index = image.channels([2, 0])
        by_name = image.channels(["B", "R"])

        assert len(by_index) == 2
        assert len(by_name) == 2
        # Subset selection preserves the requested order.
        assert np.array_equal(by_index[0].get_decompressed(), by_name[0].get_decompressed())
        assert np.array_equal(by_index[1].get_decompressed(), by_name[1].get_decompressed())
        # And matches the corresponding full-image channels.
        assert np.array_equal(by_index[0].get_decompressed(), image[2].get_decompressed())
        assert np.array_equal(by_index[1].get_decompressed(), image[0].get_decompressed())

        with pytest.raises(IndexError):
            image.channels([99])

    def test_extract_channel(self, dtype: npt.DTypeLike):
        image = compressed.Image(dtype, [], 64, 64)
        for name in ("R", "G", "B"):
            image.add_channel(np.full((64, 64), 7, dtype), 64, 64, name)

        extracted = image.extract_channel("G")

        # The channel is detached from the image but remains usable on its own.
        assert image.num_channels == 2
        assert image.get_channel_names() == ["R", "B"]
        assert extracted.shape == (64, 64)
        assert np.all(extracted.get_decompressed() == 7)

    def test_iteration(self, dtype: npt.DTypeLike):
        image = compressed.Image(dtype, [], 64, 64)
        for _ in range(3):
            image.add_channel(np.full((64, 64), 5, dtype), 64, 64)

        iterated = list(image)
        assert len(iterated) == 3
        for channel in iterated:
            assert channel.shape == (64, 64)

    def test_repr(self, dtype: npt.DTypeLike):
        image = compressed.Image(dtype, [], 64, 64)
        image.add_channel(np.full((64, 64), 1, dtype), 64, 64)

        assert "compressed_image.Image" in repr(image)
        assert "compressed_image.Channel" in repr(image[0])

    def test_channel_copy(self, dtype: npt.DTypeLike):
        image = compressed.Image(dtype, [], 32, 16)
        image.add_channel(np.full((16, 32), 4, dtype), 32, 16, "A")

        original = image[0]
        duplicate = original.copy()

        assert duplicate.dtype == original.dtype
        assert duplicate.shape == original.shape
        assert np.array_equal(duplicate.get_decompressed(), original.get_decompressed())

    def test_add_channel_from_channel_copies(self, dtype: npt.DTypeLike):
        src = compressed.Image(dtype, [], 32, 16)
        src.add_channel(np.full((16, 32), 6, dtype), 32, 16, "R")

        dst = compressed.Image(dtype, [], 32, 16)
        dst.add_channel(src.channel("R"), name="R_copied")

        assert dst.num_channels == 1
        assert dst.get_channel_names() == ["R_copied"]
        assert np.array_equal(dst[0].get_decompressed(), src[0].get_decompressed())
        # The source is untouched by the copy.
        assert np.all(src[0].get_decompressed() == 6)

    def test_move_channel_between_images(self, dtype: npt.DTypeLike):
        src = compressed.Image(dtype, [], 32, 16)
        src.add_channel(np.full((16, 32), 8, dtype), 32, 16, "R")
        src.add_channel(np.full((16, 32), 9, dtype), 32, 16, "G")

        dst = compressed.Image(dtype, [], 32, 16)
        dst.add_channel(src.extract_channel("R"), name="R")

        assert src.num_channels == 1
        assert src.get_channel_names() == ["G"]
        assert dst.num_channels == 1
        assert np.all(dst[0].get_decompressed() == 8)