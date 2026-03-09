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
from os.path import isdir, join
from os import getenv
from pathlib import Path
from shutil import copy2, copytree
from glob import glob


class SenConan(ConanFile):
    # Metadata
    name = "sen"
    author = "Enrique Parodi Spalazzi (enrique.parodi@airbus.com)"
    url = "https://github.com/airbus/sen"
    description = "Sen Software Infrastructure"
    license = "Apache 2.0"

    # Binary configuration
    settings = "os", "compiler", "build_type", "arch"

    def build_requirements(self):
        self.tool_requires("cmake/3.28.1")

    def configure(self) -> None:
        # sdl
        if self.settings.os != "Windows":
            self.options["sdl"].alsa = False
            self.options["sdl"].pulse = False
            self.options["sdl"].shared = True
            self.options["sdl"].wayland = False
            self.options["sdl"].libunwind = False

    def requirements(self):
        self.requires("asio/1.36.0")
        self.requires("cli11/2.3.2")
        self.requires("concurrentqueue/1.0.4")
        self.requires("gtest/1.17.0")
        self.requires("implot/0.16")
        self.requires("imgui/1.90.5-docking", override=True)
        self.requires("inja/3.5.0")
        self.requires("lz4/1.9.4")
        self.requires("nlohmann_json/3.11.3")
        self.requires("pugixml/1.14")
        self.requires("pybind11/2.11.1")
        self.requires("rapidyaml/0.5.0")
        self.requires("spdlog/1.12.0")
        self.requires("cpptrace/1.0.4")
        self.requires("llhttp/9.1.3")
        self.requires("tracy/0.12.1")

        if self.settings.os != "Windows":
            self.requires("sdl/2.24.0")

    def export_sources(self):
        # sources that are in git, but we don't want to include in the package
        _excluded_sources = (".conan", ".github", ".jenkins", ".cmake-format.py", ".gitattributes", ".gitignore",
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
            self.version = "%s" % (git.run("describe --tags --always"))
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

        if self.settings.os == "Windows":
            # Needed for windows builds until we have moved towards a windows conan profile
            tc.generator = "Ninja"

        tc.cache_variables["SEN_DISABLE_CLANG_TIDY"] = "OFF" if env_var_to_bool("ENABLE_CT") else "ON"
        tc.cache_variables["SEN_COVERAGE_ENABLE"] = "ON" if env_var_to_bool("ENABLE_COVERAGE") else "OFF"
        tc.cache_variables["SEN_BUILD_EXAMPLES"] = "OFF" if env_var_to_bool("DISABLE_EXAMPLES") else "ON"

        # directory for the generated documentation
        site_dir = getenv("MKDOCS_SITE_DIR")
        if site_dir:
            tc.variables["MKDOCS_SITE_DIR"] = site_dir

        # fetch licenses of our dependencies
        tps_lic_path = join(self.source_folder, "build", "foss_licenses")
        for dep in self.dependencies.values():
            target_path = join(tps_lic_path, dep.ref.name)

            # ensure that the target folder exists
            Path(target_path).mkdir(parents=True, exist_ok=True)

            if dep.package_folder is None:
                self.output.warning(f"Dependency {dep.ref} has no package_folder, cannot infer licenses.")
                continue

            # collect all the licenses for this dependency
            licenses = glob(join(dep.package_folder, "licenses/*"))

            # copy all the found licenses into a dedicated folder per dependency
            for elem in licenses:
                if isdir(elem):
                    copytree(elem, target_path, dirs_exist_ok=True)
                else:
                    copy2(elem, target_path, follow_symlinks=True)

        tc.generate()

    def build(self):
        cmake = CMake(self)

        cmake.configure()
        cmake.build()

        # TODO (SEN-480): Enable automatic testing
        # cmake.test()

    def package(self):
        cmake = CMake(self)
        cmake.install()

    def package_info(self):
        self.cpp_info.set_property("cmake_find_mode", "none")
        self.cpp_info.builddirs = [join("cmake", "sen")]
        self.cpp_info.set_property("cmake_target_name", "sen::core sen::kernel")

        # runenv library paths
        self.runenv_info.prepend_path("PATH", join(self.package_folder, "bin"))
        self.runenv_info.prepend_path("DYLD_LIBRARY_PATH", join(self.package_folder, "bin"))
        self.runenv_info.prepend_path("LD_LIBRARY_PATH", join(self.package_folder, "bin"))


def env_var_to_bool(env_var_name) -> bool:
    """
    Loads the value of an environment variable and converts it to bool.

    If no variable is found, `false` is returned.
    """
    maybe_env_var_value = getenv(env_var_name, None)
    if maybe_env_var_value:
        env_var_value = maybe_env_var_value.lower()
        if env_var_value == "true" or env_var_value == "on":
            return True

    return False
