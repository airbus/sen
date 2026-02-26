// === memory_block.cpp ================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "sen/core/base/memory_block.h"

// std
#include <cstddef>
#include <stdexcept>
#include <string>
#include <tuple>

namespace sen
{

void FixedMemoryBlock::resize(std::size_t size)
{
  if (size > owner_->getBlockSize())
  {
    std::string err;
    err.append("invalid resize of ");
    err.append(std::to_string(size));
    err.append(" bytes for fixed memory pool with a block size of ");
    err.append(std::to_string(owner_->getBlockSize()));
    throw std::out_of_range(err);
  }
  size_ = size;
}

void FixedMemoryBlock::reserve(std::size_t size)  // NOSONAR
{
  if (size > owner_->getBlockSize())
  {
    std::string err;
    err.append("invalid reserve of ");
    err.append(std::to_string(size));
    err.append(" bytes for fixed memory pool with a block size of ");
    err.append(std::to_string(owner_->getBlockSize()));
    throw std::out_of_range(err);
  }

  std::ignore = size;  // already reserved the block
}

}  // namespace sen
