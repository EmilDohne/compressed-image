..
  Copyright Contributors to the compressed-image project.

.. _building:

building
#######################

Despite being header-only, the ``compressed-image`` library is frequently built and tested as part of the github CI.

The benchmarks, tests and examples compile using the typical cmake commands (``cmake -S .``; ``cmake --build build``) as long
as these are enabled with :ref:`configurable_options`.

dependencies
*************

- `c-blosc2 <https://github.com/Blosc/c-blosc2>`_ >= 2.17.0 for compression/decompression
	- included as git submodule, built automatically
- `OpenImageIO <https://github.com/AcademySoftwareFoundation/OpenImageIO>`_ [optional, required for py bindings] >= 2.5.16.0 for reading files from disk, if compiled without this the read functions are unavailable
	- either built via ``vcpkg`` or has to be provided during configure via a system library.
- `nlohnamm json <https://github.com/nlohmann/json>`_ >= 3.11.2 for metadata storage
	- included as git submodule, built automatically
- `pybind11 <https://github.com/pybind/pybind11>`_ [optional, required for py bindings] >= 2.11.0 for building and binding to python
	- included as git submodule, built automatically
- `pybind11_image_util <https://github.com/EmilDohne/pybind11_image_util>`_ [optional, required for py bindings] ``HEAD`` for binding between ``numpy.ndarray`` and ``std::vector`` without the boilerplate
	- included as git submodule, built automatically
- `pybind11_json <https://github.com/pybind/pybind11_json>`_ [optional, required for py bindings] >= 0.2.4 for binding the nlohmann json library to python
	- included as git submodule, built automatically
- `doctest <https://github.com/doctest/doctest>`_ [optional] >= 2.4.11 for generating the test suite
	- included as git submodule, built automatically
- `google benchmark <https://github.com/google/benchmark>`_ [optional] > 1.9.1 for generating the benchmark suite
	- included as git submodule, built automatically

.. note:: 

	If you wish to simply 'fire and forget' the build without handling any of these dependencies, enable the ``COMPRESSED_IMAGE_USE_VCPKG``
	cmake flag which will automatically build the dependencies for you.

compiler flags
***************

``compressed-image`` supports being built under extended warnings/error levels without issues, here are the flags supported across the different
compilers:

gcc
====

- ``-Wall``
- ``-Werror``
- ``-Wextra``

msvc
=====

- ``/W4``
- ``/WX``
- ``/w44062`` - enum is not handled
- ``/w44464`` - relative include path includes '..'
- ``/w45264`` - ``const`` variable is not used.

clang
======

- ``-Wall``
- ``-Werror``
- ``-Wextra``

If you notice builds failing with any of these compiler flags please report it as `an issue <https://github.com/EmilDohne/compressed-image/issues>`_.

.. _configurable_options:

configurable options
*********************

The ``compressed-image`` api comes with a couple of configurable options for enabling/disabling certain features.

- ``COMPRESSED_DETERMINE_MAIN_PROJECT``- Default ON - A utility flag that enables the use of cmake to determine if we are currently compiling as the main project, automatically enables other flags.
- ``COMPRESSED_IMAGE_USE_VCPKG`` - Default OFF -  Whether to use vcpkg for installing non-submodule thirdparty dependencies (OpenImageIO)
- ``COMPRESSED_IMAGE_BUILD_TESTS`` - Default OFF - Whether to build the test suite
- ``COMPRESSED_IMAGE_BUILD_EXAMPLES``- Default OFF -  Whether to build the examples
- ``COMPRESSED_IMAGE_BUILD_PYTHON`` - Default OFF -  Whether to build the python bindings
- ``COMPRESSED_IMAGE_BUILD_DOCS`` - Default OFF -  Whether to build the documentation locally, requires the python bindings.
- ``COMPRESSED_IMAGE_BUILD_BENCHMARKS`` - Default OFF -  Whether to build the benchmarks
- ``COMPRESSED_IMAGE_EXTENDED_WARNINGS`` - Default OFF -  Whether to enable the extended warnings as shown above as `INTERFACE` options, this should only be enabled when your project also compiles under these flags.