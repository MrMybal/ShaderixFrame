"""Compatibility entry point for the .CyDocBuild documentation toolchain."""
import runpy
from pathlib import Path
def main():
    runpy.run_path(str(Path(__file__).resolve().parents[1] / ".CyDocBuild/build.py"), run_name="__main__")

if __name__ == "__main__":
    main()
