import os
import sys
from setuptools import setup
from pybind11.setup_helpers import Pybind11Extension, build_ext

# 1. Capture your environment variables
terark_root = os.environ.get("TERARKDBROOT", "/home/vondele/chess/noob/terarkdb")
# Ensure the compiler matches your Makefile
os.environ["CC"] = "g++"
os.environ["CXX"] = "g++"

# 2. Extract the library names from your LIBS string
# (Removing the '-l' prefix)
libraries = [
    "cdbdirect",
    "terarkdb",
    "terark-zip-r",
    "boost_fiber",
    "boost_context",
    "tbb",
    "snappy",
    "lz4",
    "z",
    "bz2",
    "atomic",
]

# Standard system libs often handled by the compiler but safe to include
system_libs = ["pthread", "rt", "dl", "gomp"]
all_libs = libraries + system_libs

ext_modules = [
    Pybind11Extension(
        "cdbdirect",
        ["binding.cpp"],
        # Corresponds to -I flags
        include_dirs=[
            #    os.path.join(terark_root, "output/include"),
            #    os.path.join(terark_root, "third-party/terark-zip/src"),
            #    os.path.join(terark_root, "include"),
            ".",
        ],
        # Corresponds to -L flags
        library_dirs=[
            ".",
            os.path.join(terark_root, "output/lib"),
        ],
        # Corresponds to -l flags
        libraries=all_libs,
        # Corresponds to CXXFLAGS
        extra_compile_args=[
            "-O3",
            "-march=native",
            "-fomit-frame-pointer",
            "-fPIC",
            "-flto=auto",
        ],
        # Matches your Makefile's LTO and specific path define
        extra_link_args=["-flto=auto"],
        define_macros=[
            (
                "CHESSDB_PATH",
                os.environ.get("CHESSDB_PATH", "/mnt/ssd/chess-20251115/data"),
            )
        ],
        cxx_std=17,  # Adjust to 11, 14, or 17 based on your project
    ),
]

setup(
    name="cdbdirect",
    version="0.1",
    ext_modules=ext_modules,
    cmdclass={"build_ext": build_ext},
)
