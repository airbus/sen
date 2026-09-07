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

            # my_package is built here, not shipped in the Sen package, and components are
            # opened by bare name -- so this build tree has to be on the loader's search path
            # or the kernel cannot find it. Both directories are listed because the build tree
            # puts shared objects in bin/ today; a package that puts them in lib/ still works.
            env = Environment()
            for output_dir in (join(self.build_folder, "bin"), join(self.build_folder, "lib")):
                env.prepend_path("LD_LIBRARY_PATH", output_dir)
                env.prepend_path("PATH", output_dir)
            with env.vars(self, scope="run").apply():
                self.run("sen run test_configs/my_package.yaml --start-stop", env="conanrun")
