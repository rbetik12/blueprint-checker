#pragma once
#include <string>

bool RunSingleMode(const std::string& PathToBlueprintFileStr);
bool RunBatchMode(const std::string& PathToBatchFileStr);
bool RunStdInMode();

bool ParseBlueprint(const std::string& BlueprintFilepathStr);
