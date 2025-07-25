..
  Copyright Contributors to the compressed-image project.

.. _testing:

testing
#######################

Testing of the ``compressed-image`` library is done via `doctest <https://github.com/doctest/doctest>`_ and can be enabled
as a build flag via ``COMPRESSED_IMAGE_BUILD_TESTS``:


``cmake -DCOMPRESSED_IMAGE_BUILD_TESTS=ON -B build -S .``

``cmake --build build``

This however assumes that a system version of OpenImageIO is present, to automatically build it as part of the configure 
step you may enable the ``COMPRESSED_IMAGE_USE_VCPKG`` flag

``cmake -DCOMPRESSED_IMAGE_BUILD_TESTS=ON -DCOMPRESSED_IMAGE_USE_VCPKG=ON -B build -S .``

``cmake --build build``

Once the build is complete, the tests can be run using ctest:

``cd build/path_to_test_folder/``

``ctest --verbose .``

Or alternatively you may execute the binary directly. You should see an output along these lines:

.. code-block:: none

	1: Test timeout computed to be: 10000000
	1: [doctest] doctest version is "2.4.11"
	1: [doctest] run with "--help" for options
	1: [doctest] Initialize channel from incorrect schunk................................................ ok
	1: [doctest] Initialize channel from incorrect span.................................................. ok
	1: [doctest] Roundtrip channel creation.............................................................. ok
	1: [doctest] Roundtrip channel creation larger than chunksize........................................ ok
	1: [doctest] Channel get attributes.................................................................. ok
	1: [doctest] Channel iterate......................................................................... ok
	1: [doctest] Get coordinates in base-chunk........................................................... ok
	1: [doctest] Get coordinates in non-base chunk....................................................... ok
	1: [doctest] Iter over chunk......................................................................... ok
	1: [doctest] Read compressed file smaller than one chunk............................................. ok
	1: [doctest] Read compressed tiled file and extract channels......................................... ok
	1: [doctest] Read compressed multipart file and extract channels..................................... ok
	1: [doctest] Read compressed file and extract channels............................................... ok
	1: [doctest] Read compressed file get attributes..................................................... ok
	1: [doctest] Read compressed file exactly than one chunk............................................. ok
	1: [doctest] Read compressed file larger than one chunk.............................................. ok
	1: [doctest] Read compressed file, subset of channel indices......................................... ok
	1: [doctest] Read compressed file, non contiguous channel indices.................................... ok
	1: [doctest] Read compressed file, non contiguous channel indices, out of order...................... ok