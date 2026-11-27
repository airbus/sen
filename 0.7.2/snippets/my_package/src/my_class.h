#pragma once

#include "stl/my_package/my_class.stl.h"

// sen
#include "sen/kernel/component_api.h"

namespace my_package
{

class MyClassImpl: public MyClassBase
{
public:
  SEN_NOCOPY_NOMOVE(MyClassImpl)

public:
  MyClassImpl(const std::string& name, const sen::VarMap& args);
  ~MyClassImpl() override = default;

public:
  void update(sen::kernel::RunApi& runApi) override;

protected:
  int32_t addNumbersImpl(int32_t a, int32_t b) override;
  std::string echoImpl(const std::string& message) override;
  void changePropsImpl() override;
};

}  // namespace my_package
