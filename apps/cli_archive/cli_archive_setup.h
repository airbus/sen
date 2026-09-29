#ifndef SEN_APPS_CLI_ARCHIVE_CLI_ARCHIVE_SETUP_H
#define SEN_APPS_CLI_ARCHIVE_CLI_ARCHIVE_SETUP_H

// cli11
#include <CLI/App.hpp>

void setupInfo(CLI::App& app);

void setupIndexed(CLI::App& app);

void setupMerger(CLI::App& app);

#endif  // SEN_APPS_CLI_ARCHIVE_CLI_ARCHIVE_SETUP_H
