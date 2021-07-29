// Here we explicitly say to linker which entry point to use
#pragma comment(linker, "/SUBSYSTEM:CONSOLE /ENTRY:mainCRTStartup")

#include "BlueprintChecker.h"

#include "FPlatformAgnosticChecker.h"
#include "RequiredProgramMainCPPInclude.h"
#include "UObject/CoreRedirects.h"
#include "Serialization/AsyncLoadingThread.h"

DEFINE_LOG_CATEGORY_STATIC(LogBlueprintChecker, Log, All);

IMPLEMENT_PRIMARY_GAME_MODULE(FDefaultModuleImpl, BlueprintChecker, BlueprintChecker)

FEngineLoop GEngineLoop;
bool GIsConsoleExecutable = true;

// Entry point is here
int main(int Argc, char* Argv[])
{
	if (Argc < 2)
	{
		std::cerr << "Not enough arguments!" << std::endl;
		exit(EXIT_FAILURE);
	}
	
	const size_t ArgStrLen = strlen(Argv[1]);
	const FString& BlueprintPathStr = FString(ArgStrLen, Argv[1]);

#if RUN_WITH_TESTS
	FPlatformAgnosticChecker::InitializeTestEnvironment(Argc, Argv);
#else
	FPlatformAgnosticChecker::Check(*BlueprintPathStr);
#endif
	return 0;
}

//TODO Get rid of two entry points in the file, but it looks like linker wants both of these functions here
int32 WINAPI WinMain( _In_ HINSTANCE hInInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ char*, _In_ int32 nCmdShow )
{
	return 0;
}
