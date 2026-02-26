// === object.cpp ======================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "sen/core/obj/object.h"

// sen
#include "sen/core/base/assert.h"
#include "sen/core/base/hash32.h"
#include "sen/core/base/uuid.h"
#include "sen/core/io/output_stream.h"
#include "sen/core/meta/type.h"
#include "sen/core/obj/connection_guard.h"

// std
#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>

namespace sen
{

ObjectId Object::senImplMakeId(std::string_view objectName) const
{
  UuidRandomGenerator generator;
  return hashCombine(generator().getHash32(), objectName);
}

void Object::senImplValidateName(std::string_view name)
{
  if (name.empty())
  {
    throwRuntimeError("cannot create objects with empty names");
  }

  auto wrongCharItr = std::find_if(name.begin(), name.end(), [](unsigned char c) { return c == ' '; });

  if (wrongCharItr != name.end())
  {
    throwRuntimeError("object names cannot contain literal space characters ' ': name \"" + std::string(name) +
                      "\" is invalid");
  }
}

std::string Object::senImplComputeLocalName(std::string_view name, std::string_view prefix)
{
  if (prefix.empty())
  {
    return std::string(name);
  }

  std::string result(prefix);
  result.append(".");
  result.append(name);

  senImplValidateName(result);
  return result;
}

ConnectionGuard Object::senImplMakeConnectionGuard(ConnId id, MemberHash member, bool typed)
{
  return {this, id.get(), member, typed};
}

namespace impl
{

void writeAllPropertiesToStream(const Object* instance, OutputStream& out)
{
  instance->senImplWriteAllPropertiesToStream(out);
}

}  // namespace impl

}  // namespace sen
