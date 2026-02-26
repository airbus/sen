// === init_package.cpp ================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "templates/package/basic_class_header.h"
#include "templates/package/basic_class_source.h"
#include "templates/package/basic_class_stl.h"
#include "templates/package/basic_types_stl.h"
#include "templates/package/cmakelists_txt.h"
#include "templates/package/config_yaml.h"
#include "util.h"

// cli11
#include <CLI/App.hpp>

// inja
#include <inja/json.hpp>

// std
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace
{

struct Args
{
  std::string packageName;
  std::string className;
  std::filesystem::path path;
  std::filesystem::path stlFilesRelPath;
  std::filesystem::path srcFilesRelPath;
  bool emptyClasses = false;
};

std::filesystem::path getClassSTLFileRelPath(const Args& args, const std::string& className)
{
  return (args.stlFilesRelPath / toSnakeCase(className)).replace_extension(".stl");
}

std::filesystem::path getClassCppFileRelPath(const Args& args, const std::string& className)
{
  return (args.srcFilesRelPath / toSnakeCase(className)).replace_extension(".cpp");
}

std::filesystem::path getClassHFile(const std::string& className) { return toSnakeCase(className) + ".h"; }

std::filesystem::path getClassHFileRelPath(const Args& args, const std::string& className)
{
  return (args.srcFilesRelPath / getClassHFile(className));
}

inja::json getPackageData(const Args& args)
{
  inja::json packageData;
  packageData["package_name"] = args.packageName;

  std::vector<std::string> stlFiles;
  std::vector<std::string> srcFiles;
  stlFiles.emplace_back(getClassSTLFileRelPath(args, args.className).generic_string());
  srcFiles.emplace_back(getClassHFileRelPath(args, args.className).generic_string());
  srcFiles.emplace_back(getClassCppFileRelPath(args, args.className).generic_string());

  if (!args.emptyClasses)
  {
    stlFiles.emplace_back((args.stlFilesRelPath / "basic_types.stl").generic_string());
  }

  packageData["source_files"] = std::move(srcFiles);
  packageData["stl_files"] = std::move(stlFiles);
  packageData["deps"] = {};
  packageData["non_empty_classes"] = !args.emptyClasses;

  return packageData;
}

void writePackage(Args& args)
{
  // use snake case for package names and folders
  args.packageName = toSnakeCase(args.path.filename().string());
  args.stlFilesRelPath = std::filesystem::path("stl") / args.packageName;
  args.srcFilesRelPath = "src";

  // force the folder to be snake case
  args.path.replace_filename(args.packageName);

  if (std::filesystem::exists(args.path))
  {
    std::cerr << "directory " << args.path.string() << " already exists\n";
    exit(1);
  }

  std::filesystem::create_directories(args.path / args.stlFilesRelPath);
  std::filesystem::create_directories(args.path / args.srcFilesRelPath);
  const auto packageData = getPackageData(args);

  writeFile(args.path / "CMakeLists.txt", cmakelists_txt, cmakelists_txtSize, packageData);
  writeFile(args.path / args.stlFilesRelPath / "basic_types.stl", basic_types_stl, basic_types_stlSize, packageData);

  // class
  const auto stlFile = getClassSTLFileRelPath(args, args.className);
  const auto headerFile = getClassHFileRelPath(args, args.className);
  const auto sourceFile = getClassCppFileRelPath(args, args.className);

  auto classData = packageData;
  classData["class_name"] = args.className;
  classData["class_rel_stl_file"] = stlFile.generic_string();
  classData["class_rel_header_file"] = headerFile.generic_string();
  classData["class_header_file"] = getClassHFile(args.className).generic_string();

  writeFile(args.path / stlFile, basic_class_stl, basic_class_stlSize, classData);
  writeFile(args.path / headerFile, basic_class_header, basic_class_headerSize, classData);
  writeFile(args.path / sourceFile, basic_class_source, basic_class_sourceSize, classData);
  writeFile(args.path / "config.yaml", config_yaml, config_yamlSize, classData);

  printf("To compile:\n");                                         // NOLINT
  printf("        cd %s\n", args.path.string().c_str());           // NOLINT
  printf("        cmake -S . -B build && cmake --build build\n");  // NOLINT
  printf("\n");                                                    // NOLINT
  printf("To set-up (bash):\n");                                   // NOLINT
  printf("        export LD_LIBRARY_PATH+=$(pwd)/build/bin\n");    // NOLINT
  printf("\n");                                                    // NOLINT
  printf("To set-up (fish):\n");                                   // NOLINT
  printf("        set -xa LD_LIBRARY_PATH $(pwd)/build/bin\n");    // NOLINT
  printf("\n");                                                    // NOLINT
  printf("To run:\n");                                             // NOLINT
  printf("        sen run config.yaml\n");                         // NOLINT
}

}  // namespace

void setupInitPackage(CLI::App& app)
{
  auto args = std::make_shared<Args>();
  auto init = app.add_subcommand("init", "Start a package working area");

  init->add_option("path", args->path, "Path")->required();
  init->add_option("--class", args->className, "Name of a class")->required();
  init->callback([args]() { writePackage(*args); });
}
