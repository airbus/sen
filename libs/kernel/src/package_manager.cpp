// === package_manager.cpp =============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "sen/kernel/package_manager.h"

// implementation
#include "operating_system.h"
#include "shared_library.h"

// sen
#include "sen/core/base/assert.h"
#include "sen/core/base/compiler_macros.h"
#include "sen/core/meta/class_type.h"
#include "sen/core/meta/custom_type.h"
#include "sen/core/meta/type.h"
#include "sen/core/meta/type_registry.h"

// spdlog
#include <spdlog/spdlog.h>

// std
#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace sen::kernel
{

//--------------------------------------------------------------------------------------------------------------
// PackageManager::Impl
//--------------------------------------------------------------------------------------------------------------

class PackageManager::Impl
{
  SEN_NOCOPY_NOMOVE(Impl)

public:
  explicit Impl(CustomTypeRegistry& reg): types_ {reg} {}

  ~Impl() = default;

public:
  void import(const std::vector<std::string>& packageNames)  // NOSONAR
  {
    // open all the libraries
    for (const auto& importedLibrary: packageNames)
    {
      if (std::find(importedLibs_.begin(), importedLibs_.end(), importedLibrary) != importedLibs_.end())
      {
        continue;
      }

      auto lib = os_->openSharedLibrary(importedLibrary);
      if (lib.nativeHandle == nullptr)
      {
        throwRuntimeError("could not open library");
      }

      libs_.push_back(lib);

      // try to get all the exported types
      if (auto* allTypesFunc = os_->getSymbol(lib, "getAllTypes"); allTypesFunc != nullptr)
      {
        ExportedTypesList allTypes;

        // get the function that provides all the types
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        (*reinterpret_cast<AllTypesGetterFunc>(allTypesFunc))(allTypes);  // NOSONAR

        for (auto [type, maker]: allTypes)
        {
          types_.add(type);

          // get the maker
          if (maker != nullptr && type->isClassType())
          {
            types_.addInstanceMaker(*type->asClassType(), maker);
          }

          spdlog::debug("imported type {} from library {}",
                        type->isCustomType() ? type->asCustomType()->getQualifiedName() : type->getName(),
                        importedLibrary);
        }
      }

      importedLibs_.push_back(importedLibrary);
    }
  }

  [[nodiscard]] const CustomTypeRegistry& getImportedTypes() const noexcept { return types_; }

  [[nodiscard]] const Type* lookupType(const std::string& typeName)
  {
    const auto allTypes = types_.getAll();

    // look up in the registry first
    if (auto regItr = allTypes.find(typeName); regItr != allTypes.end())
    {
      return regItr->second.type();  //.get();
    }

    auto getter = ClassType::computeTypeGetterFuncName(typeName);

    // search in the imported libraries
    for (const auto& lib: libs_)
    {
      auto* typeGetter = os_->getSymbol(lib, getter);
      if (typeGetter != nullptr)
      {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        const auto* type = (*reinterpret_cast<TypeGetterFunc>(typeGetter))();  // NOSONAR

        // Type lifetime is guaranteed through the generated type function and senExport##classname.
        types_.add(sen::makeNonOwningTypeHandle(type));

        auto makerSymbol = ClassType::computeInstanceMakerFuncName(typeName);

        // get the maker
        auto* maker = os_->getSymbol(lib, makerSymbol);
        SEN_ENSURE(maker != nullptr);

        SEN_ENSURE(type->isClassType() && "Loaded type should have been a class type.");
        auto* classType = type->asClassType();

        types_.addInstanceMaker(
          *classType,
          reinterpret_cast<InstanceMakerFunc>(maker));  // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)

        spdlog::debug("manually imported type {}", type->asCustomType()->getQualifiedName());
        return type;
      }
    }

    return nullptr;
  }

private:
  using TypeGetterFunc = const Type* (*)();
  using AllTypesGetterFunc = void (*)(ExportedTypesList&);

private:
  std::vector<std::string> importedLibs_;
  std::vector<SharedLibrary> libs_;
  std::shared_ptr<OperatingSystem> os_ = makeNativeOS();
  CustomTypeRegistry& types_;
};

//--------------------------------------------------------------------------------------------------------------
// PackageManager
//--------------------------------------------------------------------------------------------------------------

PackageManager::PackageManager(CustomTypeRegistry& reg): pimpl_(std::make_unique<Impl>(reg)) {}

PackageManager::~PackageManager() = default;

void PackageManager::import(const std::vector<std::string>& packageNames) { return pimpl_->import(packageNames); }

const CustomTypeRegistry& PackageManager::getImportedTypes() const noexcept { return pimpl_->getImportedTypes(); }

const Type* PackageManager::lookupType(const std::string& typeName) { return pimpl_->lookupType(typeName); }

}  // namespace sen::kernel
