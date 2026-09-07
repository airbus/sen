# === conanfile.py =====================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================
"""Module that defines a test_package test to consume conan package in conan v2 way."""

from os.path import join

from conan import ConanFile
from conan.tools.build import cross_building
from conan.tools.cmake import CMake, CMakeToolchain
from conan.tools.env import Environment


class TestPackageConan(ConanFile):
    """Conan file that specifies how a project is setup that uses Sen."""

    settings = "os", "arch", "compiler", "build_type"
    generators = "CMakeDeps", "VirtualRunEnv"
    test_type = "explicit"

    def requirements(self):
        """Defines the dependencies of Sen."""
        self.requires(self.tested_reference_str)

    def generate(self):
        """Generate the toolchain files."""
        tc = CMakeToolchain(self, generator="Ninja")
        tc.generate()

    def build(self):
        """Configure and build Sen test_package."""
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def test(self):
        """Defines a few test calls to ensure Sen works."""
        if not cross_building(self):
            self.run("sen --version", env="conanrun")

            # The point is that an installed Sen finds its own libraries, so the environment must
            # not hand the loader the answer: LD_LIBRARY_PATH names only my_package's own directory,
            # and the binary is invoked by full path so conanrun does not put the package's library
            # directory back.
            _exe = "sen.exe" if self.settings.os == "Windows" else "sen"
            sen_binary = join(self.dependencies["sen"].package_folder, "bin", _exe)
            env = Environment()
            env.define_path("LD_LIBRARY_PATH", join(self.build_folder, "lib"))

            # Windows uses PATH, and my_package's DLL is a runtime artefact in the build
            # tree's bin, so both directories are named.
            for output_dir in (join(self.build_folder, "bin"), join(self.build_folder, "lib")):
                env.prepend_path("PATH", output_dir)

            with env.vars(self, scope="run").apply():
                self.run(f'"{sen_binary}" run test_configs/my_package.yaml --start-stop')
