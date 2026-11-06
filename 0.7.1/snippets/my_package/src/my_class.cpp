#include "my_class.h"

// spdlog
#include <spdlog/spdlog.h>

namespace my_package
{

MyClassImpl::MyClassImpl(const std::string& name, const sen::VarMap& args)
  : MyClassBase(name, args)
{
  const auto logger = sen::kernel::KernelApi::getOrCreateLogger("my_package");
  logger->info("{} created", name);
}

void MyClassImpl::update(sen::kernel::RunApi& runApi)
{
  std::ignore = runApi;  // here goes your update logic
  setNextProp5(getProp5() + 1);
}

int32_t MyClassImpl::addNumbersImpl(int32_t a, int32_t b)
{
  return a + b;
}

std::string MyClassImpl::echoImpl(const std::string& message)
{
  return message;
}

void MyClassImpl::changePropsImpl()
{
  Vec2 val = getProp4();
  val.x += 0.5f;        // simply make some changes
  val.y += 1.5;         // to a property, to see the effect
  setNextProp4(val);
}

SEN_EXPORT_CLASS(MyClassImpl)

}  // namespace my_package
