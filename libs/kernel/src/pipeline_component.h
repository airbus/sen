// === pipeline_component.h ============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_LIBS_KERNEL_SRC_PIPELINE_COMPONENT_H
#define SEN_LIBS_KERNEL_SRC_PIPELINE_COMPONENT_H

// kernel
#include "operating_system.h"
#include "shared_library.h"

// sen
#include "sen/core/base/class_helpers.h"
#include "sen/core/meta/type.h"
#include "sen/core/meta/type_registry.h"
#include "sen/core/meta/var.h"
#include "sen/core/obj/native_object.h"
#include "sen/kernel/component.h"
#include "sen/kernel/component_api.h"
#include "sen/kernel/kernel_config.h"

// generated code
#include "stl/sen/kernel/basic_types.stl.h"

// std
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace sen::kernel
{

class PipelineComponent final: public Component
{
  SEN_NOCOPY_NOMOVE(PipelineComponent)

public:
  explicit PipelineComponent(KernelConfig::PipelineToLoad config);
  ~PipelineComponent() override;

public:
  [[nodiscard]] FuncResult preload(PreloadApi&& api) override;
  [[nodiscard]] PassResult init(InitApi&& api) override;
  [[nodiscard]] FuncResult run(RunApi& api) override;
  [[nodiscard]] FuncResult unload(UnloadApi&& api) override;
  [[nodiscard]] bool isRealTimeOnly() const noexcept override;

private:
  void openLibs(CustomTypeRegistry& reg);
  void fetchTypes(CustomTypeRegistry& reg);
  void lookupType(const std::string& name, CustomTypeRegistry& reg) const;
  void validateConfig(const CustomTypeRegistry& reg) const;
  void createObjects(const CustomTypeRegistry& reg);
  void establishConnections(InitApi&& api);
  void closeConnections(UnloadApi&& api);
  void publishObjects();
  void removeObjects();
  void deleteObjects();
  [[nodiscard]] ObjectSource& fetchConnection(const std::string& connectionString);
  [[nodiscard]] std::unordered_map<std::string, std::vector<std::shared_ptr<NativeObject>>> compileObjectsByBus();

private:
  using TypeGetterFunc = const Type* (*)();
  using AllTypesGetterFunc = void (*)(ExportedTypesList&);

private:
  KernelConfig::PipelineToLoad config_;
  std::vector<SharedLibrary> libs_;
  std::vector<std::shared_ptr<NativeObject>> objects_;
  std::unordered_map<std::string, std::shared_ptr<ObjectSource>> connections_;
  std::unordered_map<const Type*, InstanceMakerFunc> instanceMakers_;
  std::unordered_map<const NativeObject*, const KernelConfig::ObjectConfig*> objectConfigs_;
  std::shared_ptr<OperatingSystem> os_;
};

}  // namespace sen::kernel

#endif  // SEN_LIBS_KERNEL_SRC_PIPELINE_COMPONENT_H
