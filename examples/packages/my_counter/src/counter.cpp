#include "counter.h"

namespace my_counter
{

void CounterImpl::update(sen::kernel::RunApi& /*runApi*/)
{
  setNextValue(getValue() + getStep());  // (1)!

  if (getNextValue() % 10 == 0)
  {
    valueIsDivisibleByTen(getNextValue());  // (2)!
  }
}

std::string CounterImpl::helloImpl() const
{
  return "Hello from Sen! My current value is: " + std::to_string(getValue());
}

// (3)!
SEN_EXPORT_CLASS(CounterImpl)

}  // namespace my_counter
