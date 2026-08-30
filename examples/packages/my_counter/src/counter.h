#pragma once

#include "stl/my_counter/counter.stl.h"  // (1)!

// sen
#include "sen/kernel/component_api.h"

namespace my_counter
{

class CounterImpl: public CounterBase  // (2)!
{
public:
  SEN_NOCOPY_NOMOVE(CounterImpl)  // (3)!

  using CounterBase::CounterBase;
  ~CounterImpl() override = default;

public:
  void update(sen::kernel::RunApi& runApi) override;  // (4)!

protected:
  std::string helloImpl() const override;  // (5)!
};

}  // namespace my_counter
