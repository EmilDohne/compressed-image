"""
Testing utility to automatically include the local build paths as include dirs in PATH,
these are appended to the end of sys.path so system wheels or otherwise installed 
wheels would take precedence.
"""

import sys
import os

# Relative paths to the build directories, should be ordered in priority, in our case
# prefer release builds over debug builds. If you build to a different directory
# feel free to modify this. The dir should point to the containing folder of the 
# .pyd file.
_rel_paths = [
    "../../../bin-int/compressed-image/x64-release/python",
    "../../../bin-int/compressed-image/x64-debug/python",
]

for rel_path in _rel_paths:
    sys.path.append(os.path.abspath(os.path.join(__file__, rel_path)))
    print(f"Appending paths {os.path.abspath(os.path.join(__file__, rel_path))}")

# Finally, import compressed_image and show where it's coming from
import compressed_image
print(f"Imported compressed_image from {compressed_image.__file__}")