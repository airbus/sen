// === reader_writer.h =================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_LIBS_CORE_TEST_SUPPORT_READER_WRITER_H
#define SEN_LIBS_CORE_TEST_SUPPORT_READER_WRITER_H

#include "sen/core/base/span.h"
#include "sen/core/io/buffer_writer.h"

// std
#include <cstring>
#include <vector>

namespace sen::test
{

class TestWriter: public Writer
{
  SEN_NOCOPY_NOMOVE(TestWriter)

public:
  inline TestWriter() noexcept = default;
  ~TestWriter() noexcept override = default;

public:
  Span<uint8_t> loanBuffer(size_t size)
  {
    buffer_.resize(buffer_.size() + size);
    return makeSpan(&buffer_[cursor_], size);
  }

  void returnBuffer(Span<uint8_t> buffer)
  {
    writeCount_++;
    cursor_ += buffer.size();
  }

  std::vector<uint8_t>& getBuffer() noexcept { return buffer_; }

  [[nodiscard]] std::size_t getWriteCount() const noexcept { return writeCount_; }

  uint8_t* advance(size_t size) override
  {
    const auto cursor = buffer_.size();
    buffer_.resize(buffer_.size() + size);
    return std::next(buffer_.data(), static_cast<int>(cursor));
  };

  void reverse(size_t size) override { buffer_.resize(buffer_.size() - size); };

private:
  std::vector<uint8_t> buffer_;
  std::size_t cursor_ = 0U;
  std::size_t writeCount_ = 0U;
};

class TestReader  //: public Reader
{
  SEN_NOCOPY_NOMOVE(TestReader)

public:
  inline explicit TestReader(const std::vector<uint8_t>& buffer) noexcept: buffer_(buffer) {}

  virtual ~TestReader() noexcept = default;

public:
  Span<const uint8_t> read(size_t size)
  {
    readCount_++;

    auto nextCursor = cursor_ + size;
    if (nextCursor > buffer_.size())
    {
      sen::throwRuntimeError("input buffer underflow");
    }

    auto ptr = &buffer_[cursor_];
    cursor_ = nextCursor;
    return makeConstSpan(ptr, size);
  }

  const std::vector<uint8_t>& getBuffer() noexcept { return buffer_; }

  [[nodiscard]] std::size_t getReadCount() const noexcept { return readCount_; }

private:
  const std::vector<uint8_t>& buffer_;
  std::size_t cursor_ = 0U;
  std::size_t readCount_ = 0U;
};

class BufferedTestReader: public TestReader
{
  SEN_NOCOPY_NOMOVE(BufferedTestReader);

public:
  inline explicit BufferedTestReader() noexcept: TestReader(buffer_) {}

  ~BufferedTestReader() noexcept override = default;

  std::vector<uint8_t>& getBuffer() noexcept { return buffer_; }

  [[nodiscard]] const std::vector<uint8_t>& getBuffer() const noexcept { return buffer_; }

private:
  std::vector<uint8_t> buffer_;
};

template <typename T>
std::vector<uint8_t> valueToBytes(const T data)
{
  std::vector<uint8_t> result;

  if constexpr (std::is_same_v<T, std::string>)
  {
    if (!data.empty())
    {  // otherwise, we try to memcpy a nullptr
      result.resize(data.size());
      std::memcpy(result.data(), data.data(), data.size());
    }
  }
  else
  {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    const auto* dataPtr = reinterpret_cast<const uint8_t*>(&data);  // NOSONAR
    for (std::size_t i = 0U; i < sizeof(T); ++i)
    {
      result.push_back(*(std::next(dataPtr, static_cast<int>(i))));
    }
  }

  return result;
}

}  // namespace sen::test

#endif  // SEN_LIBS_CORE_TEST_SUPPORT_READER_WRITER_H
