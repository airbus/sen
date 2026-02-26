// === native_object_impl.h ============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_CORE_OBJ_DETAIL_NATIVE_OBJECT_IMPL_H
#define SEN_CORE_OBJ_DETAIL_NATIVE_OBJECT_IMPL_H

// sen
#include "sen/core/base/compiler_macros.h"
#include "sen/core/base/memory_block.h"
#include "sen/core/base/timestamp.h"
#include "sen/core/io/input_stream.h"
#include "sen/core/io/output_stream.h"
#include "sen/core/meta/native_types.h"
#include "sen/core/meta/struct_type.h"
#include "sen/core/meta/type.h"
#include "sen/core/meta/var.h"
#include "sen/core/obj/detail/event_buffer.h"
#include "sen/core/obj/detail/property_flags.h"

// std
#include <functional>
#include <string>

//--------------------------------------------------------------------------------------------------------------
// Forward declarations
//--------------------------------------------------------------------------------------------------------------

namespace sen::impl
{
class NativeObjectProxy;
class FilteredProvider;
}  // namespace sen::impl

namespace sen::kernel
{
class RegistrationApi;
class RunApi;
class PipelineComponent;
}  // namespace sen::kernel

namespace sen::kernel::impl
{
class RemoteParticipant;
class LocalParticipant;
class Runner;
class ObjectUpdate;
}  // namespace sen::kernel::impl

namespace sen::impl
{

//--------------------------------------------------------------------------------------------------------------
// Helper types
//--------------------------------------------------------------------------------------------------------------

/// Function that returns a buffer that can hold the provided size (might be partially used)
using BufferProvider = std::function<ResizableBufferWriter<FixedMemoryBlock>(std::size_t)>;

//--------------------------------------------------------------------------------------------------------------
// Used by the generated code
//--------------------------------------------------------------------------------------------------------------

template <typename T>
void writePropertyToStream(uint32_t id, uint32_t valueSize, MaybeRef<T> val, OutputStream& out);

template <typename T>
void writePropertyToStream(uint32_t id, MaybeRef<T> val, OutputStream& out);

template <typename T>
void writePropertyToStream(uint32_t id, MaybeRef<T> val, BufferProvider provider);

template <typename T>
[[nodiscard]] T fromStream(InputStream& in);

template <typename T>
void variantToStream(OutputStream& out, const Var& var)
{
  SerializationTraits<T>::write(out, toValue<T>(var));
}

template <typename T>
[[nodiscard]] Var variantFromStream(InputStream& in)
{
  return toVariant<T>(fromStream<T>(in));
}

inline void dispatchIfChanged(PropertyFlags& flags,
                              EventBuffer<>& eventBuffer,
                              uint32_t propertyId,
                              ObjectId objectId,
                              TimeStamp time,
                              TransportMode transport,
                              Object* producer)
{
  if (flags.changedInLastCycle())
  {
    eventBuffer.dispatch(propertyId, time, objectId, transport, false, nullptr, producer);
  }
}

inline void unconditionalDispatch(EventBuffer<>& eventBuffer,
                                  uint32_t propertyId,
                                  ObjectId objectId,
                                  TimeStamp time,
                                  TransportMode transport,
                                  Object* producer)
{
  eventBuffer.dispatch(propertyId, time, objectId, transport, false, nullptr, producer);
}

template <typename T>
inline void writePropertyIfChanged(PropertyFlags& flags, uint32_t propertyId, OutputStream& out, MaybeRef<T> val)
{
  if (flags.changedInLastCycle())
  {
    writePropertyToStream<T>(propertyId, val, out);
  }
}

template <typename T>
inline void writePropertyIfChanged(PropertyFlags& flags, uint32_t propertyId, BufferProvider provider, MaybeRef<T> val)
{
  if (flags.changedInLastCycle())
  {
    writePropertyToStream<T>(propertyId, val, provider);
  }
}

template <typename T>
T getFromMap(const std::string& name, const VarMap& map, std::function<void(const T&)> validator = nullptr);

template <typename T>
void tryGetFromMap(const std::string& name,
                   const VarMap& map,
                   T& val,
                   std::function<void(const T&)> validator = nullptr);

struct FieldValueGetter
{
  lang::ValueGetter getterFunc;
  const impl::PropertyFlags* flags = nullptr;
};

//----------------------------------------------------------------------------------------------------------------------
// Inline implementation
//----------------------------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------------------
// Used by the generated code
//--------------------------------------------------------------------------------------------------------------

template <typename T>
inline void writePropertyToStream(uint32_t id, uint32_t valueSize, MaybeRef<T> val, OutputStream& out)
{
  out.writeUInt32(id);
  out.writeUInt32(valueSize);
  SerializationTraits<T>::write(out, val);
}

template <typename T>
inline void writePropertyToStream(uint32_t id, MaybeRef<T> val, OutputStream& out)
{
  writePropertyToStream<T>(id, SerializationTraits<T>::serializedSize(val), val, out);
}

template <typename T>
inline void writePropertyToStream(uint32_t id, MaybeRef<T> val, BufferProvider provider)
{
  const auto valueSize = SerializationTraits<T>::serializedSize(val);

  auto writer = provider(getSerializedSize(id) + getSerializedSize(valueSize));
  OutputStream out(writer);

  writePropertyToStream<T>(id, valueSize, val, out);
}

template <typename T>
inline T fromStream(InputStream& in)
{
  T result {};
  SerializationTraits<T>::read(in, result);
  return result;
}

template <typename T>
inline T getFromMap(const std::string& name, const VarMap& map, std::function<void(const T&)> validator)
{
  if (!validator)
  {  // Provide a default validator
    validator = [](const T&) {};
  }

  auto itr = map.find(name);
  if (itr == map.end())
  {
    std::string err;
    err.append("could not find mandatory item '");
    err.append(name);
    err.append("' in map");
    throwRuntimeError(err);
    SEN_UNREACHABLE();
  }

  auto value = toValue<T>(itr->second);
  validator(value);
  return value;
}

template <typename T>
inline void tryGetFromMap(const std::string& name, const VarMap& map, T& val, std::function<void(const T&)> validator)
{
  if (!validator)
  {  // Provide a default validator
    validator = [](const T&) {};
  }

  auto itr = map.find(name);
  if (itr != map.end())
  {
    auto valCopy = toValue<T>(itr->second);
    validator(valCopy);
#if SEN_GCC_VERSION_CHECK_SMALLER(12, 0, 0)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wstringop-overflow"
    // Misdetection for wrong assignment
    val = valCopy;
#  pragma GCC diagnostic pop
#else
    val = valCopy;
#endif
  }
}

}  // namespace sen::impl

#endif  // SEN_CORE_OBJ_DETAIL_NATIVE_OBJECT_IMPL_H
