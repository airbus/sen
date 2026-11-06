// === main.cpp ========================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// sen
#include "sen/core/base/class_helpers.h"
#include "sen/core/meta/type_registry.h"
#include "sen/core/obj/object.h"
#include "sen/db/creation.h"
#include "sen/db/deletion.h"
#include "sen/db/event.h"
#include "sen/db/input.h"
#include "sen/db/keyframe.h"
#include "sen/db/property_change.h"

// std
#include <algorithm>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <string>
#include <utility>
#include <variant>

int main(int argc, char* argv[])
{
  if (argc != 2)
  {
    std::cerr << "expecting recording path" << std::endl;
    return EXIT_FAILURE;
  }

  try
  {
    // open the recording
    sen::CustomTypeRegistry reg;
    auto input = sen::db::Input(argv[1], reg);  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)

    // print the summary
    std::cout << "summary:\n" << input.getSummary() << "\n";

    // helpers
    auto objs = input.getObjectIndexDefinitions();
    auto findObj = [&](sen::ObjectId id) -> const sen::db::ObjectIndexDef&
    { return *std::find_if(objs.begin(), objs.end(), [&](auto& e) { return id == e.objectId; }); };

    // iterate over the entries
    for (auto cursor = input.begin(); !cursor.atEnd(); ++cursor)
    {
      auto time = cursor.get().time;

      // print things depending on each entry
      std::visit(sen::Overloaded {[&](const sen::db::PropertyChange& entry)
                                  {
                                    std::cout
                                      << "- " << time.toLocalString() << ": " << findObj(entry.getObjectId()).name
                                      << " " << entry.getProperty()->getName()
                                      << " changed: " << entry.getValueAsVariant().getCopyAs<std::string>() << "\n";
                                  },
                                  [&](const sen::db::Event& entry)
                                  {
                                    std::cout << "- " << time.toLocalString() << ": "
                                              << findObj(entry.getObjectId()).name << " " << entry.getEvent()->getName()
                                              << " emitted\n";
                                  },
                                  [&](const sen::db::Creation& entry)
                                  {
                                    std::cout << "- " << time.toLocalString() << ": "
                                              << findObj(entry.getSnapshot().getObjectId()).name << " created\n";
                                  },
                                  [&](const sen::db::Deletion& entry)
                                  {
                                    std::cout << "- " << time.toLocalString() << ": deleted object "
                                              << findObj(entry.getObjectId()).name << "\n";
                                  },
                                  [&](const sen::db::Keyframe& entry)
                                  {
                                    std::cout << "- " << time.toLocalString() << ": keyframe\n";
                                    for (const auto& elem: entry.getSnapshots())
                                    {
                                      std::cout << "  - " << findObj(elem.getObjectId()).name << "\n";
                                    }
                                  },
                                  [&](const sen::db::End& entry)
                                  {
                                    std::ignore = entry;
                                    std::cout << time.toLocalString() << ": end of file\n";
                                  },
                                  [&](const auto&) {}},
                 cursor.get().payload);
    }
  }
  catch (const std::exception& err)
  {
    std::cerr << "error: " << err.what() << std::endl;
    return EXIT_FAILURE;
  }
  catch (...)
  {
    std::cerr << "unexpected error" << std::endl;
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
