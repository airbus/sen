// === util.h ==========================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// sen
#include "sen/core/base/memory_block.h"
#include "sen/core/base/span.h"
#include "sen/core/io/input_stream.h"
#include "sen/kernel/type_specs_utils.h"
#include "v1.stl.h"

// spdlog
#include <spdlog/logger.h>

// std
#include <cstdint>
#include <filesystem>

namespace sen::db
{

std::shared_ptr<spdlog::logger> getLogger();

[[nodiscard]] inline uint32_t getFileHeaderSize();

template <typename T, typename B>
void inline writeToBuffer(const T& data, B& buffer);

template <typename T>
inline void write(T&& data, FILE* file);

template <typename T>
[[nodiscard]] inline std::shared_ptr<ResizableHeapBlock> writeSizeAndDataToBuffer(const T& data);

void writeToCompressedBuffer(const std::vector<uint8_t>& memBuf, kernel::Buffer& lz4Buf);

void uncompressBuffer(const std::vector<uint8_t>& lz4Buf, std::vector<uint8_t>& memBuf, std::size_t decompressedSize);

[[nodiscard]] FILE* openFile(const std::filesystem::path& path);

/// Returns the class in the hierarchy of classes that owns the property. Returns nullptr if not found
[[nodiscard]] const ::sen::ClassType* searchOwner(const ::sen::ClassType* classType,
                                                  const ::sen::Property* property) noexcept;

/// Returns the class in the hierarchy of classes to which the event is mapped. Returns nullptr if not found
[[nodiscard]] const ::sen::ClassType* searchOwner(const ::sen::ClassType* classType,
                                                  const ::sen::Event* event) noexcept;

/// Computes the old version (platform-dependent) of the property id. Used for retro-compatibility.
[[nodiscard]] MemberHash computePlatformDependentPropertyId(const ::sen::ClassType* classType,
                                                            const ::sen::Property* property);

/// Computes the old version (platform-dependent) of the event id. Used for retro-compatibility.
[[nodiscard]] MemberHash computePlatformDependentEventId(const ::sen::ClassType* classType, const ::sen::Event* event);

//----------------------------------------------------------------------------------------------------------------------
// Inline implementation
//----------------------------------------------------------------------------------------------------------------------

void doWrite(const Span<const uint8_t>& span, FILE* file);

inline uint32_t getFileHeaderSize()
{
  return SerializationTraits<v1::Magic>::serializedSize({}) + SerializationTraits<v1::FileHeader>::serializedSize({});
}

template <typename T>
inline void write(T&& data, FILE* file)
{
  auto block = std::make_shared<ResizableHeapBlock>();
  ResizableBufferWriter writer(*block);
  OutputStream out(writer);
  SerializationTraits<T>::write(out, data);
  doWrite(block->getConstSpan(), file);
}

template <typename T, typename B>
inline void writeToBuffer(const T& data, B& buffer)
{
  buffer.reserve(SerializationTraits<T>::serializedSize(data));
  ResizableBufferWriter writer(buffer);
  OutputStream out(writer);
  SerializationTraits<T>::write(out, data);
}

template <typename T>
inline std::shared_ptr<ResizableHeapBlock> writeSizeAndDataToBuffer(const T& data)
{
  uint32_t dataSize = SerializationTraits<T>::serializedSize(data);
  auto buffer = std::make_shared<ResizableHeapBlock>();

  // reserve space, include some for the size at the start
  buffer->reserve(SerializationTraits<uint32_t>::serializedSize(dataSize) + dataSize);
  ResizableBufferWriter writer(*buffer);
  OutputStream out(writer);

  out.writeUInt32(dataSize);
  SerializationTraits<T>::write(out, data);
  return buffer;
}

}  // namespace sen::db
