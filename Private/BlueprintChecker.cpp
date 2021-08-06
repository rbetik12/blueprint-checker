#include "BlueprintChecker.h"

#include "FPlatformAgnosticChecker.h"
#include "RequiredProgramMainCPPInclude.h"
#include "easyargs/easyargs.h"

FEngineLoop GEngineLoop;
bool GIsConsoleExecutable = true;
EasyArgs* EzArgs = nullptr;
std::string Mode;
std::string PathToFile;

void ExtractFilepath()
{
	PathToFile = EzArgs->GetValueFor("--filepath");

	if (PathToFile.empty())
	{
		PathToFile = EzArgs->GetValueFor("-f");
		if (PathToFile.empty() && Mode != "StdIn")
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
	      ->Value("-m", "--mode", "Utility launch mode [Single|Batch|StdIn]", false)
	      ->Value("-f", "--filepath",
	              "Path to a file.\nIn single mode - blueprint path.\nIn batch mode - filenames file path\nLaunch in daemon-like mode", false);

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

bool RunMain(int Argc, char* Argv[])
{
	InitializeEasyArgs(Argc, Argv);

	ExtractMode();
	ExtractFilepath();

	if (Mode == "Single")
	{
		return RunSingleMode(PathToFile);
	}

	if (Mode == "Batch")
	{
		return RunBatchMode(PathToFile);
	}

	if (Mode == "StdIn")
	{
		return RunStdInMode();
	}

	std::wcerr << "Incorrect mode!" << std::endl;
	return false;
}

int main(int Argc, char* Argv[])
{
	if (!RunMain(Argc, Argv))
	{
		exit(EXIT_FAILURE);
	}
	//TODO EzArgs var cleaning
	return 0;
}
