// === shell.h =========================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_COMPONENTS_SHELL_SRC_SHELL_SERVICE_SHELL_H
#define SEN_COMPONENTS_SHELL_SRC_SHELL_SERVICE_SHELL_H

#include "line_edit.h"
#include "printer.h"
#include "terminal.h"

// sen
#include "sen/core/meta/class_type.h"
#include "sen/core/meta/type.h"
#include "sen/core/obj/object_mux.h"

// generated code
#include "stl/shell.stl.h"

// kernel
#include "stl/sen/kernel/basic_types.stl.h"

namespace sen::components::shell
{

class ShellImpl: public ShellBase
{
  SEN_NOCOPY_NOMOVE(ShellImpl)

public:  // special members
  ShellImpl(const std::string& name,
            ::sen::kernel::RunApi& api,
            ObjectMux& mux,
            CustomTypeRegistry& types,
            const Configuration& config,
            Terminal* term);
  ~ShellImpl() override = default;

public:
  void update(kernel::RunApi& runApi) override;

protected:  // methods implementation
  void clearImpl() const override;
  void helpImpl() const override;
  void historyImpl() const override;
  void lsImpl() const override;
  void infoImpl(const std::string& term) const override;
  void openImpl(const std::string& source) override;
  void closeImpl(const std::string& source) override;
  void shutdownImpl() const override;
  void queryImpl(const std::string& name, const std::string& condition) override;
  void srcImpl() override;

protected:
  void onInput(std::string_view input, kernel::RunApi& api);

private:
  struct Call
  {
    std::string command;
    std::string methodName;
    std::string objectName;
    Object* object = nullptr;
    const Method* method = nullptr;
    bool hasWriterSchema = false;
    const Method* writerMethod = nullptr;
    VarList args;
  };

  struct ProviderData
  {
    std::shared_ptr<ObjectProvider> provider;
    std::shared_ptr<Interest> interest;
  };

  struct SourceData
  {
    std::shared_ptr<ObjectSource> source;
    std::map<std::string, ProviderData, std::less<>> providers;
  };

private:
  void execute(std::string_view command, kernel::RunApi& api);
  Call parseCommand(std::string_view cmd);
  void executeCall(const Call& call, const Method* method);
  void callWriterSchema(const Call& call);
  std::vector<std::string> fetchCurrentSources();
  void doOpen(std::string_view term);
  void doQuery(std::string_view name, std::string_view selection);
  [[nodiscard]] SourceData& getOrOpenSource(const kernel::BusAddress& address);
  [[nodiscard]] static MaybeConstTypeHandle<ClassType> getWriterSchema(const Object* object) noexcept;

private:
  kernel::RunApi& api_;
  ObjectMux& mux_;
  CustomTypeRegistry& types_;
  Terminal* term_;
  std::unique_ptr<LineEdit> lineEdit_;
  std::unique_ptr<ObjectList<Object>> objects_;
  Printer printer_;
  std::unordered_map<std::string, std::shared_ptr<kernel::SessionInfoProvider>> openSessions_;
  std::unordered_map<std::string, SourceData> openSources_;
  std::atomic_bool printDetectedObjects_ = false;
  ::sen::Subscription<::sen::Object> logMasterSub_;
};

}  // namespace sen::components::shell

#endif  // SEN_COMPONENTS_SHELL_SRC_SHELL_SERVICE_SHELL_H
