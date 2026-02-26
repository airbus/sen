// === confirmed_producer.cpp ==========================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// sen
#include "sen/core/base/compiler_macros.h"
#include "sen/core/base/numbers.h"
#include "sen/core/base/uuid.h"
#include "sen/core/meta/class_type.h"
#include "sen/kernel/component_api.h"

// generated code
#include "stl/stress_testing_utils/confirmed_producer.stl.h"

// spdlog
#include <spdlog/logger.h>
#include <spdlog/spdlog.h>

// std
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <random>
#include <string>
#include <utility>

namespace stress_testing_utils
{

class ConfirmedProducerImpl: public ConfirmedDataProducerBase
{
  SEN_NOCOPY_NOMOVE(ConfirmedProducerImpl)

public:
  using ConfirmedDataProducerBase::ConfirmedDataProducerBase;
  ~ConfirmedProducerImpl() override = default;

protected:
  void update(sen::kernel::RunApi& api) override
  {
    {
      auto prop = getProp1();
      modify(prop);
      setNextProp1(prop);
    }

    {
      auto prop = getProp2();
      modify(prop);
      setNextProp2(prop);
    }

    {
      auto prop = getProp3();
      modify(prop);
      setNextProp3(prop);
    }

    {
      auto prop = getCounter();
      modify(prop);
      setNextCounter(prop);
    }

    std::ignore = api;
  }

private:
  void modify(std::string& value) { value = uuidGen_().toString(); }

  void modify(float64_t& value) { value += 1.0; }

  void modify(uint64_t& value) { value += 1; }

  void modify(BigSequence& value)
  {
    std::random_device rd;                                                // obtain a random number from hardware
    std::mt19937 gen(rd());                                               // seed the generator
    std::uniform_int_distribution<> distribution(0, maxBigSequenceSize);  // define the range

    value.resize(distribution(gen));
    for (auto& elem: value)
    {
      modify(elem);
    }
  }

  void modify(SmallStruct& value)
  {
    modify(value.a);
    modify(value.b);
    modify(value.c);
    modify(value.d);
  }

  void modify(BigStruct& value)
  {
    modify(value.a);
    modify(value.b);
    modify(value.c);
    modify(value.d);
  }

private:
  std::shared_ptr<spdlog::logger> logger_ = spdlog::get("my_logger");
  sen::UuidRandomGenerator uuidGen_;
};

SEN_EXPORT_CLASS(ConfirmedProducerImpl)

}  // namespace stress_testing_utils
