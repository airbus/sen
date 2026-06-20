# === conanfile.py =====================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================
"""Module to define how to setup conan packages for Sen."""

from os import getenv
from os.path import isdir, join
from pathlib import Path

from conan import ConanFile
from conan.errors import ConanInvalidConfiguration
from conan.tools.cmake import CMake, CMakeDeps, CMakeToolchain, cmake_layout
from conan.tools.files import copy
from conan.tools.scm.git import Git
from conan.tools.system.package_manager import Apt, Dnf, Yum, Zypper

COMPONENTS = tuple(sorted(p.parent.name for p in (Path(__file__).parent / "components").glob("*/CMakeLists.txt")))

# Components enabled by `mode=basic`. `mode=full` enables every COMPONENT;
# `mode=barebones` enables none.
_BASIC_COMPONENTS = frozenset({"shell", "ether"})

# Build toggles that used to be environment variables (we now complain when they are used).
_RETIRED_ENV_VARS = {
    "ENABLE_TESTS": "-DSEN_BUILD_TESTS=ON",
    "ENABLE_EXAMPLES": "-DSEN_BUILD_EXAMPLES=ON",
    "ENABLE_CT": "-DSEN_DISABLE_CLANG_TIDY=OFF",
    "ENABLE_COVERAGE": "-DSEN_COVERAGE_ENABLE=ON",
    "ENABLE_ASAN": "-DSEN_USE_SANITIZER=ASanUBSan",
    "ENABLE_TSAN": "-DSEN_USE_SANITIZER=Thread",
}


class SenConan(ConanFile):
    """Conan file that specifies how Sen can be build and packaged with conan."""

    name = "sen"
    author = "Enrique Parodi Spalazzi (enrique.parodi@airbus.com)"  # Main PoC
    url = "https://github.com/airbus/sen"
    homepage = "https://airbus.github.io/sen/latest/"
    description = "Create and integrate distributed systems with ease"
    license = "Apache-2.0"
    topics = ("distributed", "simulation", "rpc", "network", "integration", "federation", "framework")

    # Binary configuration
    settings = "os", "compiler", "build_type", "arch"

    # Allow component discovery from the recipe folder.
    exports = "components/*/CMakeLists.txt"

    # Conan decides which components exist and therefore which dependencies get fetched with the `mode` option.
    options = {
        "mode": ["barebones", "basic", "full"],
    }
    default_options = {
        "mode": "full",
    }

    def _is_component_enabled(self, name):
        """Whether component `name` is enabled for the active `mode`."""
        if name not in COMPONENTS:
            raise ValueError(f"unknown component '{name}'; valid: {sorted(COMPONENTS)}")
        mode = str(self.options.mode)
        if mode == "full":
            return True
        if mode == "basic":
            return name in _BASIC_COMPONENTS
        return False  # barebones

    def _wants_docs(self):
        """Whether the docs toolchain is requested (profile conf, see the sen_build_docs profile)."""
        return bool(self.conf.get("user.sen:build_docs", default=False, check_type=bool))

    def validate(self):
        """Rejects the retired environment toggles, pointing at the CMake flag that replaced each."""
        stale = [(var, flag) for var, flag in _RETIRED_ENV_VARS.items() if getenv(var) is not None]
        if stale:
            hints = ", ".join(f"{var} (use {flag} at the cmake step)" for var, flag in stale)
            raise ConanInvalidConfiguration(f"retired environment variables set: {hints}")

    def build_requirements(self):
        """Defines the dependencies only need for building of Sen."""
        self.tool_requires("cmake/3.28.1")
        self.tool_requires("ninja/1.13.2")
        self.test_requires("gtest/1.17.0")
        if self._wants_docs():
            self.tool_requires("doxygen/[>=1.15.0]")

    def requirements(self):
        """Defines the dependencies of Sen."""
        self.requires("asio/1.36.0", visible=False)
        self.requires("cli11/2.3.2", visible=False)
        self.requires("concurrentqueue/1.0.4", visible=False)
        self.requires("inja/3.5.0", visible=False)
        self.requires("lz4/1.9.4", visible=False)
        self.requires("nlohmann_json/3.11.3", visible=False)
        self.requires("pugixml/1.14", visible=False)
        self.requires("rapidyaml/0.5.0", visible=False)
        self.requires("cpptrace/1.0.4", visible=False)
        self.requires("spdlog/1.17.0", visible=True)

        # explorer-only: ImGui + ImPlot + SDL.
        # ImGui is overridden use the docking branch for ImPlot (Conan would use the non-docking variant otherwise).
        if self._is_component_enabled("explorer"):
            self.requires("implot/0.16", visible=False)
            self.requires("imgui/1.90.5-docking", override=True, visible=False)
            if self.settings.os == "Linux":
                self.requires(
                    "sdl/2.24.0",
                    options={"alsa": False, "pulse": False, "shared": True, "wayland": False, "libunwind": False},
                    visible=False,
                )

        # py-only (Python integration)
        if self._is_component_enabled("py"):
            self.requires("pybind11/2.13.6", visible=True)

        # rest-only
        if self._is_component_enabled("rest"):
            self.requires("llhttp/9.1.3", visible=False)

        # tracy-only
        if self._is_component_enabled("tracy"):
            self.requires("tracy/0.12.1", options={"delayed_init": True, "manual_lifetime": True}, visible=False)

    def system_requirements(self):
        """SDL (pulled in by the explorer on Linux) requires libXext at the system level."""
        if self.settings.os == "Linux" and self._is_component_enabled("explorer"):
            Apt(self).install(["libxext-dev"], update=True)  # ubuntu, debian, ...
            Yum(self).install(["libXext-devel"], update=True)  # almalinux, rockylinux, ...
            Dnf(self).install(["libXext-devel"], update=True)  # fedora, rhel, centos, ...
            Zypper(self).install(["libxext-devel"], update=True)  # opensuse, sles, ...

    def export_sources(self):
        """Defines the set of files that should be exported for building Sen."""
        # Sources are located in the same place as this recipe, copy them to the recipe
        include_patterns = [
            "apps/*",
            "cmake/*",
            "components/*",
            "examples/*",
            "libs/*",
            "test/*",
            "CMakeLists.txt",
            ".clang-tidy",
            ".clang-format",
            "LICENSE.txt",
            "util/*",
            "resources/*",
        ]

        exclude_patterns = ["*/__pycache__/*", "*/.mypy_cache/*", "doc/*", "*/schema.json"]

        for export_source_pattern in include_patterns:
            copy(self, export_source_pattern, self.recipe_folder, self.export_sources_folder, excludes=exclude_patterns)

    # avoid copying source files to the build folder (avoids in-source builds)
    no_copy_source = True

    def set_version(self):
        """Defines the version number that should be used."""
        if isdir(join(self.recipe_folder, ".git")):
            # sets project version either to the current tag or the
            # last tag with with the diff-commit addition.
            # For example:
            #     current tag → 0.4.2
            #     otherwise   → 0.4.2-5-gc6625265
            #                   ▲ ▲ ▲ ▲  └──┬───┘
            #                   │ │ │ │     └─ hash
            #                   │ │ │ └──────  diff
            #                   │ │ └────────  patch
            #                   │ └──────────  minor
            #                   └────────────  major
            #
            #     5-gc6625265 indicates we are at commit c6625265
            #     and 5 commits have been made since tag 0.4.2
            git = Git(self, folder=self.recipe_folder)
            self.version = git.run("describe --tags --always")
        else:
            fictive_version = "0.0.0"
            self.output.warning(f"No Git repository found. Using fictive version {fictive_version}")
            self.version = fictive_version

    def layout(self):
        """Define the folder layout for building Sen."""
        cmake_layout(self)

        # used in the conan editable package mode
        self.cpp.build.builddirs = ["."]

    def generate(self):
        """Generate the cmake dependency and toolchain files."""
        deps = CMakeDeps(self)
        deps.generate()

        tc = CMakeToolchain(self)

        # Default CMake build options.
        tc.cache_variables["SEN_DISABLE_CLANG_TIDY"] = "ON"
        tc.cache_variables["SEN_COVERAGE_ENABLE"] = "OFF"
        tc.cache_variables["SEN_BUILD_EXAMPLES"] = "OFF"
        tc.cache_variables["SEN_BUILD_TESTS"] = "OFF"
        tc.cache_variables["SEN_BUILD_DOCS"] = "ON" if self._wants_docs() else "OFF"
        tc.cache_variables["SEN_USE_SANITIZER"] = "None"

        for component in COMPONENTS:
            is_on = self._is_component_enabled(component)
            tc.cache_variables[f"SEN_BUILD_{component.upper()}"] = "ON" if is_on else "OFF"

        # directory for the generated documentation
        site_dir = getenv("MKDOCS_SITE_DIR")
        if site_dir:
            tc.cache_variables["MKDOCS_SITE_DIR"] = site_dir

        # fetch licenses of our dependencies
        tps_lic_path = join(self.build_folder, "foss_licenses")
        for dep in self.dependencies.values():
            if dep.package_folder is None:
                self.output.warning(f"Dependency {dep.ref} has no package_folder, cannot infer licenses.")
                continue

            target_path = join(tps_lic_path, dep.ref.name)
            copy(self, "licenses/*", dep.package_folder, target_path, keep_path=False)

        tc.generate()

    def build(self):
        """Configure and build Sen."""
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        """Package Sen into a conan package."""
        cmake = CMake(self)
        cmake.install()

    def package_info(self):
        """Calculate the conan package info."""
        self.cpp_info.set_property("cmake_find_mode", "none")
        self.cpp_info.builddirs = [join("cmake", "sen")]
        self.cpp_info.set_property("cmake_target_name", "sen::core sen::kernel sen::db sen::util")

        # runenv library paths
        self.runenv_info.prepend_path("PATH", join(self.package_folder, "bin"))
        if self.settings.os == "Macos":
            self.runenv_info.prepend_path("DYLD_LIBRARY_PATH", join(self.package_folder, "bin"))
        elif self.settings.os == "Linux":
            self.runenv_info.prepend_path("LD_LIBRARY_PATH", join(self.package_folder, "bin"))

        # Windows: PATH is already prepended above; no additional loader path needed.
