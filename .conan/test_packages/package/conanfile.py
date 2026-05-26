# === conanfile.py =====================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================
"""Module that defines a test_package test to consume conan package in conan v2 way."""

from conan import ConanFile
from conan.tools.build import cross_building
from conan.tools.cmake import CMake, CMakeToolchain


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
            # TODO(SEN-1185): enable tests when possible
            # self.run("sen run test_configs/my_package.yaml --start-stop", env="conanrun")
            # ...
