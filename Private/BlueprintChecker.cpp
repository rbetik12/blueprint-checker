#include "BlueprintChecker.h"

#include "FPlatformAgnosticChecker.h"
#include "RequiredProgramMainCPPInclude.h"
#include "easyargs/easyargs.h"
#include <fstream>

EasyArgs* EzArgs = nullptr;
std::string Mode;
std::string PathToFile;

void ExtractFilepath()
{
	PathToFile = EzArgs->GetValueFor("--filepath");

	if (PathToFile.empty())
	{
		PathToFile = EzArgs->GetValueFor("-f");
		if (PathToFile.empty())
		{
			std::wcerr << "You must provide file path!" << std::endl;
			exit(EXIT_FAILURE);
		}
	}
}

void ExtractMode()
{
	Mode = EzArgs->GetValueFor("--mode");

	if (Mode.empty())
	{
		Mode = EzArgs->GetValueFor("-m");
		if (Mode.empty())
		{
			std::wcerr << "You must provide program run mode with -m or --mode flags!" << std::endl;
			exit(EXIT_FAILURE);
		}
	}
}

void InitializeEasyArgs(int Argc, char* Argv[])
{
	EzArgs = new EasyArgs(Argc, Argv);
	EzArgs->Version(PROGRAM_VERSION)
	      ->Description(PROGRAM_DESCRIPTION)
	      ->Flag("-h", "--help", "Help")
	      ->Flag("-t", "--test", "Run all tests")
	      ->Value("-m", "--mode", "Utility launch mode [Single|Batch]", false)
	      ->Value("-f", "--filepath",
	              "Path to a file.\nIn single mode - blueprint path.\nIn batch mode - filenames file path", false);

	if (EzArgs->IsSet("-h") || EzArgs->IsSet("--help"))
	{
		EzArgs->PrintUsage();
		exit(EXIT_SUCCESS);
	}

	if (EzArgs->IsSet("-t") || EzArgs->IsSet("--test"))
	{
		FPlatformAgnosticChecker::InitializeTestEnvironment(Argc, Argv);
		FPlatformAgnosticChecker::Exit();
		exit(EXIT_SUCCESS);
	}
}

bool ParseBlueprint(std::string BPFilepath)
{
	const size_t ArgStrLen = strlen(BPFilepath.c_str());
	const FString& BlueprintPathStr = FString(ArgStrLen, BPFilepath.c_str());
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

bool RunMain(int Argc, char* Argv[])
{
	InitializeEasyArgs(Argc, Argv);

	ExtractMode();
	ExtractFilepath();

	if (Mode == "Single")
	{
		FPlatformAgnosticChecker::Init();
		const bool Result = ParseBlueprint(PathToFile);
		FPlatformAgnosticChecker::Exit();

		return Result;
	}

	if (Mode == "Batch")
	{
		//TODO check whether file exists or not
		std::ifstream FileStream(PathToFile);

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
		FPlatformAgnosticChecker::SerializeBlueprintInfo();
		FPlatformAgnosticChecker::Exit();

		return SuccessfulParsing;
	}

	std::wcerr << "Incorrect mode!" << std::endl;
	return false;
}

int main(int Argc, char* Argv[])
{
	RunMain(Argc, Argv);
	//TODO EzArgs var cleaning
	return 0;
}
