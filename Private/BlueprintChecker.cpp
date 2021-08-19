#include "BlueprintChecker.h"

#include "Util/FPlatformAgnosticChecker.h"
#include "RequiredProgramMainCPPInclude.h"
#include "easyargs/easyargs.h"

FEngineLoop GEngineLoop;
bool GIsConsoleExecutable = true;
bool GIsTestMode = false;
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
		GIsTestMode = true;
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

// Here we explicitly say to linker which entry point to use
#pragma comment(linker, "/SUBSYSTEM:CONSOLE /ENTRY:mainCRTStartup")
#include <Windows.h>

//TODO Get rid of two entry points in the file, but it looks like linker wants both of these functions here
int WINAPI WinMain( _In_ HINSTANCE HInInstance, _In_opt_ HINSTANCE HPrevInstance, _In_ char*, _In_ int HCmdShow )
{
	return 0;
}
