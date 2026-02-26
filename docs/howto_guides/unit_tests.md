# Unit Testing

Sen comes with a dedicated TestKernel and TestComponent to ease the integration into unit testing
frameworks (can be found here: libs/kernel/include/sen/kernel/test_kernel.h).

Some examples on the usage can be found directly in the unit tests Sen is using internally (see
libs/kernel/test/unit/test_kernel/test_kernel_test.cpp).

__HINT__: This type of testing is only suitable for simple tests which don't involve multiple
components with different execution rates or deferred methods.

## The double tick

In case you want your code to react to property-changed callbacks you need to make sure to call
`kernel.tick()` twice or directly add the tick count as parameter: `kernel.tick(2)`.

To explain this, we need to take a look in the ways of working of Sen.

![Screenshot](../assets/images/two_ticks_light.svg#only-light)
![Screenshot](../assets/images/two_ticks_dark.svg#only-dark)

The changed properties, which will be retrieved within the next cycles drain phase ❷, are collected
by Sen in the Commit phase ❶ of the current cycle.

When we push updates to the object within a unit-test, this happens in the time between the kernel
steps ❸, i.e. the time the kernel waits for the next `tick()`. As the queue of changed properties is
already created at this point of time, they require another `tick()` to be visible within the object
`getProperty()`.

## Example

```c++
/// Example of a google test for checking the property-change callbacks via the test kernel.
TEST(TestKernel, AircraftObject)
{
  auto aircraft = std::make_shared<rpr::AircraftBase<>>(
    "entityName", ::rpr::EntityTypeStruct(), rpr::EntityIdentifierStruct {}, ::rpr::EntityTypeStruct());

  sen::kernel::TestComponent component;

  uint16_t lastValue = 0;

  component.onInit(
    [&](sen::kernel::InitApi&& api) -> sen::kernel::PassResult
    {
      const auto source = api.getSource("local.test");
      source->add(aircraft);

      aircraft
        ->onAcousticSignatureIndexChanged(
          {api.getWorkQueue(), [&]() { lastValue = aircraft->getAcousticSignatureIndex(); }})
        .keep();

      return sen::kernel::done();
    });

  component.onRun([&](auto& api) { return api.execLoop(std::chrono::seconds(1)); });

  sen::kernel::TestKernel kernel(&component);

  aircraft->setNextAcousticSignatureIndex(1);
  kernel.step(2);
  EXPECT_EQ(1, lastValue);

  aircraft->setNextAcousticSignatureIndex(2);
  kernel.step(2);
  EXPECT_EQ(2, lastValue);

  aircraft->setNextAcousticSignatureIndex(3);
  kernel.step(2);
  EXPECT_EQ(3, lastValue);
}
```
