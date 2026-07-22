import unittest

import compressed_image


class TestEnums(unittest.TestCase):

    def test_enums_are_registered(self):
        # Simply check that these were built/registered correctly, no additional testing is necessary
        compressed_image.Codec.blosclz
        compressed_image.Codec.lz4
        compressed_image.Codec.lz4hc
        compressed_image.Codec.zstd

    def test_gpu_enums_are_registered(self):
        # The nvcomp-backed GPU codecs must be selectable from python (they fall back to a CPU
        # equivalent when CUDA is unavailable).
        compressed_image.Codec.lz4_gpu
        compressed_image.Codec.snappy_gpu
        compressed_image.Codec.zstd_gpu
        compressed_image.Codec.deflate_gpu
        compressed_image.Codec.gdeflate_gpu
        compressed_image.Codec.cascaded_gpu


class TestCuda(unittest.TestCase):

    def test_cuda_introspection(self):
        # These must always be callable regardless of whether a GPU is present.
        available = compressed_image.cuda_available()
        self.assertIsInstance(available, bool)

        count = compressed_image.cuda_device_count()
        self.assertIsInstance(count, int)
        self.assertGreaterEqual(count, 0)

        names = compressed_image.cuda_device_names()
        self.assertIsInstance(names, list)

        if available:
            self.assertGreater(count, 0)
            self.assertEqual(len(names), count)
        else:
            self.assertEqual(count, 0)
            self.assertEqual(names, [])