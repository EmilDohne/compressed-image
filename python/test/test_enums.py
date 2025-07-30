import unittest

import compressed_image


class TestEnums(unittest.TestCase):

    def test_enums_are_registered(self):
        # Simply check that these were built/registered correctly, no additional testing is necessary
        compressed_image.Codec.blosclz
        compressed_image.Codec.lz4
        compressed_image.Codec.lz4hc
        compressed_image.Codec.zstd