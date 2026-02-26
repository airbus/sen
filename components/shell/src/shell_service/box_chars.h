// === box_chars.h =====================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_COMPONENTS_SHELL_SRC_SHELL_SERVICE_BOX_CHARS_H
#define SEN_COMPONENTS_SHELL_SRC_SHELL_SERVICE_BOX_CHARS_H

#ifdef _WIN32
// Windows code page 437 box drawing characters
constexpr auto* boxDlr = "\315";     // NOLINT
constexpr auto* boxDud = "\272";     // NOLINT
constexpr auto* boxDul = "\274";     // NOLINT
constexpr auto* boxDur = "\310";     // NOLINT
constexpr auto* boxDdl = "\273";     // NOLINT
constexpr auto* boxDdr = "\311";     // NOLINT
constexpr auto* boxDudl = "\271";    // NOLINT
constexpr auto* boxDudr = "\314";    // NOLINT
constexpr auto* boxDulr = "\312";    // NOLINT
constexpr auto* boxDdlr = "\313";    // NOLINT
constexpr auto* boxDudlr = "\316";   // NOLINT
constexpr auto* boxDuSl = "\275";    // NOLINT, not in CP850
constexpr auto* boxDuSr = "\323";    // NOLINT, not in CP850
constexpr auto* boxDdSl = "\267";    // NOLINT, not in CP850
constexpr auto* boxDdSr = "\326";    // NOLINT, not in CP850
constexpr auto* boxDlSu = "\276";    // NOLINT, not in CP850
constexpr auto* boxDlSd = "\270";    // NOLINT, not in CP850
constexpr auto* boxDrSu = "\324";    // NOLINT, not in CP850
constexpr auto* boxDrSd = "\325";    // NOLINT, not in CP850
constexpr auto* boxDuSlr = "\320";   // NOLINT, not in CP850
constexpr auto* boxDdSlr = "\322";   // NOLINT, not in CP850
constexpr auto* boxDlSud = "\265";   // NOLINT, not in CP850
constexpr auto* boxDrSud = "\306";   // NOLINT, not in CP850
constexpr auto* boxDlrSu = "\317";   // NOLINT, not in CP850
constexpr auto* boxDlrSd = "\321";   // NOLINT, not in CP850
constexpr auto* boxDlrSud = "\330";  // NOLINT, not in CP850
constexpr auto* boxDudSl = "\266";   // NOLINT, not in CP850
constexpr auto* boxDudSr = "\307";   // NOLINT, not in CP850
constexpr auto* boxDudSlr = "\327";  // NOLINT, not in CP850
constexpr auto* boxSlr = "\304";     // NOLINT
constexpr auto* boxSud = "\263";     // NOLINT
constexpr auto* boxSul = "\331";     // NOLINT
constexpr auto* boxSur = "\300";     // NOLINT
constexpr auto* boxSdl = "\277";     // NOLINT
constexpr auto* boxSdr = "\332";     // NOLINT
constexpr auto* boxSulr = "\301";    // NOLINT
constexpr auto* boxSdlr = "\302";    // NOLINT
constexpr auto* boxSudl = "\264";    // NOLINT
constexpr auto* boxSudr = "\303";    // NOLINT
constexpr auto* boxSudlr = "\305";   // NOLINT
constexpr auto* boxBox = "\334";     // NOLINT
#else
constexpr auto* boxDlr = "\u2550";     // NOLINT
constexpr auto* boxDud = "\u2551";     // NOLINT
constexpr auto* boxDul = "\u255d";     // NOLINT
constexpr auto* boxDur = "\u255a";     // NOLINT
constexpr auto* boxDdl = "\u2557";     // NOLINT
constexpr auto* boxDdr = "\u2554";     // NOLINT
constexpr auto* boxDudl = "\u2563";    // NOLINT
constexpr auto* boxDudr = "\u2560";    // NOLINT
constexpr auto* boxDulr = "\u2569";    // NOLINT
constexpr auto* boxDdlr = "\u2555";    // NOLINT
constexpr auto* boxDudlr = "\u256c";   // NOLINT
constexpr auto* boxDuSl = "\u255c";    // NOLINT
constexpr auto* boxDuSr = "\u2559";    // NOLINT
constexpr auto* boxDdSl = "\u2556";    // NOLINT
constexpr auto* boxDdSr = "\u2553";    // NOLINT
constexpr auto* boxDlSu = "\u255b";    // NOLINT
constexpr auto* boxDlSd = "\u2555";    // NOLINT
constexpr auto* boxDrSu = "\u2559";    // NOLINT
constexpr auto* boxDrSd = "\u2552";    // NOLINT
constexpr auto* boxDuSlr = "\u2568";   // NOLINT
constexpr auto* boxDdSlr = "\u2565";   // NOLINT
constexpr auto* boxDlSud = "\u2561";   // NOLINT
constexpr auto* boxDrSud = "\u255e";   // NOLINT
constexpr auto* boxDlrSu = "\u2567";   // NOLINT
constexpr auto* boxDlrSd = "\u2564";   // NOLINT
constexpr auto* boxDlrSud = "\u256a";  // NOLINT
constexpr auto* boxDudSl = "\u2562";   // NOLINT
constexpr auto* boxDudSr = "\u255f";   // NOLINT
constexpr auto* boxDudSlr = "\u256b";  // NOLINT
constexpr auto* boxSlr = "\u2500";     // NOLINT
constexpr auto* boxSud = "\u2502";     // NOLINT
constexpr auto* boxSul = "\u2518";     // NOLINT
constexpr auto* boxSur = "\u2514";     // NOLINT
constexpr auto* boxSdl = "\u2510";     // NOLINT
constexpr auto* boxSdr = "\u250c";     // NOLINT
constexpr auto* boxSulr = "\u2534";    // NOLINT
constexpr auto* boxSdlr = "\u252c";    // NOLINT
constexpr auto* boxSudl = "\u2524";    // NOLINT
constexpr auto* boxSudr = "\u251c";    // NOLINT
constexpr auto* boxSudlr = "\u2540";   // NOLINT
constexpr auto* boxBox = "\u2584";     // NOLINT

#endif  // SEN_COMPONENTS_SHELL_SRC_SHELL_SERVICE_BOX_CHARS_H
#endif
