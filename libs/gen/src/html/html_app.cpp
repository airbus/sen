// === html_app.cpp ====================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "html/html_app.h"

// generated code
#include "html_app/explorer.h"
#include "html_app/logo.h"
#include "html_app/shell.h"
#include "html_app/styles.h"

// sen
#include "sen/core/base/hash32.h"

namespace sen::gen::detail
{

HtmlApp makeHtmlApp()
{
  HtmlApp result;
  result.shell = sen::decompressSymbolToString(shell, shellSize);
  result.styles = sen::decompressSymbolToString(styles, stylesSize);
  result.script = sen::decompressSymbolToString(explorer, explorerSize);
  result.logo = sen::decompressSymbolToString(logo, logoSize);
  return result;
}

}  // namespace sen::gen::detail
