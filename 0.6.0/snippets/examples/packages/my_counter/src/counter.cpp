#include "counter.h"

namespace my_counter
{

void CounterImpl::update(sen::kernel::RunApi& /*runApi*/)
{
  setNextValue(getValue() + getStep());

  if (getNextValue() % 10 == 0)
  {
    valueIsDivisibleByTen(getNextValue());
  }
}

std::string CounterImpl::helloImpl() const
{
  return "Hello from Sen! My current value is: " + std::to_string(getValue());
}

SEN_EXPORT_CLASS(CounterImpl)

}  // namespace my_counter
