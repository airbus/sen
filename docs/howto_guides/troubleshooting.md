# Troubleshooting

This page covers the most common problems encountered when starting with Sen, in a symptom → cause →
fix format. For a deeper understanding of why things work the way they do, see
[Understanding Sen: A Mental Model](../users_guide/mental_model.md) and
[Working with Objects](objects.md).

---

## Quick diagnostics checklist

Before diving into specific problems, run through this list:

1. Is the Sen environment sourced? (`sen --version` should print a version number)
2. Is `LD_LIBRARY_PATH` set to include your build output? (e.g. `$(pwd)/build/bin`)
3. Did CMake run successfully and did `make` (or similar) complete without errors?
4. Does your YAML config reference the correct class names (case-sensitive, `package.ClassName`)?
5. Do all bus names in YAML exactly match what your code uses?
6. Does every exported class have `SEN_EXPORT_CLASS(ClassName)` at the bottom of its `.cpp`?

---

## Build and environment

### Package fails to load at runtime — `dlopen` error or `cannot open shared object file`

**Symptom:** Sen starts but immediately prints something like:

```
error: cannot open shared object file: libmy_package.so: No such file or directory
```

**Cause:** The shared library was built but the runtime linker cannot find it. `LD_LIBRARY_PATH`
does not include your build output directory.

**Fix:**

```sh
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$(pwd)/build/bin
```

Add this to your shell profile or a project-local `setup.sh` script so you do not have to run it
every time.

!!! tip
    Run `ldd build/bin/libmy_package.so` to verify all transitive dependencies are also found.

---

### Generated `.stl.h` files are missing or stale

**Symptom:** Build fails with `#include "stl/my_package/my_class.stl.h": No such file or directory`,
or you added a property in your STL file but it does not appear in the C++ class.

**Cause:** The code generator has not been run since the STL file was created or modified.

**Fix:** The generator runs automatically as part of the CMake build if you used `add_sen_package()`
correctly. Run a full clean build:

```sh
cmake -S . -B build
cmake --build build
```

If the file is still missing, check that your STL files are listed in the `STL_FILES` argument of
`add_sen_package()` in your `CMakeLists.txt`:

```cmake
add_sen_package(
    TARGET my_package
    ...
    STL_FILES
        stl/my_package/my_class.stl   # ← must be listed here
)
```

---

## Object discovery

### Object never appears in the shell / subscription is always empty

**Symptom:** You run `ls` in the shell and see nothing, or your `Subscription<T>` list
is always empty even though the component started successfully.

**Cause (most common):** Bus name mismatch. The object is published to a different bus than the one
you are watching.

**Fix:** Check that the bus name in your YAML config exactly matches what you use in code and in
the shell. Bus names are case-sensitive:

```yaml
# YAML config
objects:
  - class: my_package.MyClass
    name: obj1
    bus: local.my_bus   # ← this must exactly match
```

```cpp
// In registered():
subscription_.source = api.getSource("local.my_bus");  // ← must be identical
```

---

### Object appears, then immediately disappears

**Symptom:** The object briefly shows up but vanishes within a cycle or two.

**Cause 1:** The object is being destroyed before the component finishes registering — typically
because it is a local variable that goes out of scope.

**Fix:** Objects must be owned by the component (stored as `shared_ptr` members), not as locals.

**Cause 2:** `Subscription<T>` is a local variable. When it goes out of scope, it tears down the
subscription, which can cascade into the object being removed.

**Fix:** Make `Subscription<T>` a member variable of your class:

```cpp
class MyManagerImpl : public MyManagerBase
{
    // ✅ correct — member variable, lives as long as the object
    sen::Subscription<WorkerInterface> workers_;

    // ❌ wrong — local in registered(), destroyed immediately
  void registered(sen::kernel::RegistrationApi& api) override {
    sen::Subscription<WorkerInterface> workers;
  }
};
```

---

### Two objects with the same name

**Symptom:** You instantiate two objects but only one appears, or behavior is inconsistent.

**Cause:** Object names must be unique within a bus. If two objects share a name, one will be rejected.

**Fix:** Ensure each object has a distinct `name` in the YAML config:

```yaml
objects:
  - class: my_package.Worker
    name: worker1   # unique
    bus: local.workers
  - class: my_package.Worker
    name: worker2   # unique
    bus: local.workers
```

---

## Properties and state

### Property changes are not visible to other objects

**Symptom:** You update a property in your `update()` function but other components always see the
old value.

**Cause 1:** You are assigning to a local variable or calling a non-staging setter, instead of
using `setNextProp()`.

**Fix:** Always use the generated `setNext` accessor to stage property changes:

```cpp
// ✅ correct — stages the change for commit
setNextPosition(newPosition);

// ❌ wrong — this does nothing to the Sen property
position_ = newPosition;
```

**Cause 2:** You are calling `setNextProp()` outside of the `update()` or drain callback context
(e.g., from a constructor or a non-Sen thread).

**Fix:** All property changes must happen during the update or drain stage. Initialize values using
the YAML config (`prop_name: initial_value`) or in `registered()` via the appropriate API.

---

### Startup crash: "static property has no initial value"

**Symptom:** The kernel crashes or prints an error at startup referencing a `[static]` property.

**Cause:** Static properties require an initial value at construction time, and none was provided in
the YAML config.

**Fix:** Add the missing initial value to the object's YAML entry:

```yaml
objects:
  - class: my_package.MyClass
    name: obj1
    myStaticProp: "required value"   # ← add this
    bus: local.example
```

The YAML key is the property name as defined in STL.

---

### Method callback fires much later than expected

**Symptom:** The result arrives several cycles after the call, not on the very next one.

**Cause:** This is normal behavior under load. If the target component is running at a low
frequency (e.g. 2 Hz) and the call is made just after its drain, the next drain is almost a full
period away.

**Fix:** Increase the component frequency if lower latency is required, or accept the asynchronous
nature of the system. This is not a bug.

---

### Exception from a method is silently ignored

**Symptom:** Your method implementation throws, but the caller never sees the error.

**Cause:** The `MethodResult<R>` returned to the callback contains the exception, but the callback
does not check for it.

**Fix:** Always check `isOk()` before calling `get()`:

```cpp
calc.divide(10.0, 0.0, senImplAsyncCall([](sen::MethodResult<double> r) {
  if (r.isOk())
  {
    std::cout << "result: " << r.getValue() << std::endl;
  }
  else
  {
    try
    {
      std::rethrow_exception(r.getError());
    }
    catch (const std::exception& e)
    {
      std::cout << "error: " << e.what() << std::endl;
    }
  }
}));
```

---

## Configuration

### Component starts before its dependency is ready

**Symptom:** A component tries to find objects from another component but finds nothing on the first
few cycles. Or the kernel logs errors about missing objects during initialization.

**Cause:** Both components are in the same group, so their startup order is not guaranteed. Or the
dependent component is in a *lower* group number than its dependency.

**Fix:** Put dependencies in **lower** group numbers. Higher groups start after all lower groups
are fully running:

```yaml
build:
  - name: my_data_source
    group: 2    # starts first

  - name: my_consumer_component
    group: 3    # starts after group 2 is fully up
```

See [Using component groups](using_groups.md) for the full rules.

---

### Class not found by kernel — "unknown class" error

**Symptom:** The kernel prints something like `unknown class: my_package.MyClassImpl` at startup.

**Cause 1:** `SEN_EXPORT_CLASS(MyClassImpl)` is missing from the `.cpp` file.

**Fix:** Add the macro at the bottom of the implementation file:

```cpp
// my_class_impl.cpp
...

SEN_EXPORT_CLASS(MyClassImpl)  // ← must be present
```

**Cause 2:** The package is not listed in the `imports` of the component that tries to instantiate
the class.

**Fix:** Add it to the `imports` list in your YAML config (see the discovery section above).

**Cause 3:** The class name in YAML uses the wrong case or wrong separator. The format is always
`package_name.ClassName` — the package name from your STL `package` declaration, dot, then the
**C++** class name.

---

### YAML config error: unexpected key or wrong type

**Symptom:** The kernel refuses to start with a YAML parse error or a type mismatch.

**Cause:** A property name is misspelled, or the value type does not match the STL declaration (e.g.
a string where a number is expected).

**Fix:** Check that:
- The property key in YAML matches the STL property name exactly (case-sensitive)
- Numeric properties use numbers in YAML, not quoted strings
- `[static]` properties that are required must be present

---

## Networking

### Objects not discovered across processes

**Symptom:** You run two `sen` processes on the same machine (or different machines) but their
objects do not see each other.

**Cause:** The `ether` component (Ethernet transport) is not loaded in one or both processes.

**Fix:** Add the `ether` component to both process configs and configure matching addresses. See
the [Ether component documentation](../components/ether.md) for the full configuration reference.

### WSL2 networking issues

Multi-process or multi-machine discovery can fail on WSL2 due to how it handles multicast. See the
[FAQ](../users_guide/faq.md) for WSL2-specific networking guidance.
