#include "BlueprintChecker.h"
#include "FPlatformAgnosticChecker.h"
#include <iostream>
#include <fstream>

bool ParseBlueprint(const std::string& BlueprintFilepathStr)
{
	const size_t ArgStrLen = strlen(BlueprintFilepathStr.c_str());
	const FString& BlueprintPathStr = FString(ArgStrLen, BlueprintFilepathStr.c_str());
	const bool Result = FPlatformAgnosticChecker::Check(*BlueprintPathStr);
	if (Result)
	{
		std::wcout << "Parsed successfully!" << std::endl;
	}
	else
	{
		std::wcout << "Failed to parse!" << std::endl;
	}

	return Result;
}

bool RunSingleMode(const std::string& PathToBlueprintFileStr)
{
	FPlatformAgnosticChecker::Init();
	const bool Result = ParseBlueprint(PathToBlueprintFileStr);
	if (!Result)
	{
		std::cerr << "Can't serialize blueprint" << std::endl;
	}
	FPlatformAgnosticChecker::Exit();

	return Result;
}

bool RunBatchMode(const std::string& PathToBatchFileStr)
{
	std::ifstream FileStream(PathToBatchFileStr);

	if (!FileStream.good())
	{
		std::wcerr << "Can't open batch file!" << std::endl;
		FileStream.close();
		return false;
	}

	std::vector<std::string> BlueprintFilePaths;
	std::string Path;

	while (std::getline(FileStream, Path))
	{
		BlueprintFilePaths.push_back(Path);
	}

	if (BlueprintFilePaths.empty())
	{
		return false;
	}

	bool SuccessfulParsing = true;
	
	FPlatformAgnosticChecker::Init();
	for (auto& BlueprintPath : BlueprintFilePaths)
	{
		if (!ParseBlueprint(BlueprintPath))
		{
			SuccessfulParsing = false;
		}
	}
	FPlatformAgnosticChecker::Exit();

	return SuccessfulParsing;
}

bool RunStdInMode()
{
	FPlatformAgnosticChecker::Init();

	std::string BlueprintPath;
	bool IsRunning = true;
		
	while (IsRunning)
	{
		std::cin >> BlueprintPath;
		if (BlueprintPath == "Exit")
		{
			IsRunning = false;
			continue;
		}
			
		ParseBlueprint(BlueprintPath);
	}
	FPlatformAgnosticChecker::Exit();
	return true;
}
