#ifndef SEN_APPS_CLI_PACKAGE_ARCHIVE_SETUP_H
#define SEN_APPS_CLI_PACKAGE_ARCHIVE_SETUP_H

// sen
#include "sen/core/base/duration.h"
#include "sen/core/base/numbers.h"

// cli11
#include <CLI/App.hpp>
#include <CLI/CLI.hpp>  // NOLINT (misc-include-cleaner): to correctly link

float64_t toSeconds(const sen::Duration& duration);

void setupInfo(CLI::App& app);

void setupIndexed(CLI::App& app);

void setupMerger(CLI::App& app);

#endif  // SEN_APPS_CLI_PACKAGE_ARCHIVE_SETUP_H
