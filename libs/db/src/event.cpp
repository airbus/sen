// === event.cpp =======================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "sen/db/event.h"

// sen
#include "sen/core/base/span.h"
#include "sen/core/io/input_stream.h"
#include "sen/core/io/util.h"
#include "sen/core/meta/event.h"
#include "sen/core/meta/var.h"
#include "sen/core/obj/object.h"

// std
#include <cstdint>
#include <utility>
#include <vector>

namespace sen::db
{

Event::Event(ObjectId objectId, const ::sen::Event* event, std::vector<uint8_t> buffer)
  : objectId_(objectId), event_(event), buffer_(std::move(buffer))
{
}

ObjectId Event::getObjectId() const noexcept { return objectId_; }

const ::sen::Event* Event::getEvent() const noexcept { return event_; }

const std::vector<Var>& Event::getArgsAsVariants() const
{
  if (argsExtracted_)
  {
    return args_;
  }

  // do not attempt to work if there are no arguments to work with
  if (const auto& eventArgs = event_->getArgs(); !eventArgs.empty())
  {
    // reserve all args in one shot
    args_.reserve(eventArgs.size());

    // read the args from the buffer
    InputStream in(buffer_);
    for (const auto& arg: eventArgs)
    {
      Var argVar;
      ::sen::impl::readFromStream(argVar, in, *arg.type);
      args_.push_back(std::move(argVar));
    }
  }

  argsExtracted_ = true;
  return args_;
}

Span<const uint8_t> Event::getArgsAsBuffer() const noexcept { return buffer_; }

}  // namespace sen::db
