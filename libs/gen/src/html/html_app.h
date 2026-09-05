// === html_app.h ======================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_LIBS_GEN_SRC_HTML_HTML_APP_H
#define SEN_LIBS_GEN_SRC_HTML_HTML_APP_H

// std
#include <string>

namespace sen::gen::detail
{

/// The application, baked into the binary. Unpacked once per generator.
struct HtmlApp
{
  std::string shell;
  std::string styles;
  std::string script;
  std::string logo;
};

HtmlApp makeHtmlApp();

}  // namespace sen::gen::detail

#endif  // SEN_LIBS_GEN_SRC_HTML_HTML_APP_H
