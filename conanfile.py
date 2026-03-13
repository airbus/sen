# === conanfile.py =====================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

from conan import ConanFile
from conan.tools.cmake import CMakeDeps, CMakeToolchain, CMake, cmake_layout
from conan.tools.files import copy
from conan.tools.scm.git import Git
from conan.tools.system.package_manager import Apt, Yum, Dnf, Zypper
from os.path import isdir, join
from os import getenv

class SenConan(ConanFile):
    name = "sen"
    author = "Enrique Parodi Spalazzi (enrique.parodi@airbus.com)"  # Main PoC
    url = "https://github.com/airbus/sen"
    homepage = "https://airbus.github.io/sen/latest/"
    description = "Create and integrate distributed systems with ease"
    license = "Apache-2.0"
    topics = ("distributed", "simulation", "rpc", "network", "integration", "federation", "framework")

    # Binary configuration
    settings = "os", "compiler", "build_type", "arch"

    def build_requirements(self):
        self.tool_requires("cmake/3.28.1")
        self.tool_requires("ninja/1.13.2")
        self.test_requires("gtest/1.17.0")

    def requirements(self):
        # non-visible dependencies
        self.requires("asio/1.36.0", visible=False)
        self.requires("cli11/2.3.2", visible=False)
        self.requires("concurrentqueue/1.0.4", visible=False)
        self.requires("implot/0.16", visible=False)
        self.requires("imgui/1.90.5-docking", override=True, visible=False)
        self.requires("inja/3.5.0", visible=False)
        self.requires("lz4/1.9.4", visible=False)
        self.requires("nlohmann_json/3.11.3", visible=False)
        self.requires("pugixml/1.14", visible=False)
        self.requires("rapidyaml/0.5.0", visible=False)
        self.requires("cpptrace/1.0.4", visible=False)
        self.requires("llhttp/9.1.3", visible=False)
        self.requires("tracy/0.12.1", options={"delayed_init": True, "manual_lifetime": True}, visible=False)

        # our usage of imgui in Linux has an implicit dependency on SDL.
        # SDL has a missing dependency on libext-dev, so we need to install it.
        if self.settings.os == "Linux":
            Apt(self).install(["libxext-dev"], update=True)  # ubuntu, debian, ...
            Yum(self).install(["libxext-devel"], update=True)  # almalinux, ...
            Dnf(self).install(["libxext-devel"], update=True)  # fedora, rhel, centos, ...
            Zypper(self).install(["libxext-devel"], update=True)  # opensuse, sles, ...

            self.requires("sdl/2.24.0",
                          options={"alsa": False, "pulse": False, "shared": True, "wayland": False, "libunwind": False},
                          visible=False)

        # visible dependencies
        self.requires("pybind11/2.11.1", visible=True)
        self.requires("spdlog/1.17.0", visible=True)

    def export_sources(self):
        # sources that are in git, but we don't want to include in the package
        _excluded_sources = (".conan", ".github", ".cmake-format.py", ".gitattributes", ".gitignore",
                             ".mdformat.toml", ".pre-commit-config.yaml", ".sonar", ".yamlfix.toml", ".yamllint",
                             "conanfile.py", "docs", "utils", "README.md")

        git = Git(self, folder=self.recipe_folder)
        # get the list of files not ignored by .gitignore
        included_sources = git.included_files()

        for file_path in included_sources:
            if not file_path.startswith(_excluded_sources):
                copy(self, file_path, self.recipe_folder, self.export_sources_folder)

    # avoid copying source files to the build folder (avoids in-source builds)
    no_copy_source = True

    def set_version(self):
        if isdir(join(self.recipe_folder, '.git')):
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
        cmake_layout(self)

    def generate(self):
        deps = CMakeDeps(self)
        deps.generate()

        tc = CMakeToolchain(self)
        tc.cache_variables["SEN_DISABLE_CLANG_TIDY"] = "OFF" if env_var_to_bool("ENABLE_CT") else "ON"
        tc.cache_variables["SEN_COVERAGE_ENABLE"] = "ON" if env_var_to_bool("ENABLE_COVERAGE") else "OFF"
        tc.cache_variables["SEN_BUILD_EXAMPLES"] = "ON" if env_var_to_bool("ENABLE_EXAMPLES") else "OFF"
        tc.cache_variables["SEN_BUILD_TESTS"] = "ON" if env_var_to_bool("ENABLE_TESTS") else "OFF"

        if self.settings.os == "Windows":
            # Needed for windows builds until we have moved towards a windows conan profile
            tc.generator = "Ninja"

        # directory for the generated documentation
        site_dir = getenv("MKDOCS_SITE_DIR")
        if site_dir:
            tc.variables["MKDOCS_SITE_DIR"] = site_dir

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
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        cmake = CMake(self)
        cmake.install()

    def package_info(self):
        self.cpp_info.set_property("cmake_find_mode", "none")
        self.cpp_info.builddirs = [join("cmake", "sen")]
        for component in ["core", "kernel", "db", "util"]:
            self.cpp_info.components[component].set_property("cmake_target_name", f"sen::{component}")

        # runenv library paths
        self.runenv_info.prepend_path("PATH", join(self.package_folder, "bin"))
        if self.settings.os == "Macos":
            self.runenv_info.prepend_path("DYLD_LIBRARY_PATH", join(self.package_folder, "bin"))
        elif self.settings.os == "Linux":
            self.runenv_info.prepend_path("LD_LIBRARY_PATH", join(self.package_folder, "bin"))


def env_var_to_bool(env_var_name) -> bool:
    """
    Loads the value of an environment variable and converts it to bool.

    If no variable is found, `false` is returned.
    """
    maybe_env_var_value = getenv(env_var_name, None)
    if maybe_env_var_value:
        env_var_value = maybe_env_var_value.lower()
        if env_var_value in ("true", "on", "1", "yes"):
            return True

    return False
