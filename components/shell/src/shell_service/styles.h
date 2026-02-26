// === styles.h ========================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_COMPONENTS_SHELL_SRC_SHELL_SERVICE_STYLES_H
#define SEN_COMPONENTS_SHELL_SRC_SHELL_SERVICE_STYLES_H

#include "terminal.h"

namespace sen::components::shell
{

// line editing
constexpr Style promptHeaderStyle {TextFlags::defaultText, 0xfdbe83};
constexpr Style promptSeparatorStyle {TextFlags::defaultText, 0x57a64a};
constexpr Style promptHostStyle {TextFlags::italicText, 0xa0a0a0};
constexpr Style promptAppNameStyle {TextFlags::italicText, 0x569cd6};
constexpr Style defaultBufferStyle {TextFlags::defaultText, 0xc8c8c8};
constexpr Style highlightBufferStyle {TextFlags::boldText | TextFlags::italicText, 0xc8c8c8};

// general
constexpr Style emptyValueStyle {TextFlags::defaultText, 0xaaaaaa};
constexpr Style stringValueStyle {TextFlags::defaultText, 0xd69d85};
constexpr Style enumValueStyle {TextFlags::defaultText, 0xb8d7a3};
constexpr Style numberValueStyle {TextFlags::defaultText, 0xb5cEa8};
constexpr Style listIndexStyle {TextFlags::defaultText, 0x57a64a};
constexpr Style descriptionStyle {TextFlags::defaultText, 0xa0a0a0};
constexpr Style closedSourceStyle {TextFlags::defaultText, 0xa0a0a0};
constexpr Style titleStyle {TextFlags::underlineText, 0xffffff};
constexpr Style informationStyle {TextFlags::defaultText, 0xa0a0a0};
constexpr Style errorStyle {TextFlags::boldText, 0xff0202};
constexpr Style errorMessageStyle {TextFlags::defaultText, 0xff6666};
constexpr Style typenameStyle {TextFlags::italicText, 0x4ec9b0};
constexpr Style cellStyle {TextFlags::defaultText, 0xdcedc1};
constexpr Style helpStyle {TextFlags::defaultText, 0x57a64a};
constexpr Style quoteStyle {TextFlags::italicText, 0xd69d85};
constexpr Style authorStyle {TextFlags::boldText, 0x57a64a};

// tree rendering
constexpr Style inlineDocStyle {TextFlags::defaultText, 0xdddddd};
constexpr Style leafNodeStyle {TextFlags::boldText, 0xa8e6cf};
constexpr Style treeBranchesStyle {TextFlags::boldText, 0x6f7c85};
constexpr Style sessionStyle {TextFlags::boldText, 0xdfe0ab};
constexpr Style busStyle {TextFlags::boldText, 0x61b545};
constexpr Style groupStyle {TextFlags::defaultText, 0x42a3c7};

// completions
constexpr Style completionStyle {TextFlags::defaultText, 0x569cd6};
constexpr Style objectCompletionStyle {TextFlags::defaultText, 0x569cd6};
constexpr Style methodCompletionStyle {TextFlags::italicText, 0x569cd6};
constexpr Style groupCompletionStyle {TextFlags::defaultText, 0x2b91af};
constexpr Style fakeMethodCompletionStyle {TextFlags::italicText, 0x2b91af};

// logo/banner
constexpr Style versionStyle {TextFlags::boldText, 0xffffff};
constexpr Style buildInfoStyle {TextFlags::defaultText, 0x909090};

}  // namespace sen::components::shell

#endif  // SEN_COMPONENTS_SHELL_SRC_SHELL_SERVICE_STYLES_H
