// === completer.h =====================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_COMPONENTS_SHELL_SRC_SHELL_SERVICE_COMPLETER_H
#define SEN_COMPONENTS_SHELL_SRC_SHELL_SERVICE_COMPLETER_H

#include "printer.h"
#include "sen/core/meta/class_type.h"
#include "sen/core/obj/object.h"
#include "sen/core/obj/object_list.h"

#include <list>
#include <vector>

namespace sen::components::shell
{

class Completion final
{
public:
  Completion();
  ~Completion() = default;

public:
  SEN_COPY_ASSIGN(Completion) = default;
  SEN_COPY_CONSTRUCT(Completion) = default;
  SEN_MOVE_ASSIGN(Completion) = default;
  SEN_MOVE_CONSTRUCT(Completion) = default;

public:
  void configureAsObject();
  void configureAsMethod();
  void configureAsFakeMethod();
  void configureAsGroup();
  void set(std::string str) noexcept;

public:
  [[nodiscard]] const std::string& get() const noexcept;
  [[nodiscard]] Style getStyle() const noexcept;

private:
  std::string str_;
  Style style_;
};

using CompletionList = std::list<Completion>;
using SourcesFetcher = std::function<std::vector<std::string>()>;

class Completer final
{
  SEN_NOCOPY_NOMOVE(Completer)

public:
  explicit Completer(const ObjectList<Object>* objects) noexcept;
  ~Completer() = default;

public:
  void getCompletions(const std::string& buffer,
                      ::sen::kernel::RunApi& api,
                      SourcesFetcher& sourcesFetcher,
                      CompletionList& list,
                      std::string& commonPrefix) const;

private:
  const ObjectList<Object>* objects_;
};

}  // namespace sen::components::shell

#endif  // SEN_COMPONENTS_SHELL_SRC_SHELL_SERVICE_COMPLETER_H
