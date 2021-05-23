import os
import subprocess

from setuptools import setup, Extension
from setuptools import find_packages
from setuptools.command.build_ext import build_ext


class CMakeExtension(Extension):  # type: ignore
    def __init__(self, name: str, sourcedir: str = "") -> None:
        Extension.__init__(self, name, sources=[])
        self.sourcedir = os.path.abspath(sourcedir)


class CMakeBuild(build_ext):  # type: ignore
    def run(self) -> None:
        try:
            subprocess.check_output(["cmake", "--version"])
        except OSError:
            raise RuntimeError(
                "CMake must be installed to build the following extensions: "
                + ", ".join(e.name for e in self.extensions)
            )

        for ext in self.extensions:
            self.build_extension(ext)

    def build_extension(self, ext: CMakeExtension) -> None:
        cfg = "Release"
        self.build_dir = "build"

        print(f"Extension: {ext.name}")
        print("CMake Build Type:", cfg)

        extdir = os.path.abspath(os.path.dirname(self.get_ext_fullpath(ext.name)))

        cmake_args = ["-DCMAKE_BUILD_TYPE=" + cfg]

        cmake_args += ["-DCMAKE_LIBRARY_OUTPUT_DIRECTORY=" + os.path.join("build", extdir)]

        build_args = ["--target", ext.name]
        build_args += ["--", "-j16"]

        env = os.environ.copy()
        env["CXXFLAGS"] = '{} -DVERSION_INFO=\\"{}\\"'.format(env.get("CXXFLAGS", ""), self.distribution.get_version())
        if not os.path.exists(self.build_dir):
            os.makedirs(self.build_dir)
        subprocess.check_call(["cmake", ext.sourcedir] + cmake_args, cwd=self.build_dir, env=env)
        subprocess.check_call(["cmake", "--build", "."] + build_args, cwd=self.build_dir)


ext_modules = [CMakeExtension("ecs_cpp")]
setup(
    name="ecs",
    version="0.0.0",
    description="Entity Component System Library.",
    packages=find_packages(),
    ext_modules=ext_modules,
    cmdclass=dict(build_ext=CMakeBuild),
)
