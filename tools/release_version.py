"""Read the application and package version from CMake metadata."""
import re
from pathlib import Path

def release_version():
    text = (Path(__file__).resolve().parents[1] / 'CMakeLists.txt').read_text('utf-8')
    base = re.search(r'project\(ShaderixFrame VERSION ([\d.]+)', text).group(1)
    suffix = re.search(r'set\(SF_VERSION_SUFFIX "([^"]*)"\)', text)
    return base + ('-' + suffix[1] if suffix and suffix[1] else '')
