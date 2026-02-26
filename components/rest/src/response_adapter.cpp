// === response_adapter.cpp ============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "response_adapter.h"

// generated code
#include "stl/types.stl.h"

// sen
#include "sen/core/base/assert.h"
#include "sen/core/base/compiler_macros.h"
#include "sen/core/base/numbers.h"
#include "sen/core/meta/alias_type.h"
#include "sen/core/meta/enum_type.h"
#include "sen/core/meta/optional_type.h"
#include "sen/core/meta/sequence_type.h"
#include "sen/core/meta/struct_type.h"
#include "sen/core/meta/type.h"
#include "sen/core/meta/type_visitor.h"
#include "sen/core/meta/var.h"
#include "sen/core/meta/variant_type.h"

// std
#include <string>

namespace sen::components::rest
{

void adaptForJsonResponse(sen::Var& var, const sen::Type* type)
{
  class RestAPITypeAdapter final: public sen::TypeVisitor
  {
  public:
    SEN_NOCOPY_NOMOVE(RestAPITypeAdapter)

  public:
    explicit RestAPITypeAdapter(sen::Var& var): var_(var) {}

    ~RestAPITypeAdapter() override = default;

  public:
    void apply(const EnumType& type) override
    {
      if (!var_.holds<std::string>())
      {
        const auto* enumeration = type.getEnumFromKey(var_.getCopyAs<u32>());
        if (!enumeration)
        {
          throwRuntimeError("Cannot get enum from key");
        }

        if (type.getHash() == sen::MetaTypeTrait<HttpMethod>::meta()->getHash())
        {
          switch (var_.getCopyAs<u8>())
          {
            case 0:
              var_ = "get";
              break;

            case 1:
              var_ = "post";
              break;

            case 2:
              var_ = "put";
              break;

            case 3:
              var_ = "delete";
              break;

            case 4:
              var_ = "option";
              break;
          }
        }
        else if (type.getHash() == sen::MetaTypeTrait<NotificationType>::meta()->getHash())
        {
          switch (var_.getCopyAs<u8>())
          {
            case 0:
              var_ = "event";
              break;

            case 1:
              var_ = "invoke";
              break;

            case 2:
              var_ = "property";
              break;

            case 3:
              var_ = "object_added";
              break;

            case 4:
              var_ = "object_removed";
              break;
          }
        }
        else if (type.getHash() == sen::MetaTypeTrait<RelType>::meta()->getHash())
        {
          switch (var_.getCopyAs<u8>())
          {
            case 0:
              var_ = "method";
              break;

            case 1:
              var_ = "setter";
              break;

            case 2:
              var_ = "getter";
              break;

            case 3:
              var_ = "event";
              break;

            case 4:
              var_ = "property";
              break;

            case 5:
              var_ = "property_subscribe";
              break;

            case 6:
              var_ = "property_unsubscribe";
              break;

            case 7:
              var_ = "def";
              break;
          }
        }
        else
        {
          var_ = enumeration->name;
        }
      }
    }

    void apply(const sen::StructType& type) override
    {
      auto& mapRef = var_.get<VarMap>();

      for (const auto& field: type.getFields())
      {
        adaptForJsonResponse(mapRef[field.name], field.type.type());
      }
    }

    void apply(const sen::VariantType& type) override
    {
      if (var_.holds<KeyedVar>())
      {
        const auto& [key, var] = var_.get<KeyedVar>();

        const auto* field = type.getFieldFromKey(key);
        if (!field)
        {
          throwRuntimeError("Cannot find variant field by key");
        }

        adaptForJsonResponse(*var, field->type.type());
      }
    }

    void apply(const SequenceType& type) override
    {
      auto& listRef = var_.get<VarList>();

      for (auto& elem: listRef)
      {
        adaptForJsonResponse(elem, type.getElementType().type());
      }
    }

    void apply(const AliasType& type) override { adaptForJsonResponse(var_, type.getAliasedType().type()); }

    void apply(const OptionalType& type) override { adaptForJsonResponse(var_, type.getType().type()); }

  private:
    sen::Var& var_;
  };

  RestAPITypeAdapter adapter(var);
  type->accept(adapter);
}

}  // namespace sen::components::rest
