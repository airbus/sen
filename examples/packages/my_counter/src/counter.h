#pragma once

#include "stl/my_counter/counter.stl.h"

// sen
#include "sen/kernel/component_api.h"

namespace my_counter
{

class CounterImpl: public CounterBase
{
public:
  SEN_NOCOPY_NOMOVE(CounterImpl)

  using CounterBase::CounterBase;
  ~CounterImpl() override = default;

public:
  void update(sen::kernel::RunApi& runApi) override;

protected:
  std::string helloImpl() const override;
};

}  // namespace my_counter
