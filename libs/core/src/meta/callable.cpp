// === callable.cpp ====================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "sen/core/meta/callable.h"

// implementation
#include "utils.h"

// sen
#include "sen/core/base/assert.h"
#include "sen/core/base/span.h"
#include "sen/core/meta/type.h"

// std
#include <cstddef>
#include <string>
#include <string_view>
#include <utility>

namespace sen
{

namespace
{

void checkCallableArgs(const CallableSpec& spec)
{
  const auto argCount = spec.args.size();

  // all arguments shall have unique names and unique keys
  for (std::size_t i = 0U; i < argCount; i++)
  {
    const auto& currentArg = spec.args.at(i);

    for (std::size_t j = 0U; j < argCount; j++)
    {
      if (j != i)
      {
        const auto& otherArg = spec.args.at(j);

        // check the name
        if (otherArg.name == currentArg.name)
        {
          std::string err;
          err.append("duplicated argument name '");
          err.append(currentArg.name);
          err.append("'");
          sen::throwRuntimeError(err);
        }
      }
    }
  }
}

}  // namespace

Callable::Callable(CallableSpec spec): spec_(std::move(spec))
{
  // populate the map from name hash to Arg index
  for (size_t i = 0; i < spec_.args.size(); ++i)
  {
    nameHashToArgIndex_.try_emplace(spec_.args[i].getNameHash(), i);
  }
}

std::string_view Callable::getName() const noexcept { return spec_.name; }

std::string_view Callable::getDescription() const noexcept { return spec_.description; }

TransportMode Callable::getTransportMode() const noexcept { return spec_.transportMode; }

Span<const Arg> Callable::getArgs() const noexcept { return makeConstSpan(spec_.args); }

const CallableSpec& Callable::getCallableSpec() const noexcept { return spec_; }

bool Callable::operator==(const Callable& other) const noexcept { return (this == &other) || spec_ == other.spec_; }

bool Callable::operator!=(const Callable& other) const noexcept { return !(*this == other); }

void Callable::checkSpec(const CallableSpec& spec)
{
  impl::checkMemberName(spec.name);
  checkCallableArgs(spec);
}

const Arg* Callable::getArgFromName(std::string_view name) const
{
  for (const auto& item: spec_.args)
  {
    if (item.name == name)
    {
      return &item;
    }
  }

  return nullptr;
}

size_t Callable::getArgIndexFromNameHash(MemberHash nameHash) const
{
  if (const auto& itr = nameHashToArgIndex_.find(nameHash); itr != nameHashToArgIndex_.end())
  {
    return itr->second;
  }

  throwRuntimeError("Fatal error in Callable::getArgIndexFromNameHash. Argument index not found.");
}

}  // namespace sen
