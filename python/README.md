# Python bindings

This readme will cover the basic steps for building the python bindings, executing the tests and building the wheels that are distributed from scratch.

This documentation is aimed at developers trying to modify, extend or fix the python bindings. Users trying to use them should install these via pip instead.
`py -m pip install compressed_image`.

## Building the python bindings

The python bindings are built by CMake using pybind11. To enable building of the python bindings make sure to specify the `-DCOMPRESSED_IMAGE_BUILD_PYTHON=ON` flag. 

If you additionally want to build the dependencies (openimageio) you should also specify the `-DCOMPRESSED_IMAGE_USE_VCPKG=ON` flag.

### Build steps

> [!NOTE]
> As these steps are cmake based they will work cross-platform.

`cd <dir/to/compressed-image>`

`cmake -DCMAKE_BUILD_TYPE=Release -DCOMPRESSED_IMAGE_BUILD_PYTHON=ON -DCOMPRESSED_IMAGE_USE_VCPKG=ON -B build -S .`

`cmake --build build`

You should now be left with a folder tree roughly as follows:

```
compressed-image/
    build/
        python/
            ...
            compressed_image.cpxxx-os_xxxxx.pyd/so/dylib
```

## Testing the python bindings

Testing of the python bindings is done via the [pytest](https://pypi.org/project/pytest/) library, if you followed the build steps above you should have a `compressed_image.cpxxx-xxx_xxx.pyd/so/dylib` in the build directory. To then test these you should go to the `python/test` directory and execute pytest:

- `cd <dir/to/compressed-image>/python/test`
- `pytest`

This will now run the test suite and you should be good to go!

Whenever you change something you may either need to run the build commands above (if you changed the cpp source code) or just rerun pytest if you only changed the test suite. Good luck! 

### Dynamic path mapping

The test suite dynamically maps the build directory into the python test suite via the [include_build_path](./test/include_build_path.py) file. All this does is dynamically appends the build directory to the `PYTHONPATH` allowing us to import from this.

This is admittedly not the best solution for this but will work in most cases, if you build to a different directory than the one described above you may extend this list of paths to check for.

## Building the wheel

Sometimes it is helpful to build the wheel locally when either trying to see how the package would be installed for users.

For building wheels the process is quite similar to the build but even easier!

`cd <dir/to/compressed-image>`

`py -m pip install cibuildhweel` (if not already done)

`cibuildwheel`

Now you should see a build being kicked off in the background, this will likely take some time especially since the dependencies need to be downloaded and built, this could take up to 10-15 minutes so please stand by!

Once the build is done you should have a `wheelhouse` folder that was created in the root of the project. In there there should be a `compressed_image-x.x.x-cpxxx-cpxxx-xxx_xxxxx.whl` which is your wheel (the x's denote variables)

> [!NOTE]
> You may notice this kicking off the wheel building for all supported Python versions. To limit this and build for a specific version, you can set the CIBW_BUILD environment variable:
> 
> Windows (PowerShell):
> - $env:CIBW_BUILD = "cp312-*"
>
> macOS / Linux (bash/zsh):
> - export CIBW_BUILD="cp312-*"
>
> Where `cp312` would be replaced by the version you wish to build for such as `cp313`

## Building the sdist

Similar to the wheel building, it can be very handy to build the sdist as well (source distribution) to see what users would receive if they build from scratch.

`cd <dir/to/compressed-image>`

`py -m pip install build` (if not already done)

`py -m build . --sdist`

Once the build is done you should have a `dist` folder that was created in the root of the project. In there there should be a `compressed_image-x.x.x.tar.gz` which is your source dist (the x's denote variables)
