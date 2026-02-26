// === method.cpp ======================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "sen/core/meta/method.h"

// sen
#include "sen/core/base/hash32.h"
#include "sen/core/meta/callable.h"
#include "sen/core/meta/type.h"

// std
#include <memory>
#include <utility>

namespace sen
{
Method::Method(const MethodSpec& spec, Private /* private */)
  : Callable(spec.callableSpec)
  , constness_(spec.constness)
  , deferred_(spec.deferred)
  , localOnly_(spec.localOnly)
  , returnType_(spec.returnType)
  , propertyRelation_(spec.propertyRelation)
  , id_(hashCombine(methodHashSeed, spec.callableSpec.name))
  , hash_(impl::hash<MethodSpec>()(spec))
{
}

Constness Method::getConstness() const noexcept { return constness_; }

bool Method::getDeferred() const noexcept { return deferred_; }

ConstTypeHandle<> Method::getReturnType() const noexcept { return returnType_; }

const PropertyRelation& Method::getPropertyRelation() const noexcept { return propertyRelation_; }

bool Method::getLocalOnly() const noexcept { return localOnly_; }

bool Method::operator==(const Method& other) const noexcept
{
  if (this == &other)
  {
    return true;
  }

  return Callable::operator==(other) && constness_ == other.constness_ && returnType_ == other.returnType_;
}

bool Method::operator!=(const Method& other) const noexcept { return !(*this == other); }

MemberHash Method::getHash() const noexcept { return hash_; }

MemberHash Method::getId() const noexcept { return id_; }

std::shared_ptr<Method> Method::make(MethodSpec spec)
{
  checkSpec(spec.callableSpec);
  return std::make_shared<Method>(std::move(spec), Private {});
}

}  // namespace sen
