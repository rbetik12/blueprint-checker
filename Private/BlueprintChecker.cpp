#include "BlueprintChecker.h"
#include "FPlatformAgnosticChecker.h"
#include "RequiredProgramMainCPPInclude.h"
#include "easyargs/easyargs.h"
#include "UObject/CoreRedirects.h"
#include "Serialization/AsyncLoadingThread.h"

IMPLEMENT_PRIMARY_GAME_MODULE(FDefaultModuleImpl, BlueprintChecker, BlueprintChecker)

int main(int Argc, char* Argv[])
{
	EasyArgs *EzArgs = new EasyArgs(Argc, Argv);
	EzArgs->Version(PROGRAM_VERSION)
			->Description(PROGRAM_DESCRIPTION)
			->Flag("-h", "--help", "Help")
			->Value("-m", "--mode", "Utility launch mode [Single|Batch]", true)
			->Value("-f", "--filepath", "Path to a file.\nIn single mode - blueprint path.\nIn batch mode - filenames file path", true);

	if (EzArgs->IsSet("-h") || EzArgs->IsSet("--help"))
	{
		EzArgs->PrintUsage();
		exit(EXIT_SUCCESS);
	}
	
	std::string Mode = EzArgs->GetValueFor("--mode");

	if (Mode.empty())
	{
		Mode = EzArgs->GetValueFor("-m");
		if (Mode.empty())
		{
			std::wcerr << "You must provide program run mode with -m or --mode flags!" << std::endl;
			exit(EXIT_FAILURE);
		}
	}

	std::string Filepath = EzArgs->GetValueFor("--filepath");

	if (Filepath.empty())
	{
		Filepath = EzArgs->GetValueFor("-f");
		if (Filepath.empty())
		{
			std::wcerr << "You must provide file path!" << std::endl;
			exit(EXIT_FAILURE);
		}
	}
	
	if (Mode == "Single")
	{
		const size_t ArgStrLen = strlen(Filepath.c_str());
		const FString& BlueprintPathStr = FString(ArgStrLen, Filepath.c_str());
		
		FPlatformAgnosticChecker::Init();
		const bool Result = FPlatformAgnosticChecker::Check(*BlueprintPathStr);
		if (Result)
		{
			std::wcout << "Parsed successfully!" << std::endl;
		}
		else
		{
			std::wcout << "Failed to parse!" << std::endl;
		}
		FPlatformAgnosticChecker::Exit();
	}
	else if (Mode == "Batch")
	{
		//Batch mode stuff
	}
	else
	{
		std::wcerr << "Incorrect mode!" << std::endl;
		exit(EXIT_FAILURE);
	}
	
	return 0;
}
