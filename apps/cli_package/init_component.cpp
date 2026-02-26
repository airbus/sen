// === init_component.cpp ==============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "templates/component/cmakelists_txt.h"
#include "templates/component/component_basic_cpp.h"
#include "templates/component/component_full_cpp.h"
#include "templates/component/component_with_cfg_cpp.h"
#include "templates/component/config_stl.h"
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
  std::string componentName;
  std::string targetName;
  std::filesystem::path path;
  std::filesystem::path stlFilesRelPath;
  std::filesystem::path srcFilesRelPath;
  bool withConfig = false;
  bool full = false;
  bool emptyClasses = false;
};

inja::json getComponentData(const Args& args)
{
  std::vector<std::string> stlFiles = {(args.stlFilesRelPath / "config.stl").generic_string()};
  std::vector<std::string> srcFiles = {(args.srcFilesRelPath / "component.cpp").generic_string()};

  inja::json componentData;
  componentData["component_name"] = args.componentName;
  componentData["target_name"] = args.targetName;
  componentData["package_name"] = args.targetName;
  componentData["source_files"] = std::move(srcFiles);
  if (args.full || args.withConfig)
  {
    componentData["stl_files"] = std::move(stlFiles);
  }
  else
  {
    componentData["stl_files"] = {};
  }
  componentData["deps"] = {};

  return componentData;
}

void writeComponent(Args& args)
{
  // use snake case for the target name names and the component folder
  args.componentName = args.path.filename().generic_string();

  if (!isUpperCamelCase(args.componentName))
  {
    std::cerr << "component names must be upper camel cased\n";
    exit(1);
  }

  args.targetName = toSnakeCase(args.componentName);
  args.path.replace_filename(args.targetName);
  args.stlFilesRelPath = std::filesystem::path("stl") / args.targetName;
  args.srcFilesRelPath = "src";

  if (std::filesystem::exists(args.path))
  {
    std::cerr << "directory " << args.path.string() << " already exists\n";
    exit(2);
  }

  std::filesystem::create_directories(args.path / args.srcFilesRelPath);

  if (args.withConfig || args.full)
  {
    std::filesystem::create_directories(args.path / args.stlFilesRelPath);
  }

  const auto componentData = getComponentData(args);

  writeFile(args.path / "CMakeLists.txt", cmakelists_txt, cmakelists_txtSize, componentData);
  if (args.withConfig || args.full)
  {
    writeFile(args.path / args.stlFilesRelPath / "config.stl", config_stl, config_stlSize, componentData);
  }

  if (args.withConfig)
  {
    writeFile(args.path / args.srcFilesRelPath / "component.cpp",
              component_with_cfg_cpp,
              component_with_cfg_cppSize,
              componentData);
  }
  else if (args.full)
  {
    writeFile(
      args.path / args.srcFilesRelPath / "component.cpp", component_full_cpp, component_full_cppSize, componentData);
  }
  else
  {
    writeFile(
      args.path / args.srcFilesRelPath / "component.cpp", component_basic_cpp, component_basic_cppSize, componentData);
  }
}

}  // namespace

void setupInitComponent(CLI::App& app)
{
  auto args = std::make_shared<Args>();
  auto init = app.add_subcommand("init-component", "Start a component working area");

  init->add_option("path", args->path, "Path")->required();
  auto withCfg = init->add_flag("--with-config", args->withConfig, "Include STL and template for configuration");
  auto full = init->add_flag("--full", args->full, "Include configuration and implementation of all methods");

  withCfg->excludes(full);

  init->callback([args]() { writeComponent(*args); });
}
